#pragma once
#include <navier/storage/field.hpp>
#include <navier/storage/page.hpp>
#include <filesystem>
#include <memory>
namespace navier::storage {
struct DatasetOptions {bool create=false;bool recover_tail=true;std::size_t cache_pages=64;std::uint64_t max_field_bytes=256ULL*1024*1024;std::string metadata;};
struct DatasetStatistics {std::uint64_t bytes_read=0,bytes_written=0,records_scanned=0,field_reads=0;bool index_rebuilt=false,recovered_tail=false;PageId index_pages=0,root_page=0;BufferStatistics buffer;};
// NAVIER02. Methods serialize access within this instance. Requires exclusive writer ownership across instances/processes.
class DatasetStore {
    struct Implementation;std::unique_ptr<Implementation> impl_;
public:
    explicit DatasetStore(const std::filesystem::path& path,DatasetOptions options={});
    ~DatasetStore();DatasetStore(const DatasetStore&)=delete;DatasetStore& operator=(const DatasetStore&)=delete;
    void save(std::uint64_t step,double time,std::span<const FieldView> fields,std::span<const Metric> metrics={},CheckpointKind kind=CheckpointKind::analysis);
    [[nodiscard]] CheckpointDescriptor get_checkpoint(std::uint64_t step);
    [[nodiscard]] Field load_field(std::uint64_t step,std::string_view name);
    [[nodiscard]] ScientificCheckpoint load(std::uint64_t step);
    [[nodiscard]] std::vector<CheckpointDescriptor> range_steps(std::uint64_t begin,std::uint64_t end,std::size_t limit=1000);
    [[nodiscard]] std::uint64_t size()const;
    [[nodiscard]] std::pair<std::uint64_t,std::uint64_t> step_bounds()const;
    [[nodiscard]] std::uint64_t generation()const;
    [[nodiscard]] std::string metadata()const;
    [[nodiscard]] DatasetStatistics statistics()const;
    void validate(bool full=false);
    // Explicitly discards only a detected incomplete tail, then updates the superblock.
    void repair_tail();
};
}
