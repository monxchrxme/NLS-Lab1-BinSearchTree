#include <tree_lab/search_tree.hpp>
#include "../examples/student.hpp"

#include <gtest/gtest.h>

#include <complex>
#include <limits>
#include <random>
#include <set>
#include <tuple>
#include <type_traits>
#include <vector> // Independent expected traversal sequences

template <typename T, typename Compare>
std::vector<T> tree_values(const SearchTree<T, Compare>& tree,
                          TraversalOrder order = TraversalOrder::left_root_right) {
    std::vector<T> result;
    tree.for_each([&](const T& value) { result.push_back(value); }, order);
    return result;
}

TEST(Tree, EmptyAndUniqueKeys) {
    SearchTree<int> tree;
    EXPECT_TRUE(tree.empty());
    EXPECT_EQ(tree.size(), 0U);
    EXPECT_FALSE(tree.find(1));
    EXPECT_FALSE(tree.erase(1));
    EXPECT_TRUE(tree.insert(8));
    EXPECT_FALSE(tree.insert(8));
    EXPECT_EQ(tree.size(), 1U);
    ASSERT_TRUE(tree.find(8));
    EXPECT_EQ(tree.find(8).value().get(), 8);
    tree.clear();
    tree.clear();
    EXPECT_TRUE(tree.empty());
}

TEST(Tree, AllSixDepthFirstTraversals) {
    const SearchTree<int> tree{4, 2, 6, 1, 3, 5, 7};
    EXPECT_EQ(tree_values(tree, TraversalOrder::root_left_right), (std::vector{4, 2, 1, 3, 6, 5, 7}));
    EXPECT_EQ(tree_values(tree, TraversalOrder::root_right_left), (std::vector{4, 6, 7, 5, 2, 3, 1}));
    EXPECT_EQ(tree_values(tree, TraversalOrder::left_root_right), (std::vector{1, 2, 3, 4, 5, 6, 7}));
    EXPECT_EQ(tree_values(tree, TraversalOrder::left_right_root), (std::vector{1, 3, 2, 5, 7, 6, 4}));
    EXPECT_EQ(tree_values(tree, TraversalOrder::right_root_left), (std::vector{7, 6, 5, 4, 3, 2, 1}));
    EXPECT_EQ(tree_values(tree, TraversalOrder::right_left_root), (std::vector{7, 5, 6, 3, 1, 2, 4}));
    EXPECT_THROW(tree.for_each([](int) {}, static_cast<TraversalOrder>(99)), std::invalid_argument);
}

TEST(Tree, BreadthFirstTraversalVisitsEachLevelFromLeftToRight) {
    constexpr auto order = TraversalOrder::breadth_first;
    EXPECT_TRUE(tree_values(SearchTree<int>{}, order).empty());
    EXPECT_EQ(tree_values(SearchTree<int>{8}, order), (std::vector{8}));

    const SearchTree<int> tree{8, 3, 10, 1, 6, 14, 4, 7, 13};
    EXPECT_EQ(tree_values(tree, order), (std::vector{8, 3, 10, 1, 6, 14, 4, 7, 13}));
}

TEST(Tree, BeginAndEndVisitInComparatorOrder) {
    const SearchTree<int> empty;
    EXPECT_EQ(empty.begin(), empty.end());

    const SearchTree<int, std::greater<int>> tree{4, 2, 6, 1, 3};
    std::vector<int> visited;
    for (const auto& value : tree) {
        static_assert(std::is_same_v<decltype(value), const int&>);
        EXPECT_EQ(&value, &tree.find(value).value().get());
        visited.push_back(value);
    }
    EXPECT_EQ(visited, (std::vector{6, 4, 3, 2, 1}));
}

TEST(Tree, ValuesSupportsAllTraversalOrders) {
    const SearchTree<int> tree{4, 2, 6, 1, 3, 5, 7};
    struct Example {
        TraversalOrder order;
        std::vector<int> expected;
    };
    const Example examples[]{
        {TraversalOrder::root_left_right, {4, 2, 1, 3, 6, 5, 7}},
        {TraversalOrder::root_right_left, {4, 6, 7, 5, 2, 3, 1}},
        {TraversalOrder::left_root_right, {1, 2, 3, 4, 5, 6, 7}},
        {TraversalOrder::left_right_root, {1, 3, 2, 5, 7, 6, 4}},
        {TraversalOrder::right_root_left, {7, 6, 5, 4, 3, 2, 1}},
        {TraversalOrder::right_left_root, {7, 5, 6, 3, 1, 2, 4}},
        {TraversalOrder::breadth_first, {4, 2, 6, 1, 3, 5, 7}}
    };
    const SearchTree<int> empty;
    const SearchTree<int> singleton{4};
    for (const auto& [order, expected] : examples) {
        SCOPED_TRACE(static_cast<int>(order));
        const auto range = tree.values(order);
        EXPECT_EQ(std::vector<int>(range.begin(), range.end()), expected);
        const auto empty_range = empty.values(order);
        EXPECT_EQ(empty_range.begin(), empty_range.end());
        const auto single_range = singleton.values(order);
        EXPECT_EQ(std::vector<int>(single_range.begin(), single_range.end()), (std::vector{4}));
    }
    EXPECT_THROW(tree.values(static_cast<TraversalOrder>(99)), std::invalid_argument);
}

TEST(Tree, TraversalIteratorCopiesAdvanceIndependently) {
    const SearchTree<int> tree{4, 2, 6, 1, 3, 5, 7};
    for (auto order : {TraversalOrder::root_left_right, TraversalOrder::breadth_first}) {
        const auto range = tree.values(order);
        auto iterator = range.begin();
        ++iterator;
        auto copy = iterator;
        ASSERT_EQ(*iterator, 2);
        EXPECT_EQ(*iterator++, 2);
        EXPECT_EQ(*copy, 2);
        EXPECT_NE(copy, iterator);
        EXPECT_EQ(++copy, iterator);
        EXPECT_EQ(*copy, order == TraversalOrder::breadth_first ? 6 : 1);
        while (iterator != range.end()) ++iterator;
        EXPECT_NE(copy, range.end());
    }
}

TEST(Tree, StructuralTraversalEmptyAndSingleton) {
    SearchTree<int> tree;
    int calls = 0;
    tree.for_each_node([&](const int&, std::size_t, TreeBranch) { ++calls; });
    EXPECT_EQ(calls, 0);
    tree.insert(8);
    std::as_const(tree).for_each_node([&](const auto& value, std::size_t depth, TreeBranch branch) {
        static_assert(std::is_same_v<decltype(value), const int&>);
        ++calls;
        EXPECT_EQ(&value, &tree.find(8).value().get());
        EXPECT_EQ(depth, 0U);
        EXPECT_EQ(branch, TreeBranch::root);
    });
    EXPECT_EQ(calls, 1);
}

TEST(Tree, StructuralTraversalReportsPreorderDepthAndPhysicalBranches) {
    const SearchTree<int> tree{8, 3, 10, 1, 6, 9, 12, 11};
    const auto before = tree.to_string();
    using Visit = std::tuple<int, std::size_t, TreeBranch>;
    std::vector<Visit> visited;
    tree.for_each_node([&](const int& value, std::size_t depth, TreeBranch branch) {
        visited.emplace_back(value, depth, branch);
        EXPECT_EQ(&value, &tree.find(value).value().get());
    });
    const std::vector<Visit> expected{
        {8, 0, TreeBranch::root}, {3, 1, TreeBranch::left}, {1, 2, TreeBranch::left},
        {6, 2, TreeBranch::right}, {10, 1, TreeBranch::right}, {9, 2, TreeBranch::left},
        {12, 2, TreeBranch::right}, {11, 3, TreeBranch::left}};
    EXPECT_EQ(visited, expected);
    EXPECT_EQ(tree.to_string(), before);
    const SearchTree<int, std::greater<int>> descending{8, 10, 9, 3, 6};
    visited.clear();
    descending.for_each_node([&](const int& value, std::size_t depth, TreeBranch branch) {
        visited.emplace_back(value, depth, branch);
    });
    const std::vector<Visit> reversed{
        {8, 0, TreeBranch::root}, {10, 1, TreeBranch::left}, {9, 2, TreeBranch::right},
        {3, 1, TreeBranch::right}, {6, 2, TreeBranch::left}};
    EXPECT_EQ(visited, reversed);
}

TEST(Tree, StructuralTraversalSupportsUserTypesAndCallbackExceptions) {
    const SearchTree<Student> tree{Student(2, "Иван", "2005-01-01"), Student(1, "Анна", "2006-01-01")};
    const auto before = tree.to_string();
    int calls = 0;
    EXPECT_THROW(tree.for_each_node([&](const Student& value, std::size_t depth, TreeBranch branch) {
        ++calls;
        EXPECT_EQ(value.id(), calls == 1 ? 2 : 1);
        EXPECT_EQ(depth, static_cast<std::size_t>(calls - 1));
        EXPECT_EQ(branch, calls == 1 ? TreeBranch::root : TreeBranch::left);
        if (calls == 2) { throw std::runtime_error("callback failed"); }
    }), std::runtime_error);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(tree.to_string(), before);
}

TEST(Tree, EraseLeafOneChildAndTwoChildren) {
    SearchTree<int> tree{8, 3, 10, 1, 6, 14, 4, 7, 13};
    EXPECT_TRUE(tree.erase(1));
    EXPECT_TRUE(tree.erase(14));
    EXPECT_TRUE(tree.erase(8));
    EXPECT_EQ(tree_values(tree), (std::vector{3, 4, 6, 7, 10, 13}));
    EXPECT_FALSE(tree.erase(8));
    for (int value : {3, 4, 6, 7, 10, 13}) {
        EXPECT_TRUE(tree.erase(value));
    }
    EXPECT_TRUE(tree.empty());
}

TEST(Tree, EraseDeepSuccessorWithRightChildPreservesOtherAddresses) {
    SearchTree<int> tree{10, 5, 20, 15, 17, 30};
    const auto* address = &tree.find(15).value().get();
    EXPECT_TRUE(tree.erase(10));
    EXPECT_EQ(&tree.find(15).value().get(), address);
    EXPECT_EQ(tree.to_string(), "TREE1 \"15\" \"5\" # # \"20\" \"17\" # # \"30\" # #");
}

TEST(Tree, RandomOperationsMatchStandardSet) {
    std::mt19937 generator(42);
    SearchTree<int> tree;
    std::set<int> reference;
    for (int step = 0; step < 10000; ++step) {
        int value = static_cast<int>(generator() % 500);
        switch (generator() % 3) {
        case 0: EXPECT_EQ(tree.insert(value), reference.insert(value).second); break;
        case 1: EXPECT_EQ(tree.erase(value), reference.erase(value) != 0); break;
        default: EXPECT_EQ(tree.contains(value), reference.contains(value)); break;
        }
        ASSERT_EQ(tree.size(), reference.size());
    }
    EXPECT_EQ(tree_values(tree), (std::vector<int>(reference.begin(), reference.end())));
}

TEST(Tree, CopyMoveAndSelfAssignment) {
    SearchTree<int> original{8, 3, 10, 6};
    auto copy = original;
    EXPECT_EQ(copy.to_string(), original.to_string());
    original.for_each([&](const int& value) {
        EXPECT_NE(&copy.find(value).value().get(), &value);
    });
    copy.erase(3);
    EXPECT_TRUE(original.contains(3));
    copy = original;
    auto* alias = &copy;
    copy = *alias;
    EXPECT_EQ(copy.to_string(), original.to_string());
    SearchTree<int> moved(std::move(copy));
    EXPECT_TRUE(copy.empty());
    EXPECT_TRUE(copy.insert(1));
    copy = std::move(moved);
    EXPECT_TRUE(moved.empty());
    copy = std::move(*alias);
    EXPECT_EQ(copy.to_string(), original.to_string());
    const auto expected = original.to_string();
    original.clear();
    EXPECT_EQ(copy.to_string(), expected);
}

TEST(Tree, MapWhereAndReduce) {
    SearchTree<int> tree{2, 1, 3};
    auto mapped = tree.map([](int value) { return -value; });
    EXPECT_EQ(tree_values(mapped), (std::vector{-3, -2, -1}));
    EXPECT_EQ(tree.map([](int) { return 0; }).size(), 1U);
    auto filtered = tree.where([](int value) { return value % 2; });
    EXPECT_EQ(tree_values(filtered), (std::vector{1, 3}));
    EXPECT_NE(&filtered.find(1).value().get(), &tree.find(1).value().get());
    EXPECT_EQ(tree.reduce([](int element, int accumulator) { return 2 * element + 3 * accumulator; }, 4), 144);
    EXPECT_EQ(tree.reduce([](int value, std::string acc) { return acc + std::to_string(value); }, std::string{}), "123");
    auto pointer = tree.reduce([](int value, std::unique_ptr<int[]> acc) {
        acc[0] += value;
        return acc;
    }, std::make_unique<int[]>(1));
    EXPECT_EQ(pointer[0], 6);
    EXPECT_TRUE(tree.where([](int) { return false; }).empty());
    EXPECT_EQ(SearchTree<int>{}.reduce([](int x, int y) { return x + y; }, 17), 17);
}

TEST(Tree, SubtreeRequiresExactValuesAndShape) {
    const SearchTree<int> tree{8, 3, 10, 1, 6};
    auto child = tree.subtree(3);
    EXPECT_EQ(child.to_string(), "TREE1 \"3\" \"1\" # # \"6\" # #");
    EXPECT_NE(&child.find(3).value().get(), &tree.find(3).value().get());
    EXPECT_TRUE(tree.contains_subtree(child));
    EXPECT_TRUE(tree.contains_subtree(tree));
    EXPECT_TRUE(tree.contains_subtree(SearchTree<int>{}));
    EXPECT_FALSE(SearchTree<int>{}.contains_subtree(tree));
    EXPECT_FALSE(tree.contains_subtree(SearchTree<int>{1, 3, 6}));
    child.erase(6);
    EXPECT_FALSE(tree.contains_subtree(child));
    EXPECT_THROW(tree.subtree(99), std::out_of_range);
}

TEST(Tree, SerializationPreservesShapeAndSupportsChainedStreams) {
    const SearchTree<int> tree{8, 3, 10};
    const std::string text = "TREE1 \"8\" \"3\" # # \"10\" # #";
    EXPECT_EQ(tree.to_string(), text);
    EXPECT_EQ(SearchTree<int>::from_string(text).to_string(), text);
    EXPECT_EQ(SearchTree<int>::from_string("  TREE1 # \n").size(), 0U);
    std::stringstream stream;
    stream << tree << '\n' << SearchTree<int>{};
    SearchTree<int> first, second{99};
    stream >> first >> second;
    EXPECT_FALSE(stream.fail());
    EXPECT_EQ(first.to_string(), text);
    EXPECT_TRUE(second.empty());
}

TEST(Tree, RejectsMalformedSerializationAndPreservesDestination) {
    const std::string invalid[]{
        "", "TREE2 #", "TREE1", "TREE1 8 # #", "TREE1 \"8\" #",
        "TREE1 \"8\" # # extra", "TREE1 \"8\" \"8\" # # #",
        "TREE1 \"8\" \"9\" # # #", "TREE1 \"8\" # \"7\" # #",
        "TREE1 \"8\" \"3\" # \"9\" # # #", "TREE1 \"x\" # #",
        "TREE1 \"8x\" # #", "TREE1 \"999999999999999999999999\" # #",
        "TREE1 \"8 # #", "TREE1 \"8\"##", "TREE1 #junk"
    };
    for (const auto& text : invalid) {
        SCOPED_TRACE(text);
        EXPECT_THROW(SearchTree<int>::from_string(text), std::invalid_argument);
    }
    SearchTree<int> tree{5, 4, 6};
    const auto before = tree.to_string();
    std::istringstream in("TREE1 \"8\" \"9\" # # #");
    in >> tree;
    EXPECT_TRUE(in.fail());
    EXPECT_EQ(tree.to_string(), before);
    std::istringstream throwing("bad");
    throwing.exceptions(std::ios::failbit);
    EXPECT_THROW(throwing >> tree, std::ios_base::failure);
    EXPECT_EQ(tree.to_string(), before);
}

TEST(Tree, StringsEscapeQuotesBackslashesWhitespaceAndHash) {
    const SearchTree<std::string> tree{"", "#", "two words", "quote\"slash\\\nline", "Привет"};
    auto copy = SearchTree<std::string>::from_string(tree.to_string());
    EXPECT_EQ(copy.to_string(), tree.to_string());
    EXPECT_EQ(tree_values(copy), tree_values(tree));
}

TEST(Tree, DoublesRoundTripExactly) {
    const SearchTree<double> tree{0.1, -1.5, 1.0 / 3, std::numeric_limits<double>::max(),
                                  std::numeric_limits<double>::min(), std::numeric_limits<double>::denorm_min()};
    auto copy = SearchTree<double>::from_string(tree.to_string());
    EXPECT_EQ(tree_values(copy), tree_values(tree));
    EXPECT_THROW(SearchTree<double>::from_string("TREE1 \"nan\" # #"), std::invalid_argument);
    EXPECT_THROW(ValueCodec<double>::encode(std::numeric_limits<double>::infinity()), std::invalid_argument);
}

struct ComplexLess {
    bool operator()(const std::complex<double>& left, const std::complex<double>& right) const {
        return std::tuple(left.real(), left.imag()) < std::tuple(right.real(), right.imag());
    }
};

TEST(Tree, ComplexNumbersWithCustomComparator) {
    using Tree = SearchTree<std::complex<double>, ComplexLess>;
    Tree tree{{2, 1}, {1, 5}, {2, -1}, {0.1, 1.0 / 3}};
    EXPECT_EQ(tree.size(), 4U);
    EXPECT_TRUE(tree.contains({2, -1}));
    EXPECT_EQ(Tree::from_string(tree.to_string()).to_string(), tree.to_string());
    EXPECT_TRUE(tree.erase({1, 5}));
    const std::complex<double> tiny(std::numeric_limits<double>::denorm_min(), -0.0);
    EXPECT_EQ(ValueCodec<std::complex<double>>::decode(ValueCodec<std::complex<double>>::encode(tiny)), tiny);
    EXPECT_THROW(ValueCodec<std::complex<double>>::decode("(1,2,3)"), std::invalid_argument);
}

TEST(Tree, StudentCodecAndExactSubtreeComparison) {
    const Student student(12, "Иван \"Иванов\"", "2005-04-12");
    SearchTree<Student> tree{student, Student(7, "Анна", "2006-03-01")};
    EXPECT_FALSE(tree.insert(Student(12, "Other name", "2000-01-01")));
    auto copy = SearchTree<Student>::from_string(tree.to_string());
    ASSERT_TRUE(copy.find(student));
    EXPECT_EQ(copy.find(student).value().get(), student);
    EXPECT_TRUE(tree.contains_subtree(copy));
    EXPECT_FALSE(tree.contains_subtree(SearchTree<Student>{Student(7, "Different", "2006-03-01")}));
    EXPECT_THROW(ValueCodec<Student>::decode("12 unquoted \"date\""), std::invalid_argument);
}

struct DirectionCompare {
    bool descending = false;
    bool operator()(int left, int right) const { return descending ? left > right : left < right; }
};

TEST(Tree, StatefulComparatorSurvivesAllFactories) {
    using Tree = SearchTree<int, DirectionCompare>;
    Tree tree({2, 1, 3}, DirectionCompare{true});
    EXPECT_EQ(tree_values(tree), (std::vector{3, 2, 1}));
    EXPECT_EQ(tree_values(Tree(tree)), tree_values(tree));
    EXPECT_EQ(tree_values(tree.map([](int x) { return x; })), tree_values(tree));
    EXPECT_EQ(tree_values(tree.where([](int) { return true; })), tree_values(tree));
    EXPECT_EQ(tree_values(tree.subtree(2)), tree_values(tree));
    EXPECT_EQ(Tree::from_string(tree.to_string(), DirectionCompare{true}).to_string(), tree.to_string());
    std::istringstream in(tree.to_string());
    Tree target(DirectionCompare{true});
    in >> target;
    EXPECT_EQ(target.to_string(), tree.to_string());
}

struct ThrowingValue {
    int value;
    static inline int copies_left = -1;
    explicit ThrowingValue(int item) : value(item) {}
    ThrowingValue(const ThrowingValue& other) : value(other.value) {
        if (copies_left == 0) {
            throw std::runtime_error("copy failed");
        }
        if (copies_left > 0) { --copies_left; }
    }
    ThrowingValue(ThrowingValue&&) noexcept = default;
    ThrowingValue& operator=(const ThrowingValue&) = delete;
    friend bool operator<(const ThrowingValue& a, const ThrowingValue& b) { return a.value < b.value; }
};

TEST(Tree, EraseTransplantsSuccessorsWithoutCopyingOrAssigningValues) {
    const std::vector<int> cases[]{
        {8, 3, 10},
        {8, 3, 10, 12},
        {20, 8, 30, 3, 10, 12},
        {1, 8, 3, 10, 12},
        {8, 3, 20, 10, 12, 30},
        {40, 8, 50, 3, 20, 10, 12, 30},
        {1, 8, 3, 20, 10, 12, 30}
    };
    for (const auto& values : cases) {
        SearchTree<ThrowingValue> tree;
        for (int value : values) tree.insert(ThrowingValue(value));
        std::vector<std::pair<int, const ThrowingValue*>> addresses;
        tree.for_each([&](const ThrowingValue& value) {
            if (value.value != 8) addresses.push_back({value.value, &value});
        });

        ThrowingValue::copies_left = 0;
        bool erased = false;
        EXPECT_NO_THROW(erased = tree.erase(ThrowingValue(8)));
        ThrowingValue::copies_left = -1;
        ASSERT_TRUE(erased);
        EXPECT_EQ(tree.size(), values.size() - 1);
        EXPECT_FALSE(tree.contains(ThrowingValue(8)));
        for (const auto& [value, address] : addresses) {
            const auto found = tree.find(ThrowingValue(value));
            ASSERT_TRUE(found);
            EXPECT_EQ(&found->get(), address);
        }
    }
}

TEST(Tree, EraseUsesPhysicalSuccessorWithDescendingComparator) {
    SearchTree<int, std::greater<int>> tree{8, 3, 10, 6, 1, 4};
    const auto* successor = &tree.find(6).value().get();
    EXPECT_TRUE(tree.erase(8));
    EXPECT_EQ(&tree.find(6).value().get(), successor);
    EXPECT_EQ(tree.to_string(),
              "TREE1 \"6\" \"10\" # # \"3\" \"4\" # # \"1\" # #");
}

TEST(Tree, FailedCopiesAndCallbacksPreserveSources) {
    SearchTree<ThrowingValue> source;
    for (int value : {4, 2, 6, 1, 3}) { source.insert(ThrowingValue(value)); }
    SearchTree<ThrowingValue> target;
    target.insert(ThrowingValue(99));
    ThrowingValue::copies_left = 2;
    EXPECT_THROW(target = source, std::runtime_error);
    EXPECT_EQ(target.size(), 1U);
    EXPECT_TRUE(target.contains(ThrowingValue(99)));
    ThrowingValue::copies_left = 0;
    ThrowingValue value(10);
    EXPECT_THROW(source.insert(value), std::runtime_error);
    EXPECT_EQ(source.size(), 5U);
    ThrowingValue::copies_left = -1;
    EXPECT_TRUE(source.erase(ThrowingValue(4))); // no T assignment needed
    SearchTree<int> tree{2, 1, 3};
    EXPECT_THROW(tree.map([](int x) -> int {
        if (x == 3) { throw std::runtime_error("map failed"); }
        return x;
    }), std::runtime_error);
    EXPECT_THROW(tree.where([](int) -> bool { throw std::runtime_error("predicate failed"); }), std::runtime_error);
    EXPECT_EQ(tree_values(tree), (std::vector{1, 2, 3}));
}

struct ThrowingCompare {
    std::shared_ptr<bool> fail;
    bool operator()(int left, int right) const {
        if (*fail) { throw std::runtime_error("comparison failed"); }
        return left < right;
    }
};

TEST(Tree, ThrowingComparatorDoesNotDetachNodesOrCommitInput) {
    auto fail = std::make_shared<bool>(false);
    SearchTree<int, ThrowingCompare> tree({4, 2, 6}, ThrowingCompare{fail});
    const auto before = tree.to_string();
    *fail = true;
    EXPECT_THROW(tree.insert(5), std::runtime_error);
    EXPECT_THROW(tree.erase(4), std::runtime_error);
    EXPECT_THROW(tree.find(2), std::runtime_error);
    std::istringstream in("TREE1 \"8\" \"3\" # # #");
    EXPECT_THROW(in >> tree, std::runtime_error);
    EXPECT_EQ(tree.to_string(), before);
    EXPECT_EQ(tree.size(), 3U);
    *fail = false;
    EXPECT_TRUE(tree.erase(4));
}

TEST(Tree, DeepTreesUseNoRecursiveDestructionCopyOrParsing) {
    constexpr int count = 100000;
    std::ostringstream text;
    text << "TREE1 ";
    for (int i = 0; i < count; ++i) { text << '"' << i << "\" # "; }
    text << '#';
    auto tree = SearchTree<int>::from_string(text.str());
    EXPECT_EQ(tree.size(), static_cast<std::size_t>(count));
    std::size_t visits = 0;
    bool metadata_valid = true;
    tree.for_each_node([&](const int& value, std::size_t depth, TreeBranch branch) {
        metadata_valid = metadata_valid && value == static_cast<int>(visits) && depth == visits &&
            branch == (visits == 0 ? TreeBranch::root : TreeBranch::right);
        ++visits;
    });
    EXPECT_EQ(visits, static_cast<std::size_t>(count));
    EXPECT_TRUE(metadata_valid);
    auto copy = tree;
    EXPECT_TRUE(tree.contains_subtree(copy));
    EXPECT_EQ(copy.to_string(), text.str());
    EXPECT_EQ(copy.reduce([](int value, long long acc) { return acc + value; }, 0LL),
              static_cast<long long>(count) * (count - 1) / 2);
    auto subtree = tree.subtree(count - 10);
    EXPECT_EQ(subtree.size(), 10U);
    EXPECT_TRUE(tree.erase(count - 1));
    copy = tree;
    EXPECT_EQ(copy.to_string(), tree.to_string());
    copy = SearchTree<int>{-1};
    EXPECT_TRUE(copy.contains(-1));
    EXPECT_EQ(copy.size(), 1U);
    copy.clear();
    EXPECT_TRUE(copy.empty());
    // The nearly complete temporary tree must also be destroyed safely on error.
    EXPECT_THROW(SearchTree<int>::from_string(text.str().substr(0, text.str().size() - 1)), std::invalid_argument);
    std::ostringstream left_text;
    left_text << "TREE1 ";
    for (int i = count; i > 0; --i) { left_text << '"' << i << "\" "; }
    for (int i = 0; i <= count; ++i) { left_text << "# "; }
    auto left_tree = SearchTree<int>::from_string(left_text.str());
    EXPECT_EQ(left_tree.size(), static_cast<std::size_t>(count));
    visits = 0;
    metadata_valid = true;
    left_tree.for_each_node([&](const int& value, std::size_t depth, TreeBranch branch) {
        metadata_valid = metadata_valid && value == count - static_cast<int>(visits) && depth == visits &&
            branch == (visits == 0 ? TreeBranch::root : TreeBranch::left);
        ++visits;
    });
    EXPECT_EQ(visits, static_cast<std::size_t>(count));
    EXPECT_TRUE(metadata_valid);
    left_tree.clear();
}

TEST(Tree, SearchAndTraversalDoNotKeepRemovedValuesAlive) {
    using Value = std::shared_ptr<int>;
    struct ByValue {
        bool operator()(const Value& a, const Value& b) const { return *a < *b; }
    };
    SearchTree<Value, ByValue> tree;
    auto root_value = std::make_shared<int>(8);
    std::weak_ptr<int> root_watch = root_value;
    tree.insert(std::move(root_value));
    tree.insert(std::make_shared<int>(3));
    auto successor_value = std::make_shared<int>(10);
    std::weak_ptr<int> successor_watch = successor_value;
    tree.insert(std::move(successor_value));
    const auto match = tree.find(std::make_shared<int>(8));
    static_assert(std::is_same_v<decltype(tree.find(root_value)),
                  std::optional<std::reference_wrapper<const Value>>>);
    ASSERT_TRUE(match);
    tree.for_each([](const Value&) {});
    tree.for_each_node([](const Value&, std::size_t, TreeBranch) {});
    auto independent = tree.subtree(std::make_shared<int>(3));
    EXPECT_TRUE(tree.erase(match->get()));
    EXPECT_TRUE(root_watch.expired()); // the optional reference did not own the erased node
    EXPECT_FALSE(successor_watch.expired());
    tree.clear();
    EXPECT_TRUE(successor_watch.expired());
    EXPECT_TRUE(independent.contains(std::make_shared<int>(3)));
}

TEST(Tree, MovePreservesElementIdentityAndComparatorForReuse) {
    auto failure_switch = std::make_shared<bool>(false);
    SearchTree<int, ThrowingCompare> source({3, 1, 4}, ThrowingCompare{failure_switch});
    const int& original = source.find(3).value().get();
    auto moved = std::move(source);
    EXPECT_EQ(&moved.find(3).value().get(), &original);
    ASSERT_EQ(source.key_comp().fail, failure_switch);
    EXPECT_TRUE(source.insert(7));
    EXPECT_TRUE(source.contains(7));
    source = std::move(moved);
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(&source.find(3).value().get(), &original);
    ASSERT_EQ(moved.key_comp().fail, failure_switch);
    EXPECT_TRUE(moved.insert(5));
    EXPECT_TRUE(moved.contains(5));
}

struct CopyFailureCompare {
    static inline bool fail_copy = false;
    bool descending = false;

    explicit CopyFailureCompare(bool reverse = false) : descending(reverse) {}
    CopyFailureCompare(const CopyFailureCompare& other) : descending(other.descending) {
        if (fail_copy) throw std::runtime_error("comparator copy");
    }
    CopyFailureCompare(CopyFailureCompare&&) noexcept = default;
    CopyFailureCompare& operator=(CopyFailureCompare&&) noexcept = default;

    bool operator()(int a, int b) const { return descending ? a > b : a < b; }
};

TEST(Tree, ExplicitAssignmentsPreserveBothTreesOnComparatorCopyFailure) {
    using Tree = SearchTree<int, CopyFailureCompare>;
    Tree source({4, 2, 6}, CopyFailureCompare{true});
    Tree target({1, 0}, CopyFailureCompare{false});
    const auto original_source = source.to_string();
    const auto original_target = target.to_string();
    CopyFailureCompare::fail_copy = true;
    EXPECT_THROW(target = source, std::runtime_error);
    EXPECT_THROW(target = std::move(source), std::runtime_error);
    CopyFailureCompare::fail_copy = false;
    EXPECT_EQ(source.to_string(), original_source);
    EXPECT_EQ(target.to_string(), original_target);
    EXPECT_FALSE(target.key_comp().descending);

    target = source;
    EXPECT_TRUE(target.key_comp().descending);
    EXPECT_NE(&target.find(4).value().get(), &source.find(4).value().get());
    const auto* original = &source.find(4).value().get();
    target = std::move(source);
    EXPECT_EQ(&target.find(4).value().get(), original);
    EXPECT_TRUE(target.key_comp().descending);
    EXPECT_TRUE(source.empty());
    EXPECT_TRUE(source.key_comp().descending);
    EXPECT_TRUE(source.insert(8));
}

struct FallibleCompare {
    static inline bool fail_move = false;
    static inline bool fail_assignment = false;
    bool descending = false;

    explicit FallibleCompare(bool reverse = false) : descending(reverse) {}
    FallibleCompare(const FallibleCompare&) = default;
    FallibleCompare(FallibleCompare&& other) : descending(other.descending) {
        if (fail_move) throw std::runtime_error("comparator move");
    }
    FallibleCompare& operator=(FallibleCompare&& other) {
        descending = other.descending;
        if (fail_assignment) throw std::runtime_error("comparator assignment");
        return *this;
    }
    friend void swap(FallibleCompare& left, FallibleCompare& right) noexcept {
        std::swap(left.descending, right.descending);
    }
    bool operator()(int a, int b) const { return descending ? a > b : a < b; }
};

TEST(Tree, ThrowingComparatorMovementAndAssignmentPreserveValidTreeStates) {
    using Tree = SearchTree<int, FallibleCompare>;
    Tree source({4, 2, 6}, FallibleCompare{true});
    Tree target({1, 0}, FallibleCompare{false});
    const auto original_source = source.to_string();
    const auto original_target = target.to_string();
    FallibleCompare::fail_move = true;
    EXPECT_THROW(target = source, std::runtime_error);
    FallibleCompare::fail_move = false;
    EXPECT_EQ(target.to_string(), original_target);
    EXPECT_EQ(source.to_string(), original_source);

    FallibleCompare::fail_assignment = true;
    EXPECT_THROW(target = source, std::runtime_error);
    EXPECT_TRUE(target.empty());
    EXPECT_EQ(source.to_string(), original_source);
    EXPECT_THROW(target = std::move(source), std::runtime_error);
    FallibleCompare::fail_assignment = false;
    EXPECT_TRUE(target.empty());
    EXPECT_EQ(source.to_string(), original_source);
    const auto* address = &source.find(4).value().get();
    target = std::move(source);
    EXPECT_EQ(&target.find(4).value().get(), address);
    EXPECT_TRUE(source.empty());
    EXPECT_TRUE(source.insert(8));
}

TEST(Tree, SwapExchangesNodesAndOrderingWithNothrowComparatorSwap) {
    using Tree = SearchTree<int, FallibleCompare>;

    Tree left({4, 2, 6}, FallibleCompare{true});
    Tree right({1, 0}, FallibleCompare{false});
    const auto* left_root = &left.find(4).value().get();
    const auto* right_root = &right.find(1).value().get();
    FallibleCompare::fail_move = true;
    FallibleCompare::fail_assignment = true;
    left.swap(right);
    left.swap(left);
    FallibleCompare::fail_move = false;
    FallibleCompare::fail_assignment = false;

    EXPECT_FALSE(left.key_comp().descending);
    EXPECT_TRUE(right.key_comp().descending);
    EXPECT_EQ(tree_values(left), (std::vector{0, 1}));
    EXPECT_EQ(tree_values(right), (std::vector{6, 4, 2}));
    EXPECT_EQ(&left.find(1).value().get(), right_root);
    EXPECT_EQ(&right.find(4).value().get(), left_root);
}

struct FallibleValueMove {
    static inline bool fail = false;
    int value;

    explicit FallibleValueMove(int number) : value(number) {}
    FallibleValueMove(const FallibleValueMove&) = default;
    FallibleValueMove(FallibleValueMove&& other) : value(other.value) {
        if (fail) throw std::runtime_error("value move");
    }
    bool operator<(const FallibleValueMove& other) const { return value < other.value; }
};

TEST(Tree, ByValueNodeConstructorKeepsTreeUnchangedOnMoveFailure) {
    SearchTree<FallibleValueMove> tree;
    const FallibleValueMove first(1), second(2);
    EXPECT_TRUE(tree.insert(first));
    const auto* original = &tree.find(first).value().get();
    FallibleValueMove::fail = true;
    EXPECT_NO_THROW(EXPECT_FALSE(tree.insert(first)));
    EXPECT_THROW(tree.insert(second), std::runtime_error);
    EXPECT_THROW(SearchTree<FallibleValueMove> copy(tree), std::runtime_error);
    FallibleValueMove::fail = false;
    EXPECT_EQ(tree.size(), 1U);
    EXPECT_EQ(&tree.find(first).value().get(), original);
    EXPECT_TRUE(tree.insert(second));
}
