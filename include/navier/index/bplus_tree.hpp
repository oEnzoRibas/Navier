#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace navier::index {

// Order is the maximum number of children of an internal node.
// Keys and values must be copyable; Compare must define a strict weak ordering.
template <typename Key, typename Value, std::size_t Order = 32,
          typename Compare = std::less<Key>>
class BPlusTree {
    static_assert(Order >= 3, "B+ Tree order must be at least three");

    struct Node {
        explicit Node(bool is_leaf) : leaf(is_leaf) {
            keys.reserve(Order);
            if (leaf) values.reserve(Order);
            else children.reserve(Order + 1);
        }
        bool leaf;
        std::vector<Key> keys;
        std::vector<Value> values;
        std::vector<std::unique_ptr<Node>> children;
        Node* next = nullptr; // Non-owning: root/children own all nodes.
    };
    struct Split {
        Key separator;
        std::unique_ptr<Node> right;
    };

public:
    explicit BPlusTree(Compare compare = Compare{})
        : root_(std::make_unique<Node>(true)), compare_(std::move(compare)) {}
    BPlusTree(const BPlusTree&) = delete;
    BPlusTree& operator=(const BPlusTree&) = delete;
    BPlusTree(BPlusTree&&) = delete;
    BPlusTree& operator=(BPlusTree&&) = delete;

    // Returns true for a new key, false when replacing an existing value.
    bool insert(const Key& key, const Value& value) {
        bool inserted = false;
        auto split = insert_into(*root_, key, value, inserted);
        if (split) {
            auto new_root = std::make_unique<Node>(false);
            new_root->keys.push_back(std::move(split->separator));
            new_root->children.push_back(std::move(root_));
            new_root->children.push_back(std::move(split->right));
            root_ = std::move(new_root);
        }
        if (inserted) ++size_;
        return inserted;
    }

    // Returned pointers are invalidated by subsequent insertions.
    [[nodiscard]] const Value* find(const Key& key) const {
        const Node* leaf = find_leaf(key);
        const auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key, compare_);
        if (it == leaf->keys.end() || !equivalent(*it, key)) return nullptr;
        return &leaf->values[static_cast<std::size_t>(it - leaf->keys.begin())];
    }
    [[nodiscard]] bool contains(const Key& key) const { return find(key) != nullptr; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    // Inclusive bounds, ordered according to Compare. Reversed bounds are empty.
    [[nodiscard]] std::vector<std::pair<Key, Value>> range(const Key& begin, const Key& end) const {
        std::vector<std::pair<Key, Value>> result;
        if (compare_(end, begin)) return result;
        const Node* leaf = find_leaf(begin);
        auto pos = static_cast<std::size_t>(
            std::lower_bound(leaf->keys.begin(), leaf->keys.end(), begin, compare_) - leaf->keys.begin());
        while (leaf) {
            for (; pos < leaf->keys.size(); ++pos) {
                if (compare_(end, leaf->keys[pos])) return result;
                result.emplace_back(leaf->keys[pos], leaf->values[pos]);
            }
            leaf = leaf->next;
            pos = 0;
        }
        return result;
    }

    // Height counts levels; the empty tree consists of one leaf.
    [[nodiscard]] std::size_t height() const noexcept {
        std::size_t levels = 1;
        const Node* node = root_.get();
        while (!node->leaf) { ++levels; node = node->children.front().get(); }
        return levels;
    }

    [[nodiscard]] bool validate() const {
        std::vector<const Node*> leaves;
        std::optional<std::size_t> leaf_depth;
        std::size_t count = 0;
        const Key* minimum = nullptr;
        const Key* maximum = nullptr;
        if (!validate_node(*root_, true, 0, leaf_depth, leaves, count, minimum, maximum)) return false;
        if (count != size_) return false;
        for (std::size_t i = 0; i < leaves.size(); ++i) {
            const Node* expected = i + 1 < leaves.size() ? leaves[i + 1] : nullptr;
            if (leaves[i]->next != expected) return false;
        }
        return true;
    }

private:
    [[nodiscard]] bool equivalent(const Key& a, const Key& b) const {
        return !compare_(a, b) && !compare_(b, a);
    }
    [[nodiscard]] std::size_t child_index(const Node& node, const Key& key) const {
        // Separators are the minimum keys of their right children: equality goes right.
        return static_cast<std::size_t>(std::upper_bound(node.keys.begin(), node.keys.end(), key, compare_)
                                        - node.keys.begin());
    }
    [[nodiscard]] const Node* find_leaf(const Key& key) const {
        const Node* node = root_.get();
        while (!node->leaf) node = node->children[child_index(*node, key)].get();
        return node;
    }

    std::optional<Split> insert_into(Node& node, const Key& key, const Value& value, bool& inserted) {
        if (node.leaf) {
            const auto it = std::lower_bound(node.keys.begin(), node.keys.end(), key, compare_);
            const auto pos = static_cast<std::size_t>(it - node.keys.begin());
            if (it != node.keys.end() && equivalent(*it, key)) {
                node.values[pos] = value;
                return std::nullopt;
            }
            node.keys.insert(it, key);
            node.values.insert(node.values.begin() + static_cast<std::ptrdiff_t>(pos), value);
            inserted = true;
            if (node.keys.size() < Order) return std::nullopt;
            auto right = std::make_unique<Node>(true);
            const auto mid = node.keys.size() / 2;
            right->keys.assign(node.keys.begin() + static_cast<std::ptrdiff_t>(mid), node.keys.end());
            right->values.assign(node.values.begin() + static_cast<std::ptrdiff_t>(mid), node.values.end());
            node.keys.erase(node.keys.begin() + static_cast<std::ptrdiff_t>(mid), node.keys.end());
            node.values.erase(node.values.begin() + static_cast<std::ptrdiff_t>(mid), node.values.end());
            right->next = node.next;
            node.next = right.get();
            Key separator = right->keys.front();
            return Split{std::move(separator), std::move(right)};
        }
        const auto pos = child_index(node, key);
        auto split = insert_into(*node.children[pos], key, value, inserted);
        if (!split) return std::nullopt;
        node.keys.insert(node.keys.begin() + static_cast<std::ptrdiff_t>(pos), std::move(split->separator));
        node.children.insert(node.children.begin() + static_cast<std::ptrdiff_t>(pos + 1), std::move(split->right));
        if (node.keys.size() < Order) return std::nullopt;
        const auto mid = node.keys.size() / 2;
        Key separator = node.keys[mid];
        auto right = std::make_unique<Node>(false);
        right->keys.assign(node.keys.begin() + static_cast<std::ptrdiff_t>(mid + 1), node.keys.end());
        for (std::size_t i = mid + 1; i < node.children.size(); ++i)
            right->children.push_back(std::move(node.children[i]));
        node.keys.erase(node.keys.begin() + static_cast<std::ptrdiff_t>(mid), node.keys.end());
        node.children.resize(mid + 1);
        return Split{std::move(separator), std::move(right)};
    }

    bool validate_node(const Node& node, bool is_root, std::size_t depth,
                       std::optional<std::size_t>& leaf_depth, std::vector<const Node*>& leaves,
                       std::size_t& count, const Key*& minimum, const Key*& maximum) const {
        if (node.keys.size() >= Order) return false;
        for (std::size_t i = 1; i < node.keys.size(); ++i)
            if (!compare_(node.keys[i - 1], node.keys[i])) return false;
        if (node.leaf) {
            if (!node.children.empty() || node.values.size() != node.keys.size()) return false;
            if (!is_root && node.keys.size() < Order / 2) return false;
            if (leaf_depth && *leaf_depth != depth) return false;
            leaf_depth = depth;
            leaves.push_back(&node);
            count += node.keys.size();
            minimum = node.keys.empty() ? nullptr : &node.keys.front();
            maximum = node.keys.empty() ? nullptr : &node.keys.back();
            return true;
        }
        if (!node.values.empty() || node.next || node.children.size() != node.keys.size() + 1) return false;
        if (node.keys.empty() || (!is_root && node.keys.size() < (Order - 1) / 2)) return false;
        const Key* previous_max = nullptr;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            const Key* child_min = nullptr;
            const Key* child_max = nullptr;
            if (!node.children[i] || !validate_node(*node.children[i], false, depth + 1,
                    leaf_depth, leaves, count, child_min, child_max)) return false;
            if (!child_min || !child_max) return false;
            if (i == 0) minimum = child_min;
            else if (!equivalent(node.keys[i - 1], *child_min) || !compare_(*previous_max, *child_min)) return false;
            previous_max = child_max;
        }
        maximum = previous_max;
        return true;
    }

    std::unique_ptr<Node> root_;
    Compare compare_;
    std::size_t size_ = 0;
};

} // namespace navier::index
