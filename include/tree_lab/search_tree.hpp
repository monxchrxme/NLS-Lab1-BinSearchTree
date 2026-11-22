#pragma once

#include "queue.hpp"
#include "stack.hpp"
#include "value_codec.hpp"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>

enum class TraversalOrder {
    root_left_right, // pre-order (прямой)
    root_right_left, // (прямой справа налево)
    left_root_right, // in-order (симметричный)
    left_right_root, // post-order (обратный)
    right_root_left, // (обратный симметричный)
    right_left_root, // (обратный справа налево)
    breadth_first    // (в ширину)
};

enum class TreeBranch { 
    root, 
    left, 
    right 
};

template <typename T, typename Compare = std::less<T>>
class SearchTree {
private:
    struct Node {
        T value;
        std::shared_ptr<Node> left{nullptr};
        std::shared_ptr<Node> right{nullptr};

        explicit Node(T item) : value(std::move(item)) {}
    };

    using NodePtr = std::shared_ptr<Node>;

    enum class Step { 
        visit, 
        left, 
        right 
    };

    static constexpr Step dfs_steps_[][3] = {
        {Step::visit, Step::left, Step::right},
        {Step::visit, Step::right, Step::left},
        {Step::left, Step::visit, Step::right},
        {Step::left, Step::right, Step::visit},
        {Step::right, Step::visit, Step::left},
        {Step::right, Step::left, Step::visit}
    };

    Compare compare_;
    NodePtr root_ = nullptr;
    std::size_t size_ = 0;

    NodePtr find_node(const T& value) const {
        auto current = root_;
        while (current) {
            if (compare_(value, current->value)) {
                current = current->left;
            } else if (compare_(current->value, value)) {
                current = current->right;
            } else {
                return current;
            }
        }
        return nullptr;
    }

    template <typename U>
    bool insert_value(U&& value) {
        auto current = root_;
        NodePtr parent;
        bool is_left = false;
        while (current) {
            parent = current;
            if (compare_(value, current->value)) {
                is_left = true;
                current = current->left;
            } else if (compare_(current->value, value)) {
                is_left = false;
                current = current->right;
            } else {
                return false;
            }
        }

        auto node = std::make_shared<Node>(std::forward<U>(value));
        if (!parent) {
            root_ = std::move(node);
        } else if (is_left) {
            parent->left = std::move(node);
        } else {
            parent->right = std::move(node);
        }
        ++size_;
        return true;
    }

    SearchTree copy_subtree(const NodePtr& source) const {
        SearchTree result(compare_);
        if (!source) return result;

        result.root_ = std::make_shared<Node>(source->value);
        result.size_ = 1;
        struct PendingCopy {
            NodePtr original;
            NodePtr copy;
        };
        Stack<PendingCopy> pending;
        pending.push({source, result.root_});
        while (!pending.empty()) {
            const auto [original, copy] = std::move(pending.top());
            pending.pop();
            if (original->right) {
                copy->right = std::make_shared<Node>(original->right->value);
                ++result.size_;
                pending.push({original->right, copy->right});
            }
            if (original->left) {
                copy->left = std::make_shared<Node>(original->left->value);
                ++result.size_;
                pending.push({original->left, copy->left});
            }
        }
        return result;
    }

    static SearchTree read_tree(std::istream& in, Compare compare) {
        read_format(in, "TREE1");
        SearchTree result(std::move(compare));
        struct Pending {
            NodePtr parent;
            bool is_left;
            NodePtr lower;
            NodePtr upper;
        };
        Stack<Pending> pending;
        pending.push({nullptr, false, nullptr, nullptr});
        while (!pending.empty()) {
            const auto [parent, is_left, lower, upper] = std::move(pending.top());
            pending.pop();
            auto value = read_value<T>(in);
            if (!value) {
                continue;
            }
            if ((lower && !result.compare_(lower->value, *value)) ||
                (upper && !result.compare_(*value, upper->value))) {
                throw std::invalid_argument("Invalid BST ordering or duplicate key");
            }
            auto node = std::make_shared<Node>(std::move(*value));
            if (!parent) {
                result.root_ = node;
            } else if (is_left) {
                parent->left = node;
            } else {
                parent->right = node;
            }
            ++result.size_;
            pending.push({node, false, node, upper});
            pending.push({node, true, lower, node});
        }
        return result;
    }

public:
    class ValueRange;

    class const_iterator {
    private:
        friend class ValueRange;

        struct Task {
            NodePtr node;
            bool visit_value;
        };

        const Step* steps_ = nullptr; // null step sequence selects BFS
        NodePtr current_;
        Stack<Task> dfs_pending_;
        Queue<NodePtr> bfs_pending_;

        const_iterator(const NodePtr& root, const Step* steps) : steps_(steps) {
            if (!root) return;
            if (steps_) {
                dfs_pending_.push({root, false});
                advance_dfs();
            } else {
                current_ = root;
            }
        }

        void advance_dfs() {
            while (!dfs_pending_.empty()) {
                auto [node, visit_value] = std::move(dfs_pending_.top());
                dfs_pending_.pop();
                if (visit_value) {
                    current_ = std::move(node);
                    return;
                }
                for (int i = 2; i >= 0; --i) {
                    if (steps_[i] == Step::visit) {
                        dfs_pending_.push({node, true});
                    } else {
                        const auto& child = steps_[i] == Step::left ? node->left : node->right;
                        if (child) dfs_pending_.push({child, false});
                    }
                }
            }
            current_.reset();
        }

    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator() = default;

        reference operator*() const { return current_->value; }
        pointer operator->() const { return std::addressof(current_->value); }

        const_iterator& operator++() {
            if (steps_) {
                advance_dfs();
            } else {
                if (current_->left) bfs_pending_.push(current_->left);
                if (current_->right) bfs_pending_.push(current_->right);
                if (bfs_pending_.empty()) {
                    current_.reset();
                } else {
                    current_ = std::move(bfs_pending_.front());
                    bfs_pending_.pop();
                }
            }
            return *this;
        }

        const_iterator operator++(int) {
            auto previous = *this;
            ++*this;
            return previous;
        }

        friend bool operator==(const const_iterator& a, const const_iterator& b) noexcept {
            return a.current_ == b.current_ && (!a.current_ || a.steps_ == b.steps_);
        }
    };

    class ValueRange {
    private:
        friend class SearchTree;
        const SearchTree& tree_;
        const Step* steps_;

        ValueRange(const SearchTree& tree, const Step* steps) : tree_(tree), steps_(steps) {}

    public:
        const_iterator begin() const { return const_iterator(tree_.root_, steps_); }
        const_iterator end() const { return {}; }
    };

    ValueRange values(TraversalOrder order) const & {
        if (order == TraversalOrder::breadth_first) return ValueRange(*this, nullptr);

        const auto index = static_cast<std::size_t>(order);
        if (index >= std::size(dfs_steps_)) {
            throw std::invalid_argument("Unknown traversal order");
        }
        return ValueRange(*this, dfs_steps_[index]);
    }

    ValueRange values(TraversalOrder order) const && = delete;

    const_iterator begin() const { return values(TraversalOrder::left_root_right).begin(); }
    const_iterator end() const { return {}; }

    explicit SearchTree(Compare compare = Compare{}) : compare_(std::move(compare)) {}

    SearchTree(std::initializer_list<T> values, Compare compare = Compare{})
        : SearchTree(std::move(compare)) {
        for (const auto& value : values) {
            insert(value);
        }
    }

    SearchTree(const SearchTree& other) : SearchTree(other.compare_) {
        auto copy = other.copy_subtree(other.root_);
        root_ = std::move(copy.root_);
        size_ = std::exchange(copy.size_, 0);
    }

    SearchTree(SearchTree&& other)
        : compare_(other.compare_), root_(std::move(other.root_)),
          size_(std::exchange(other.size_, 0)) {}

    SearchTree& operator=(const SearchTree& other) {
        if (this != &other) {
            auto copy = other.copy_subtree(other.root_);
            clear();
            compare_ = std::move(copy.compare_);
            root_ = std::move(copy.root_);
            size_ = std::exchange(copy.size_, 0);
        }
        return *this;
    }

    SearchTree& operator=(SearchTree&& other) {
        if (this != &other) {
            Compare compare(other.compare_);
            clear();
            compare_ = std::move(compare);
            root_ = std::move(other.root_);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    ~SearchTree() noexcept { clear(); }

    void swap(SearchTree& other) {
        if (this == &other) return;
        using std::swap;
        swap(compare_, other.compare_);
        root_.swap(other.root_);
        swap(size_, other.size_);
    }

    const Compare& key_comp() const noexcept { return compare_; }
    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    // Rotate left children upward, then delete childless roots
    void clear() noexcept {
        while (root_) {
            if (root_->left) {
                auto left = std::move(root_->left);
                root_->left = std::move(left->right);
                left->right = std::move(root_);
                root_ = std::move(left);
            } else {
                auto old_root = std::move(root_);
                root_ = std::move(old_root->right);
            }
        }
        size_ = 0;
    }

    bool insert(const T& value) { return insert_value(value); }
    bool insert(T&& value) { return insert_value(std::move(value)); }

    std::optional<std::reference_wrapper<const T>> find(const T& value) const {
        const auto node = find_node(value);
        // std::reference_wrapper<const T> 
        if (node) { return std::cref(node->value); }
        return std::nullopt;
    }

    bool contains(const T& value) const { 
        return find_node(value) != nullptr; 
    }

    bool erase(const T& value) {
        auto current = root_;
        NodePtr parent;
        while (current) {
            if (compare_(value, current->value)) {
                parent = current;
                current = current->left;
            } else if (compare_(current->value, value)) {
                parent = current;
                current = current->right;
            } else {
                break;
            }
        }
        if (!current) {
            return false;
        }
        NodePtr replacement;
        // Node has no more than 1 child 
        if (!current->left || !current->right) {
            replacement = current->left ? std::move(current->left) : std::move(current->right);
        } 
        // Node has 2 children 
        else {
            auto successor_parent = current;
            auto successor = current->right;
            while (successor->left) {
                successor_parent = successor;
                successor = successor->left;
            }
            if (successor_parent != current) {
                successor_parent->left = std::move(successor->right);
                successor->right = std::move(current->right);
            } else {
                current->right.reset();
            }
            successor->left = std::move(current->left);
            replacement = std::move(successor);
        }
        if (!parent) {
            root_ = std::move(replacement);
        } else if (parent->left == current) {
            parent->left = std::move(replacement);
        } else {
            parent->right = std::move(replacement);
        }
        --size_;
        return true;
    }

    template <typename Function>
    void for_each(Function&& function,
                  TraversalOrder order = TraversalOrder::left_root_right) const {
        for (const T& value : values(order)) {
            std::invoke(function, value);
        }
    }

    // Read-only preorder: value, depth (root = 0), branch from the parent
    // As with for_each, the callback must not mutate this tree through an alias
    template <typename Function>
    void for_each_node(Function&& function) const {
        struct Pending {
            NodePtr node;
            std::size_t depth;
            TreeBranch branch;
        };
        Stack<Pending> pending;
        if (root_) { pending.push({root_, 0, TreeBranch::root}); }
        while (!pending.empty()) {
            const auto [node, depth, branch] = std::move(pending.top());
            pending.pop();
            std::invoke(function, std::as_const(node->value), depth, branch);
            if (node->right) {
                pending.push({node->right, depth + 1, TreeBranch::right});
            }
            if (node->left) {
                pending.push({node->left, depth + 1, TreeBranch::left});
            }
        }
    }

    template <typename Function>
    SearchTree map(Function&& function) const {
        SearchTree result(compare_);
        for_each([&](const T& value) { result.insert(std::invoke(function, value)); },
                 TraversalOrder::root_left_right);
        return result;
    }

    template <typename Predicate>
    SearchTree where(Predicate&& predicate) const {
        SearchTree result(compare_);
        for_each([&](const T& value) {
            if (std::invoke(predicate, value)) {
                result.insert(value);
            }
        }, TraversalOrder::root_left_right);
        return result;
    }

    template <typename Function, typename Accumulator>
    Accumulator reduce(Function&& function, Accumulator accumulator) const {
        for_each([&](const T& value) {
            accumulator = std::invoke(function, value, std::move(accumulator));
        });
        return accumulator;
    }

    SearchTree subtree(const T& value) const {
        const auto node = find_node(value);
        if (!node) {
            throw std::out_of_range("Subtree root not found");
        }
        return copy_subtree(node);
    }

    bool contains_subtree(const SearchTree& other) const {
        if (other.empty()) {
            return true;
        }
        struct PendingComparison {
            NodePtr candidate;
            NodePtr pattern;
        };
        Stack<PendingComparison> pending;
        pending.push({find_node(other.root_->value), other.root_});
        while (!pending.empty()) {
            const auto [candidate, pattern] = std::move(pending.top());
            pending.pop();
            if (!candidate || !pattern) {
                if (candidate != pattern) {
                    return false;
                }
                continue;
            }
            if (!(candidate->value == pattern->value)) {
                return false;
            }
            pending.push({candidate->left, pattern->left});
            pending.push({candidate->right, pattern->right});
        }
        return true;
    }

    std::string to_string() const {
        std::ostringstream out;
        out << *this;
        return out.str();
    }

    static SearchTree from_string(const std::string& text, Compare compare = Compare{}) {
        std::istringstream in(text);
        auto result = read_tree(in, std::move(compare));
        require_text_end(in);
        return result;
    }

    friend std::ostream& operator<<(std::ostream& out, const SearchTree& tree) {
        out << "TREE1";
        Stack<NodePtr> pending;
        pending.push(tree.root_);
        while (!pending.empty()) {
            const auto node = std::move(pending.top());
            pending.pop();
            out << ' ';
            if (!node) {
                out << '#';
            } else {
                write_value(out, node->value);
                pending.push(node->right);
                pending.push(node->left);
            }
        }
        return out;
    }

    friend std::istream& operator>>(std::istream& in, SearchTree& tree) {
        try {
            auto replacement = read_tree(in, tree.compare_);
            tree = std::move(replacement);
        } catch (const std::invalid_argument&) {
            in.setstate(std::ios::failbit);
        }
        return in;
    }
};
