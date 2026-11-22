#include <tree_lab/ordered_set.hpp>
#include "../examples/student.hpp"

#include <gtest/gtest.h>
#include <complex>
#include <random>
#include <set>
#include <tuple>
#include <type_traits>
#include <vector>

TEST(Set, BasicOperationsAndValueSemantics) {
    OrderedSet<int> set;
    EXPECT_TRUE(set.empty());
    EXPECT_FALSE(set.find(4));
    EXPECT_TRUE(set.insert(4));
    EXPECT_FALSE(set.insert(4));
    ASSERT_TRUE(set.find(4));
    EXPECT_EQ(set.find(4).value().get(), 4);
    auto copy = set;
    EXPECT_NE(&copy.find(4).value().get(), &set.find(4).value().get());
    EXPECT_TRUE(copy.erase(4));
    EXPECT_FALSE(copy.erase(4));
    EXPECT_TRUE(set.contains(4));
    copy = set;
    auto moved = std::move(copy);
    EXPECT_TRUE(copy.empty());
    copy = std::move(moved);
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(copy, set);
    copy.clear();
    EXPECT_EQ(copy.size(), 0U);
}

TEST(Set, IteratorsUseTreeTraversalOrders) {
    const OrderedSet<int, std::greater<int>> set{4, 2, 6, 1, 3, 5, 7, 4};
    std::vector<int> visited;
    for (const auto& value : set) {
        static_assert(std::is_same_v<decltype(value), const int&>);
        EXPECT_EQ(&value, &set.find(value).value().get());
        visited.push_back(value);
    }
    EXPECT_EQ(visited, (std::vector{7, 6, 5, 4, 3, 2, 1}));

    const auto preorder = set.values(TraversalOrder::root_left_right);
    EXPECT_EQ(std::vector<int>(preorder.begin(), preorder.end()),
              (std::vector{4, 6, 7, 5, 2, 3, 1}));
    const auto breadth_first = set.values(TraversalOrder::breadth_first);
    EXPECT_EQ(std::vector<int>(breadth_first.begin(), breadth_first.end()),
              (std::vector{4, 6, 2, 7, 5, 3, 1}));

    const OrderedSet<int> empty;
    EXPECT_EQ(empty.begin(), empty.end());
}

TEST(Set, AlgebraAndCompoundOperators) {
    OrderedSet<int> left{3, 1, 2}, right{4, 2, 3};
    EXPECT_EQ(left | right, (OrderedSet<int>{1, 2, 3, 4}));
    EXPECT_EQ(left & right, (OrderedSet<int>{2, 3}));
    EXPECT_EQ(left - right, (OrderedSet<int>{1}));
    EXPECT_EQ(right - left, (OrderedSet<int>{4}));
    EXPECT_EQ(left.set_union(right), left | right);
    EXPECT_EQ(left.intersection(right), left & right);
    EXPECT_EQ(left.difference(right), left - right);
    auto copy = left;
    EXPECT_EQ(&(copy |= right), &copy);
    EXPECT_EQ(copy, left | right);
    copy = left;
    EXPECT_EQ(&(copy &= right), &copy);
    EXPECT_EQ(copy, left & right);
    copy = left;
    EXPECT_EQ(&(copy -= right), &copy);
    EXPECT_EQ(copy, left - right);
    left |= left;
    left &= left;
    EXPECT_EQ(left.size(), 3U);
    left -= left;
    EXPECT_TRUE(left.empty());
}

TEST(Set, InclusionEqualityAndEmptySets) {
    const OrderedSet<int> a{2, 1, 3}, b{1, 3, 2}, small{1, 3}, empty;
    EXPECT_EQ(a, b);
    EXPECT_NE(a.to_string(), b.to_string());
    EXPECT_NE(a, small);
    EXPECT_TRUE(small <= a);
    EXPECT_TRUE(a >= small);
    EXPECT_TRUE(a <= a);
    EXPECT_TRUE(a.contains_subset(small));
    EXPECT_TRUE(small.is_subset_of(a));
    EXPECT_FALSE(a <= small);
    EXPECT_TRUE(empty <= a);
    EXPECT_FALSE(a <= empty);
    EXPECT_EQ(a | empty, a);
    EXPECT_EQ(a & empty, empty);
    EXPECT_EQ(a - empty, a);
    EXPECT_EQ(empty - a, empty);
    EXPECT_EQ(empty, OrderedSet<int>{});
}

TEST(Set, MapWhereReduce) {
    const OrderedSet<int> set{2, 1, 3, 4};
    EXPECT_EQ(set.map([](int x) { return x % 2; }), (OrderedSet<int>{0, 1}));
    auto filtered = set.where([](int x) { return x > 2; });
    EXPECT_EQ(filtered, (OrderedSet<int>{3, 4}));
    EXPECT_NE(&filtered.find(3).value().get(), &set.find(3).value().get());
    EXPECT_EQ(set.reduce([](int value, std::string acc) { return acc + std::to_string(value); }, std::string{}), "1234");
    EXPECT_EQ(OrderedSet<int>{}.reduce([](int x, int acc) { return x + acc; }, 7), 7);
}

TEST(Set, SerializationAndStreamsPreserveStructure) {
    const OrderedSet<int> set{2, 1, 3};
    EXPECT_EQ(set.to_string(), "SET1 TREE1 \"2\" \"1\" # # \"3\" # #");
    auto copy = OrderedSet<int>::from_string(set.to_string());
    EXPECT_EQ(copy.to_string(), set.to_string());
    std::stringstream stream;
    stream << set << ' ' << OrderedSet<int>{};
    OrderedSet<int> first, second{1};
    stream >> first >> second;
    EXPECT_FALSE(stream.fail());
    EXPECT_EQ(first, set);
    EXPECT_TRUE(second.empty());
    EXPECT_TRUE(OrderedSet<int>::from_string("SET1 TREE1 #").empty());
}

TEST(Set, InvalidInputDoesNotChangeExistingSet) {
    for (const auto& text : {"", "TREE1 #", "SET2 TREE1 #", "SET1 TREE1 # junk",
                             "SET1 TREE1 \"1\" #", "SET1 TREE1 \"1\" \"1\" # # #"}) {
        EXPECT_THROW(OrderedSet<int>::from_string(text), std::invalid_argument);
    }
    OrderedSet<int> set{3, 2, 4};
    const auto before = set.to_string();
    std::istringstream in("SET1 TREE1 \"1\" #");
    in >> set;
    EXPECT_TRUE(in.fail());
    EXPECT_EQ(set.to_string(), before);
    std::istringstream throwing("SET1 TREE1 bad");
    throwing.exceptions(std::ios::failbit);
    EXPECT_THROW(throwing >> set, std::ios_base::failure);
    EXPECT_EQ(set.to_string(), before);
}

TEST(Set, StringsDoublesAndStudents) {
    OrderedSet<std::string> strings{"hello world", "#", "", "a\"b\\c"};
    EXPECT_EQ(OrderedSet<std::string>::from_string(strings.to_string()), strings);
    OrderedSet<double> doubles{0.1, -2.5, 1.0 / 3};
    EXPECT_EQ(OrderedSet<double>::from_string(doubles.to_string()), doubles);
    OrderedSet<Student> students{Student(2, "Пётр", "2005-01-02"), Student(1, "Анна", "2005-03-04")};
    EXPECT_EQ(OrderedSet<Student>::from_string(students.to_string()), students);
    EXPECT_EQ(students.map([](const Student& s) { return Student(1, s.name(), s.birth_date()); }).size(), 1U);
}

struct SetComplexLess {
    bool operator()(const std::complex<double>& a, const std::complex<double>& b) const {
        return std::tuple(a.real(), a.imag()) < std::tuple(b.real(), b.imag());
    }
};

TEST(Set, ComplexComparatorAndAlgebra) {
    using Set = OrderedSet<std::complex<double>, SetComplexLess>;
    Set a{{1, 2}, {3, 4}}, b{{3, 4}, {5, 6}};
    EXPECT_EQ((a | b).size(), 3U);
    EXPECT_EQ(a & b, (Set{{3, 4}}));
    EXPECT_EQ(Set::from_string(a.to_string()), a);
}

struct SetDirectionCompare {
    bool descending = false;
    bool operator()(int a, int b) const { return descending ? a > b : a < b; }
};

TEST(Set, SwapExchangesComparatorAndContents) {
    using Set = OrderedSet<int, SetDirectionCompare>;

    Set left({1, 2}, SetDirectionCompare{false});
    Set right({3, 4, 5}, SetDirectionCompare{true});
    left.swap(right);

    EXPECT_TRUE(left.key_comp().descending);
    EXPECT_FALSE(right.key_comp().descending);
    EXPECT_EQ(left, (Set{3, 4, 5}));
    EXPECT_EQ(right, (Set{1, 2}));
}

TEST(Set, StatefulComparatorIsPreservedByAlgebraAndInput) {
    using Set = OrderedSet<int, SetDirectionCompare>;
    Set a({2, 1, 3}, SetDirectionCompare{true});
    Set b({3, 4}, SetDirectionCompare{true});
    auto combined = a | b;
    EXPECT_TRUE(combined.key_comp().descending);
    EXPECT_TRUE((a & b).key_comp().descending);
    EXPECT_TRUE((a - b).key_comp().descending);
    EXPECT_TRUE(a.map([](int x) { return -x; }).key_comp().descending);
    EXPECT_EQ(combined.reduce([](int x, std::string acc) { return acc + std::to_string(x); }, std::string{}), "4321");
    EXPECT_EQ(Set::from_string(a.to_string(), SetDirectionCompare{true}), a);
    Set restored(SetDirectionCompare{true});
    std::istringstream in(a.to_string());
    in >> restored;
    EXPECT_EQ(restored, a);
    EXPECT_EQ(restored.to_string(), a.to_string());
}

TEST(Set, RandomAlgebraMatchesStandardSets) {
    std::mt19937 generator(123);
    for (int round = 0; round < 30; ++round) {
        OrderedSet<int> a, b;
        std::set<int> a_ref, b_ref;
        for (int i = 0; i < 100; ++i) {
            int x = static_cast<int>(generator() % 150);
            int y = static_cast<int>(generator() % 150);
            a.insert(x); a_ref.insert(x);
            b.insert(y); b_ref.insert(y);
        }
        auto united = a | b;
        auto intersected = a & b;
        auto subtracted = a - b;
        for (int value = 0; value < 150; ++value) {
            EXPECT_EQ(united.contains(value), a_ref.contains(value) || b_ref.contains(value));
            EXPECT_EQ(intersected.contains(value), a_ref.contains(value) && b_ref.contains(value));
            EXPECT_EQ(subtracted.contains(value), a_ref.contains(value) && !b_ref.contains(value));
        }
    }
}
