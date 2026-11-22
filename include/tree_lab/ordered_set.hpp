#pragma once

#include "search_tree.hpp"

template <typename T, typename Compare = std::less<T>>
class OrderedSet {
private:
    SearchTree<T, Compare> tree_;

    explicit OrderedSet(SearchTree<T, Compare> tree) : tree_(std::move(tree)) {}

public:
    using const_iterator = typename SearchTree<T, Compare>::const_iterator;

    explicit OrderedSet(Compare compare = Compare{}) : tree_(std::move(compare)) {}
    OrderedSet(std::initializer_list<T> values, Compare compare = Compare{})
        : tree_(values, std::move(compare)) {}

    bool insert(const T& value) { return tree_.insert(value); }
    bool insert(T&& value) { return tree_.insert(std::move(value)); }
    bool erase(const T& value) { return tree_.erase(value); }
    std::optional<std::reference_wrapper<const T>> find(const T& value) const {
        return tree_.find(value);
    }
    bool contains(const T& value) const { return tree_.contains(value); }

    std::size_t size() const noexcept { return tree_.size(); }
    bool empty() const noexcept { return tree_.empty(); }
    void clear() noexcept { tree_.clear(); }
    const Compare& key_comp() const noexcept { return tree_.key_comp(); }
    void swap(OrderedSet& other) { tree_.swap(other.tree_); }

    const_iterator begin() const { return tree_.begin(); }
    const_iterator end() const { return tree_.end(); }

    auto values(TraversalOrder order) const & { return tree_.values(order); }
    auto values(TraversalOrder order) const && = delete;

    template <typename Function>
    void for_each(Function&& function,
                  TraversalOrder order = TraversalOrder::left_root_right) const {
        tree_.for_each(std::forward<Function>(function), order);
    }

    template <typename Function>
    OrderedSet map(Function&& function) const {
        return OrderedSet(tree_.map(std::forward<Function>(function)));
    }

    template <typename Predicate>
    OrderedSet where(Predicate&& predicate) const {
        return OrderedSet(tree_.where(std::forward<Predicate>(predicate)));
    }

    template <typename Function, typename Accumulator>
    Accumulator reduce(Function&& function, Accumulator accumulator) const {
        return tree_.reduce(std::forward<Function>(function), std::move(accumulator));
    }

    OrderedSet set_union(const OrderedSet& other) const {
        OrderedSet result(*this);
        other.for_each([&](const T& value) { result.insert(value); },
                       TraversalOrder::root_left_right);
        return result;
    }

    OrderedSet intersection(const OrderedSet& other) const {
        return where([&](const T& value) { return other.contains(value); });
    }

    OrderedSet difference(const OrderedSet& other) const {
        return where([&](const T& value) { return !other.contains(value); });
    }

    bool is_subset_of(const OrderedSet& other) const {
        if (size() > other.size()) {
            return false;
        }
        bool subset = true;
        for_each([&](const T& value) { subset = subset && other.contains(value); });
        return subset;
    }

    bool contains_subset(const OrderedSet& other) const { return other.is_subset_of(*this); }

    std::string to_string() const { return "SET1 " + tree_.to_string(); }

    static OrderedSet from_string(const std::string& text, Compare compare = Compare{}) {
        std::istringstream in(text);
        read_format(in, "SET1");
        SearchTree<T, Compare> tree(std::move(compare));
        if (!(in >> tree)) {
            throw std::invalid_argument("Invalid set data");
        }
        require_text_end(in);
        return OrderedSet(std::move(tree));
    }

    friend OrderedSet operator|(const OrderedSet& left, const OrderedSet& right) {
        return left.set_union(right);
    }
    friend OrderedSet operator&(const OrderedSet& left, const OrderedSet& right) {
        return left.intersection(right);
    }
    friend OrderedSet operator-(const OrderedSet& left, const OrderedSet& right) {
        return left.difference(right);
    }

    OrderedSet& operator|=(const OrderedSet& other) {
        auto result = *this | other;
        *this = std::move(result);
        return *this;
    }
    OrderedSet& operator&=(const OrderedSet& other) {
        auto result = *this & other;
        *this = std::move(result);
        return *this;
    }
    OrderedSet& operator-=(const OrderedSet& other) {
        auto result = *this - other;
        *this = std::move(result);
        return *this;
    }
    friend bool operator==(const OrderedSet& left, const OrderedSet& right) {
        return left.size() == right.size() && left.is_subset_of(right) && right.is_subset_of(left);
    }
    friend bool operator!=(const OrderedSet& left, const OrderedSet& right) { return !(left == right); }
    friend bool operator<=(const OrderedSet& left, const OrderedSet& right) { return left.is_subset_of(right); }
    friend bool operator>=(const OrderedSet& left, const OrderedSet& right) { return right <= left; }

    friend std::ostream& operator<<(std::ostream& out, const OrderedSet& set) {
        return out << "SET1 " << set.tree_;
    }
    friend std::istream& operator>>(std::istream& in, OrderedSet& set) {
        try {
            read_format(in, "SET1");
            SearchTree<T, Compare> tree(set.key_comp());
            if (in >> tree) {
                set.tree_ = std::move(tree);
            }
        } catch (const std::invalid_argument&) {
            in.setstate(std::ios::failbit);
        }
        return in;
    }
};
