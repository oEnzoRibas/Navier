#include <navier/storage/dataset_store.hpp>
#include "field_region.hpp"
#include <navier/index/persistent_bplus_tree.hpp>
#include <navier/format/file.hpp>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <random>
#include <unordered_set>
namespace navier::storage
{
    namespace
    {
        constexpr std::uint64_t super_size = 4096, slot_stride = 256;
        constexpr std::size_t slot_size = 128, record_header = 64, footer_size = 16, max_directory = 1024 * 1024;
        struct Super
        {
            std::uint64_t generation = 0, end = 4096, begin = 4096, metadata_size = 0;
            std::uint32_t metadata_crc = 0;
            std::array<std::uint64_t, 2> identity{};
            PageId root = 0;
            std::uint64_t count = 0, first = 0, last = 0;
        };
        struct StoredField
        {
            FieldDescriptor descriptor;
            std::uint64_t offset = 0, bytes = 0;
            std::uint32_t crc = 0;
        };
        struct Record
        {
            CheckpointDescriptor descriptor;
            std::vector<StoredField> fields;
            std::uint64_t offset = 0, size = 0;
        };
        format::Bytes super_bytes(const Super &h)
        {
            format::Bytes b(slot_size);
            format::magic(b, 0, "NAVIER02");
            format::put(b, 8, 2, 4);
            format::put(b, 12, 0x01020304U, 4);
            format::put(b, 16, page_size, 4);
            format::put(b, 24, h.generation);
            format::put(b, 32, h.end);
            format::put(b, 40, h.begin);
            format::put(b, 48, super_size);
            format::put(b, 56, h.metadata_size);
            format::put(b, 64, h.metadata_crc, 4);
            format::put(b, 72, h.identity[0]);
            format::put(b, 80, h.identity[1]);
            format::put(b, 88, h.root);
            format::put(b, 96, h.count);
            format::put(b, 104, h.first);
            format::put(b, 112, h.last);
            format::put(b, 124, format::checksum(std::span(b).first(124)), 4);
            return b;
        }
        std::optional<Super> parse_super(std::span<const std::byte> b)
        {
            if (!format::matches(b, 0, "NAVIER02") || format::get(b, 8, 4) != 2 || format::get(b, 12, 4) != 0x01020304U || format::get(b, 16, 4) != page_size || format::get(b, 20, 4) != 0 || format::get(b, 124, 4) != format::checksum(b.first(124)))
                return std::nullopt;
            Super h;
            h.generation = format::get(b, 24);
            h.end = format::get(b, 32);
            h.begin = format::get(b, 40);
            h.metadata_size = format::get(b, 56);
            h.metadata_crc = static_cast<std::uint32_t>(format::get(b, 64, 4));
            h.identity = {format::get(b, 72), format::get(b, 80)};
            h.root = format::get(b, 88);
            h.count = format::get(b, 96);
            h.first = format::get(b, 104);
            h.last = format::get(b, 112);
            if (format::get(b, 48) != super_size || h.metadata_size > max_directory || h.begin != (super_size + h.metadata_size + 7) / 8 * 8 || h.end < h.begin || h.count > h.generation || (h.count && h.first > h.last))
                return std::nullopt;
            return h;
        }
        template <class T, class Sink>
        void blocks(std::span<const T> values, const Sink &sink)
        {
            static_assert((sizeof(T) == 4 || sizeof(T) == 8) && std::numeric_limits<T>::is_iec559);
            using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
            std::array<std::byte, 65536> buffer{};
            for (std::size_t base = 0; base < values.size();)
            {
                const auto count = std::min(values.size() - base, buffer.size() / sizeof(T));
                for (std::size_t i = 0; i < count; ++i)
                    format::put(buffer, i * sizeof(T), std::bit_cast<Bits>(values[base + i]), sizeof(T));
                sink(std::span<const std::byte>(buffer).first(count * sizeof(T)));
                base += count;
            }
        }
        format::Bytes directory_bytes(const std::vector<StoredField> &fields, std::span<const Metric> metrics)
        {
            format::Encoder e;
            for (const auto &f : fields)
            {
                e.text(f.descriptor.name);
                e.text(f.descriptor.unit);
                e.number(static_cast<std::uint8_t>(f.descriptor.scalar_type), 1);
                e.number(f.descriptor.dimensions.size(), 1);
                e.number(0, 2);
                for (const auto d : f.descriptor.dimensions)
                    e.number(d);
                e.number(f.offset);
                e.number(f.bytes);
                e.number(f.bytes);
                e.number(f.crc, 4);
            }
            for (const auto &m : metrics)
            {
                e.text(m.name);
                e.text(m.unit);
                e.number(std::bit_cast<std::uint64_t>(m.value));
            }
            if (e.bytes.size() > max_directory)
                throw std::invalid_argument("Checkpoint directory exceeds 1 MiB");
            return e.bytes;
        }
    }
    struct DatasetStore::Implementation
    {
        std::filesystem::path path, index_path;
        DatasetOptions options;
        std::unique_ptr<format::File> file;
        std::unique_ptr<PageManager> pages;
        std::unique_ptr<BufferPool> pool;
        std::unique_ptr<index::PersistentBPlusTree> tree;
        mutable std::mutex mutex;
        Super h;
        std::string metadata;
        DatasetStatistics stats;
        bool poisoned = false;
        void usable() const
        {
            if (poisoned)
                throw std::runtime_error("Failed append: discard store and reopen");
        }
        void persist_super()
        {
            file->write((h.generation % 2) * slot_stride, super_bytes(h));
            file->flush();
        }
        Record record(std::uint64_t at)
        {
            const auto head = file->read(at, record_header);
            if (!format::matches(head, 0, "NVCPT002") || format::get(head, 60, 4) != format::checksum(std::span(head).first(60)))
                throw std::runtime_error("Checkpoint header checksum/magic mismatch");
            Record r;
            r.offset = at;
            r.size = format::get(head, 8);
            const auto fc = format::get(head, 40, 4), mc = format::get(head, 44, 4), dir_size = format::get(head, 48, 4), kind = format::get(head, 52, 4);
            if (fc > 1024 || mc > 1024 || !fc || dir_size > max_directory || kind > 1 || r.size < record_header + footer_size || dir_size > r.size - record_header - footer_size || r.size > file->size() - at)
                throw std::runtime_error("Invalid checkpoint length/directory");
            r.descriptor = {format::get(head, 16), std::bit_cast<double>(format::get(head, 24)), format::get(head, 32), static_cast<CheckpointKind>(kind), {}, {}};
            if (!std::isfinite(r.descriptor.time) || !r.descriptor.generation)
                throw std::runtime_error("Invalid checkpoint time/generation");
            const auto bytes = file->read(at + record_header, format::memory_size(dir_size));
            if (format::get(head, 56, 4) != format::checksum(bytes))
                throw std::runtime_error("Checkpoint directory checksum mismatch");
            format::Decoder d(bytes);
            std::unordered_set<std::string> names;
            std::uint64_t expected = record_header + dir_size;
            for (std::uint64_t i = 0; i < fc; ++i)
            {
                StoredField f;
                f.descriptor.name = d.text();
                f.descriptor.unit = d.text();
                f.descriptor.scalar_type = static_cast<ScalarType>(d.number(1));
                const auto rank = d.number(1), codec = d.number(2);
                if (rank < 1 || rank > 8 || codec != 0)
                    throw std::runtime_error("Unsupported rank/codec");
                for (std::uint64_t axis = 0; axis < rank; ++axis)
                    f.descriptor.dimensions.push_back(d.number());
                f.offset = d.number();
                f.bytes = d.number();
                const auto raw = d.number();
                f.crc = static_cast<std::uint32_t>(d.number(4));
                if (!names.insert(f.descriptor.name).second || f.offset != expected || f.bytes != raw || f.bytes != f.descriptor.byte_size())
                    throw std::runtime_error("Invalid field directory/shape/offset");
                expected = format::checked_add(expected, f.bytes);
                if (expected > r.size - footer_size)
                    throw std::runtime_error("Field payload outside record");
                r.descriptor.fields.push_back(f.descriptor);
                r.fields.push_back(std::move(f));
            }
            names.clear();
            for (std::uint64_t i = 0; i < mc; ++i)
            {
                Metric m{d.text(), 0, d.text()};
                m.value = std::bit_cast<double>(d.number());
                if (m.name.empty() || !std::isfinite(m.value) || !names.insert(m.name).second)
                    throw std::runtime_error("Invalid metric metadata");
                r.descriptor.metrics.push_back(std::move(m));
            }
            if (!d.done() || expected != r.size - footer_size)
                throw std::runtime_error("Directory/payload length mismatch");
            const auto footer = file->read(format::checked_add(at, r.size - footer_size), footer_size);
            if (!format::matches(footer, 0, "NVCOMMIT") || format::get(footer, 8) != r.size)
                throw std::runtime_error("Checkpoint completion marker mismatch");
            return r;
        }
        void connect_index(const std::filesystem::path &target, bool initialize)
        {
            pages = std::make_unique<PageManager>(target, initialize);
            pool = std::make_unique<BufferPool>(*pages, options.cache_pages);
            tree = std::make_unique<index::PersistentBPlusTree>(*pool, initialize, h.identity);
        }
        void release_index()
        {
            tree.reset();
            pool.reset();
            pages.reset();
        }
        void rebuild()
        {
            auto temp = index_path;
            temp += ".rebuild";
            release_index();
            connect_index(temp, true);
            std::uint64_t at = h.begin, generation = 0;
            const auto length = file->size();
            stats.index_rebuilt = true;
            while (at < length)
            {
                if (length - at < record_header)
                {
                    stats.recovered_tail = true;
                    break;
                }
                const auto head = file->read(at, record_header);
                const bool valid_header = format::matches(head, 0, "NVCPT002") && format::get(head, 60, 4) == format::checksum(std::span(head).first(60));
                if (!valid_header)
                {
                    if (at >= h.end)
                    {
                        stats.recovered_tail = true;
                        break;
                    }
                    throw std::runtime_error("Corrupt committed checkpoint header");
                }
                const auto size = format::get(head, 8);
                if (size > length - at)
                {
                    stats.recovered_tail = true;
                    break;
                }
                const auto r = record(at);
                if (r.descriptor.generation != generation + 1)
                    throw std::runtime_error("Nonsequential checkpoint generations");
                tree->insert(r.descriptor.step, {at, r.size});
                if (!generation)
                    h.first = h.last = r.descriptor.step;
                else
                {
                    h.first = std::min(h.first, r.descriptor.step);
                    h.last = std::max(h.last, r.descriptor.step);
                }
                generation = r.descriptor.generation;
                at += r.size;
                ++stats.records_scanned;
            }
            if (stats.recovered_tail && !options.recover_tail)
                throw std::runtime_error("Incomplete dataset tail (recovery disabled)");
            tree->commit(generation);
            h.generation = generation;
            h.end = at;
            h.count = tree->size();
            h.root = tree->root_page();
            release_index();
            std::filesystem::rename(temp, index_path);
            connect_index(index_path, false);
        }
        explicit Implementation(const std::filesystem::path &input, DatasetOptions opt) : path(input), index_path(input), options(std::move(opt))
        {
            index_path += ".nvi";
            if (options.create)
            {
                if (std::filesystem::exists(path))
                    throw std::runtime_error("Dataset already exists; creation never replaces it");
                if (options.metadata.size() > max_directory)
                    throw std::invalid_argument("Dataset metadata exceeds 1 MiB");
                std::ofstream out(path, std::ios::binary);
                std::array<char, super_size> zero{};
                out.write(zero.data(), super_size);
                out.close();
                if (!out)
                    throw std::runtime_error("Cannot create dataset");
            }
            file = std::make_unique<format::File>(path);
            if (options.create)
            {
                h.metadata_size = options.metadata.size();
                metadata = options.metadata;
                const auto raw = std::as_bytes(std::span(metadata));
                h.metadata_crc = format::checksum(raw);
                h.begin = (super_size + h.metadata_size + 7) / 8 * 8;
                h.end = h.begin;
                std::random_device random;
                h.identity = {std::uint64_t(random()) << 32 | random(), std::uint64_t(random()) << 32 | random()};
                file->write(super_size, raw);
                const auto metadata_end = super_size + h.metadata_size;
                if (h.end > metadata_end)
                {
                    std::array<std::byte, 8> padding{};
                    file->write(metadata_end, std::span(padding).first(format::memory_size(h.end - metadata_end)));
                }
                connect_index(index_path, true);
                h.root = tree->root_page();
                file->write(0, super_bytes(h));
                file->write(slot_stride, super_bytes(h));
                file->flush();
                return;
            }
            if (file->size() < super_size)
                throw std::runtime_error("Truncated superblock; NAVIER01 files use legacy CheckpointStore");
            const auto a = parse_super(file->read(0, slot_size)), b = parse_super(file->read(slot_stride, slot_size));
            if (!a && !b)
                throw std::runtime_error("Unsupported/corrupt NAVIER02 superblock (legacy NAVIER01 is not reinterpreted)");
            h = (!b || (a && a->generation >= b->generation)) ? *a : *b;
            if (h.begin > file->size())
                throw std::runtime_error("Truncated dataset metadata");
            const auto raw = file->read(super_size, format::memory_size(h.metadata_size));
            if (format::checksum(raw) != h.metadata_crc)
                throw std::runtime_error("Dataset metadata checksum mismatch");
            metadata.assign(reinterpret_cast<const char *>(raw.data()), raw.size());
            bool rebuild_needed = file->size() != h.end;
            if (!rebuild_needed)
            {
                try
                {
                    connect_index(index_path, false);
                    if (tree->generation() != h.generation || tree->size() != h.count || tree->root_page() != h.root)
                        rebuild_needed = true;
                    else
                        (void)tree->find(std::numeric_limits<std::uint64_t>::max());
                }
                catch (const std::runtime_error &)
                {
                    rebuild_needed = true;
                }
            }
            if (rebuild_needed)
                rebuild();
        }
        Field field(const Record &r, const StoredField &selected)
        {
            if (selected.bytes > options.max_field_bytes)
                throw std::length_error("Field exceeds configured memory budget");
            const auto count = format::memory_size(selected.descriptor.count());
            Field result;
            result.descriptor = selected.descriptor;
            format::CRC32 crc;
            const auto decode = [&]<class T>(std::vector<T> &values)
            {using Bits=std::conditional_t<sizeof(T)==4,std::uint32_t,std::uint64_t>;values.resize(count);std::array<std::byte,65536> buffer{};std::uint64_t at=format::checked_add(r.offset,selected.offset);for(std::size_t base=0;base<count;){const auto n=std::min(count-base,buffer.size()/sizeof(T));auto chunk=std::span(buffer).first(n*sizeof(T));file->read_into(at,chunk);crc.update(chunk);for(std::size_t i=0;i<n;++i)values[base+i]=std::bit_cast<T>(static_cast<Bits>(format::get(chunk,i*sizeof(T),sizeof(T))));at+=chunk.size();base+=n;} };
            if (selected.descriptor.scalar_type == ScalarType::float32)
            {
                std::vector<float> values;
                decode(values);
                result.values = std::move(values);
            }
            else
            {
                std::vector<double> values;
                decode(values);
                result.values = std::move(values);
            }
            if (crc.value() != selected.crc)
                throw std::runtime_error("Field payload checksum mismatch: " + selected.descriptor.name);
            ++stats.field_reads;
            return result;
        }
        Record locate(std::uint64_t step)
        {
            usable();
            const auto location = tree->find(step);
            if (!location)
                throw std::out_of_range("Checkpoint not found");
            if (location->offset < h.begin || location->offset >= h.end || location->size > h.end - location->offset)
                throw std::runtime_error("Index reference outside committed region");
            auto r = record(location->offset);
            if (r.descriptor.step != step || r.size != location->size)
                throw std::runtime_error("Index/checkpoint reference mismatch");
            return r;
        }
    };
    DatasetStore::DatasetStore(const std::filesystem::path &path, DatasetOptions options) : impl_(std::make_unique<Implementation>(path, std::move(options))) {}
    DatasetStore::~DatasetStore() = default;
    void DatasetStore::save(std::uint64_t step, double time, std::span<const FieldView> fields, std::span<const Metric> metrics, CheckpointKind kind)
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        s.usable();
        if (s.stats.recovered_tail)
            throw std::runtime_error("Incomplete tail retained: explicitly repair_tail() before appending");
        if (!std::isfinite(time) || fields.empty() || fields.size() > 1024 || metrics.size() > 1024 || (kind != CheckpointKind::analysis && kind != CheckpointKind::restart))
            throw std::invalid_argument("Invalid checkpoint fields/time/kind");
        std::unordered_set<std::string> names;
        std::vector<StoredField> stored;
        for (const auto &input : fields)
        {
            StoredField f;
            f.descriptor = input.descriptor;
            f.bytes = f.descriptor.byte_size();
            if (!names.insert(f.descriptor.name).second)
                throw std::invalid_argument("Duplicate field name");
            format::CRC32 crc;
            std::visit([&](auto values)
                       {using T=typename decltype(values)::value_type;if((std::is_same_v<T,float>?ScalarType::float32:ScalarType::float64)!=f.descriptor.scalar_type||values.size()!=f.descriptor.count())throw std::invalid_argument("Field type/shape does not match input");blocks(values,[&](auto bytes){crc.update(bytes);}); }, input.values);
            f.crc = crc.value();
            stored.push_back(std::move(f));
        }
        names.clear();
        for (const auto &m : metrics)
            if (m.name.empty() || m.name.size() > 255 || m.unit.size() > 255 || !std::isfinite(m.value) || !names.insert(m.name).second)
                throw std::invalid_argument("Invalid/duplicate metric");
        auto dir = directory_bytes(stored, metrics);
        std::uint64_t relative = record_header + dir.size();
        for (auto &f : stored)
        {
            f.offset = relative;
            relative = format::checked_add(relative, f.bytes);
        }
        dir = directory_bytes(stored, metrics);
        const auto length = format::checked_add(relative, footer_size), generation = format::checked_add(s.h.generation, 1);
        (void)format::checked_add(s.h.end, length);
        format::Bytes head(record_header);
        format::magic(head, 0, "NVCPT002");
        format::put(head, 8, length);
        format::put(head, 16, step);
        format::put(head, 24, std::bit_cast<std::uint64_t>(time));
        format::put(head, 32, generation);
        format::put(head, 40, fields.size(), 4);
        format::put(head, 44, metrics.size(), 4);
        format::put(head, 48, dir.size(), 4);
        format::put(head, 52, static_cast<std::uint32_t>(kind), 4);
        format::put(head, 56, format::checksum(dir), 4);
        format::put(head, 60, format::checksum(std::span(head).first(60)), 4);
        try
        {
            const auto at = s.h.end;
            s.file->write(at, head);
            s.file->write(at + record_header, dir);
            for (std::size_t i = 0; i < fields.size(); ++i)
            {
                auto offset = format::checked_add(at, stored[i].offset);
                std::visit([&](auto values)
                           { blocks(values, [&](auto bytes)
                                    {s.file->write(offset,bytes);offset+=bytes.size(); }); }, fields[i].values);
            }
            std::array<std::byte, footer_size> footer{};
            format::magic(footer, 0, "NVCOMMIT");
            format::put(footer, 8, length);
            s.file->write(at + length - footer_size, footer);
            s.file->flush();
            s.tree->insert(step, {at, length});
            s.tree->commit(generation);
            s.h.generation = generation;
            s.h.end += length;
            if (!s.h.count)
                s.h.first = s.h.last = step;
            else
            {
                s.h.first = std::min(s.h.first, step);
                s.h.last = std::max(s.h.last, step);
            }
            s.h.count = s.tree->size();
            s.h.root = s.tree->root_page();
            s.persist_super();
        }
        catch (...)
        {
            s.poisoned = true;
            throw;
        }
    }
    CheckpointDescriptor DatasetStore::get_checkpoint(std::uint64_t step)
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        return s.locate(step).descriptor;
    }
    Field DatasetStore::load_field(std::uint64_t step, std::string_view name)
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        const auto r = s.locate(step);
        const auto found = std::find_if(r.fields.begin(), r.fields.end(), [&](const auto &f)
                                        { return f.descriptor.name == name; });
        if (found == r.fields.end())
            throw std::out_of_range("Field not found");
        return s.field(r, *found);
    }
    ScientificCheckpoint DatasetStore::load(std::uint64_t step)
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        const auto r = s.locate(step);
        std::uint64_t total = 0;
        for (const auto &f : r.fields)
            total = format::checked_add(total, f.bytes);
        if (total > s.options.max_field_bytes)
            throw std::length_error("Full checkpoint exceeds memory budget; use load_field");
        ScientificCheckpoint result{r.descriptor, {}};
        for (const auto &f : r.fields)
            result.fields.push_back(s.field(r, f));
        return result;
    }
    std::vector<CheckpointDescriptor> DatasetStore::range_steps(std::uint64_t begin, std::uint64_t end, std::size_t limit)
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        s.usable();
        std::vector<CheckpointDescriptor> result;
        std::size_t descriptor_bytes = 0;
        if (limit > 100000)
            throw std::invalid_argument("Range page exceeds 100000 descriptors");
        if (!limit)
            return result;
        s.tree->range(begin, end, [&](auto step, auto location)
                      {(void)location;auto cp=s.locate(step).descriptor;std::size_t bytes=sizeof(cp);for(const auto& f:cp.fields)bytes+=sizeof(f)+f.name.size()+f.unit.size()+f.dimensions.size()*8;for(const auto& m:cp.metrics)bytes+=sizeof(m)+m.name.size()+m.unit.size();if(bytes>16*1024*1024-descriptor_bytes)throw std::length_error("Descriptor page budget exceeded; request fewer checkpoints");descriptor_bytes+=bytes;result.push_back(std::move(cp));return result.size()<limit; });
        return result;
    }
    std::uint64_t DatasetStore::size() const
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        s.usable();
        return s.tree->size();
    }
    std::pair<std::uint64_t, std::uint64_t> DatasetStore::step_bounds() const
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        s.usable();
        return {s.h.first, s.h.last};
    }
    std::uint64_t DatasetStore::generation() const
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        s.usable();
        return s.h.generation;
    }
    std::string DatasetStore::metadata() const
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        return s.metadata;
    }
    DatasetStatistics DatasetStore::statistics() const
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        auto result = s.stats;
        result.bytes_read = s.file->bytes_read;
        result.bytes_written = s.file->bytes_written;
        result.index_pages = s.pages->page_count();
        result.root_page = s.tree->root_page();
        result.buffer = s.pool->statistics();
        return result;
    }
    void DatasetStore::validate(bool full)
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        s.usable();
        if (!s.tree->validate())
            throw std::runtime_error("Persistent B+ Tree invariant failure");
        std::optional<std::uint64_t> first;
        std::uint64_t last = 0;
        s.tree->range(0, std::numeric_limits<std::uint64_t>::max(), [&](auto step, auto location)
                      {(void)location;(void)s.locate(step);if(!first)first=step;last=step;return true; });
        if (first && (*first != s.h.first || last != s.h.last))
            throw std::runtime_error("Superblock/index step bounds mismatch");
        if (!full)
            return;
        std::array<std::byte, 65536> buffer{};
        std::uint64_t at = s.h.begin, generation = 0;
        while (at < s.h.end)
        {
            const auto record = s.record(at);
            if (record.descriptor.generation != generation + 1)
                throw std::runtime_error("Invalid historical checkpoint generation");
            for (const auto &field : record.fields)
            {
                format::CRC32 crc;
                std::uint64_t remaining = field.bytes, offset = format::checked_add(at, field.offset);
                while (remaining)
                {
                    const auto count = format::memory_size(std::min<std::uint64_t>(remaining, buffer.size()));
                    auto block = std::span(buffer).first(count);
                    s.file->read_into(offset, block);
                    crc.update(block);
                    offset += count;
                    remaining -= count;
                }
                if (crc.value() != field.crc)
                    throw std::runtime_error("Historical field checksum mismatch: " + field.descriptor.name);
            }
            at += record.size;
            generation = record.descriptor.generation;
        }
        if (at != s.h.end || generation != s.h.generation)
            throw std::runtime_error("Superblock/history length mismatch");
    }

    void DatasetStore::repair_tail()
    {
        auto &s = *impl_;
        std::lock_guard lock(s.mutex);
        s.usable();
        if (s.stats.recovered_tail)
            s.file->truncate(s.h.end);
        s.persist_super();
        s.stats.recovered_tail = false;
    }

    Field DatasetStore::load_region(std::uint64_t step, std::string_view name, const FieldRegion& region)
    {
        auto& s = *impl_;
        std::lock_guard lock(s.mutex);
        const auto record = s.locate(step);
        const auto found = std::find_if(record.fields.begin(), record.fields.end(),
                                       [&](const auto& field) { return field.descriptor.name == name; });
        if (found == record.fields.end()) throw std::out_of_range("Field not found");
        auto directory = s.path;
        directory += ".nvb";
        return detail::read_region(*s.file, found->descriptor, region,
                                   format::checked_add(record.offset, found->offset), found->crc,
                                   s.options.max_field_bytes, directory, s.h.identity, record.descriptor.generation);
    }
} // namespace navier::storage
