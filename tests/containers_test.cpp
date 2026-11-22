#include <tree_lab/queue.hpp>
#include <tree_lab/stack.hpp>

#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <string>

// Counters check buffer cleanup when copying or assignment fails.
struct CountedItem {
    inline static int live = 0;
    inline static int copies_left = -1;
    int value{0};
    CountedItem() { ++live; }
    explicit CountedItem(int v) : value(v) { ++live; }
    CountedItem(const CountedItem& other) : value(other.value) {
        if (copies_left == 0) { throw std::runtime_error("copy"); }
        if (copies_left > 0) { --copies_left; }
        ++live;
    }
    CountedItem(CountedItem&& other) noexcept(false) : value(std::exchange(other.value, -1)) { ++live; }
    CountedItem& operator=(const CountedItem& other) {
        if (copies_left == 0) throw std::runtime_error("copy");
        if (copies_left > 0) --copies_left;
        value = other.value;
        return *this;
    }
    CountedItem& operator=(CountedItem&& other) noexcept(false) {
        value = std::exchange(other.value, -1);
        return *this;
    }
    ~CountedItem() { --live; }
};

TEST(DynamicArray, GrowthAccessAndSelfReferencingAppend) {
    DynamicArray<std::string> array;
    EXPECT_TRUE(array.empty());
    EXPECT_EQ(array.begin(), array.end());
    EXPECT_THROW(array.pop_back(), std::out_of_range);
    EXPECT_THROW(array.back(), std::out_of_range);
    EXPECT_THROW(std::as_const(array).back(), std::out_of_range);
    EXPECT_THROW(array.at(0), std::out_of_range);
    array.push_back("first");
    array.push_back(array[0]); // alias survives reallocation
    std::size_t expansions = 0;
    for (int i = 0; i < 1000; ++i) {
        const auto before = array.capacity();
        array.push_back(std::to_string(i));
        expansions += array.capacity() != before;
    }
    EXPECT_LT(expansions, 12U);
    EXPECT_EQ(array[0], "first");
    EXPECT_EQ(std::as_const(array)[1], "first");
    EXPECT_THROW(array[array.size()], std::out_of_range);
    EXPECT_EQ(array.back(), "999");
    array.pop_back();
    EXPECT_EQ(std::as_const(array).back(), "998");
    std::size_t count = 0;
    for (const auto& value : array) { count += !value.empty(); }
    EXPECT_EQ(count, array.size());
}

TEST(DynamicArray, CopyMoveAndBufferLifetime) {
    {
        DynamicArray<CountedItem> source;
        source.reserve(8);
        EXPECT_EQ(CountedItem::live, 8);
        source.push_back(CountedItem(1));
        source.push_back(CountedItem(2));
        auto copy = source;
        EXPECT_EQ(copy.capacity(), source.capacity());
        copy[1].value = 3;
        EXPECT_EQ(source.back().value, 2);
        source = copy;
        auto moved = std::move(source);
        EXPECT_TRUE(source.empty());
        source.push_back(CountedItem(7));
        source = std::move(moved);
        EXPECT_TRUE(moved.empty());
        copy.pop_back();
        EXPECT_EQ(CountedItem::live, source.capacity() + copy.capacity());
        copy.clear();
        source.clear();
        EXPECT_EQ(CountedItem::live, source.capacity() + copy.capacity());
    }
    EXPECT_EQ(CountedItem::live, 0);
}

TEST(DynamicArray, InitialCapacityAndExplicitAssignments) {
    DynamicArray<int> empty(8);
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.capacity(), 8U);
    auto empty_copy = empty;
    EXPECT_TRUE(empty_copy.empty());
    EXPECT_EQ(empty_copy.capacity(), 8U);
    DynamicArray<int> zero(0);
    EXPECT_EQ(zero.capacity(), 0U);
    DynamicArray<int> one_value{8};
    EXPECT_EQ(one_value.size(), 1U);
    EXPECT_EQ(one_value[0], 8);

    DynamicArray<int> source(16);
    source.push_back(42);
    DynamicArray<int> target(32);
    target.push_back(7);
    target = source;
    EXPECT_EQ(target.size(), 1U);
    EXPECT_EQ(target.capacity(), 16U);
    EXPECT_NE(target.begin(), source.begin());
    target[0] = 9;
    EXPECT_EQ(source[0], 42);

    auto* same = &target;
    target = *same;
    target = std::move(*same);
    EXPECT_EQ(target[0], 9);
    EXPECT_EQ(target.capacity(), 16U);

    const auto* buffer = source.begin();
    target = std::move(source);
    EXPECT_EQ(target.begin(), buffer);
    EXPECT_EQ(target.capacity(), 16U);
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.capacity(), 0U);
    source.push_back(5);
    EXPECT_EQ(source.back(), 5);
}

TEST(DynamicArray, PopClearAndShrinkReleaseOwnedResources) {
    DynamicArray<std::shared_ptr<int>> array;
    array.push_back(std::make_shared<int>(1));
    array.push_back(std::make_shared<int>(2));
    std::weak_ptr<int> first = array[0];
    std::weak_ptr<int> second = array[1];
    array.pop_back();
    EXPECT_TRUE(second.expired());
    array.resize(0);
    EXPECT_TRUE(first.expired());

    array.push_back(std::make_shared<int>(3));
    first = array[0];
    array.clear();
    EXPECT_TRUE(first.expired());
    array.resize(2);
    EXPECT_FALSE(array[0]);
    EXPECT_FALSE(array[1]);
}

TEST(DynamicArray, SupportsOverAlignedElementsAndRejectsSizeOverflow) {
    struct alignas(128) Item {
        int value{0};
        Item() = default;
        explicit Item(int number) : value(number) {}
    };

    DynamicArray<Item> array;
    array.push_back(Item(42));
    array.reserve(16);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(array.begin()) % alignof(Item), 0U);
    EXPECT_EQ(array.back().value, 42);
    EXPECT_THROW(array.reserve(std::numeric_limits<std::size_t>::max()),
                 std::length_error);
    EXPECT_EQ(array.size(), 1U);
    EXPECT_EQ(array.back().value, 42);
}

TEST(DynamicArray, MoveGrowthAndFailedCopyAssignment) {
    DynamicArray<CountedItem> source;
    for (int i = 0; i < 4; ++i) { source.push_back(CountedItem(i)); }
    DynamicArray<CountedItem> target;
    target.push_back(CountedItem(42));
    const auto* original = source.begin();
    CountedItem::copies_left = 0;
    EXPECT_THROW(source.push_back(source[0]), std::runtime_error);
    EXPECT_EQ(source.begin(), original);
    EXPECT_EQ(source.capacity(), 4U);

    // Growth and append use moving even when copying is available and throws.
    source.push_back(CountedItem(4));
    EXPECT_EQ(source.size(), 5U);
    EXPECT_EQ(source.capacity(), 8U);
    EXPECT_EQ(CountedItem::live, source.capacity() + target.capacity());
    CountedItem::copies_left = 1;
    EXPECT_THROW(target = source, std::runtime_error);
    CountedItem::copies_left = -1;
    EXPECT_EQ(CountedItem::live, source.capacity() + target.capacity());
    EXPECT_EQ(target.back().value, 42);
    for (int i = 0; i < 5; ++i) { EXPECT_EQ(source[i].value, i); }
}

struct MoveOnlyItem {
    int value{0};
    static inline int fail_on = -1;
    MoveOnlyItem() = default;
    explicit MoveOnlyItem(int v) : value(v) {}
    MoveOnlyItem(MoveOnlyItem&& other) : value(other.value) {
        if (value == fail_on) { throw std::runtime_error("move"); }
        other.value = -1;
    }
    MoveOnlyItem& operator=(MoveOnlyItem&& other) {
        if (other.value == fail_on) throw std::runtime_error("move");
        value = std::exchange(other.value, -1);
        return *this;
    }
};

TEST(DynamicArray, ThrowingMoveOnlyElementsRemainDestructibleAndReusable) {
    DynamicArray<MoveOnlyItem> array;
    for (int i = 0; i < 4; ++i) array.push_back(MoveOnlyItem(i));
    const auto* original = array.begin();
    MoveOnlyItem::fail_on = 1;
    EXPECT_THROW(array.push_back(MoveOnlyItem(4)), std::runtime_error);
    MoveOnlyItem::fail_on = -1;
    ASSERT_EQ(array.size(), 4U);
    EXPECT_EQ(array.begin(), original);
    EXPECT_EQ(array.capacity(), 4U);
    EXPECT_EQ(array[0].value, -1);
    EXPECT_EQ(array[1].value, 1);
    array.clear();
    array.push_back(MoveOnlyItem(3));
    EXPECT_EQ(array.back().value, 3);
}

TEST(Stack, LargeLifoCopyAndMove) {
    Stack<int> stack;
    EXPECT_THROW(stack.pop(), std::out_of_range);
    EXPECT_THROW(std::as_const(stack).top(), std::out_of_range);
    for (int i = 0; i < 100000; ++i) { stack.push(i); }
    auto copy = stack;
    copy.pop();
    Stack<int> moved;
    moved = std::move(copy);
    EXPECT_TRUE(copy.empty());
    EXPECT_EQ(moved.size(), 99999U);
    copy.push(42);
    EXPECT_EQ(copy.top(), 42);
    for (int i = 99999; i >= 0; --i) {
        ASSERT_EQ(std::as_const(stack).top(), i);
        stack.pop();
    }
    moved.clear();
    EXPECT_TRUE(moved.empty());
    EXPECT_TRUE(stack.empty());
}

TEST(Queue, WrappedGrowthCopyAndMoveKeepLogicalOrder) {
    Queue<int> queue;
    EXPECT_THROW(queue.front(), std::out_of_range);
    EXPECT_THROW(queue.pop(), std::out_of_range);
    for (int i = 0; i < 8; ++i) { queue.push(i); }
    for (int i = 0; i < 5; ++i) { EXPECT_EQ(queue.front(), i); queue.pop(); }
    for (int i = 8; i < 13; ++i) { queue.push(i); }
    auto copy = queue; // full, wrapped ring
    queue.push(queue.front()); // grow while argument aliases an old slot
    Queue<int> assigned;
    assigned = queue;
    auto moved = std::move(queue);
    ASSERT_TRUE(queue.empty());
    queue.push(42);
    EXPECT_EQ(queue.front(), 42);
    queue = std::move(moved);
    EXPECT_TRUE(moved.empty());
    for (int i = 5; i < 13; ++i) {
        ASSERT_EQ(std::as_const(queue).front(), i);
        ASSERT_EQ(copy.front(), i);
        ASSERT_EQ(assigned.front(), i);
        queue.pop(); copy.pop(); assigned.pop();
    }
    EXPECT_TRUE(copy.empty());
    EXPECT_EQ(queue.front(), 5);
    EXPECT_EQ(queue.size(), 1U);
    EXPECT_EQ(assigned.front(), 5);
    queue.clear();
    EXPECT_THROW(std::as_const(queue).front(), std::out_of_range);
}

TEST(Queue, InitialCapacityAndMoveGrowth) {
    {
        Queue<CountedItem> queue;
        EXPECT_EQ(CountedItem::live, 0);
        queue.push(CountedItem(0));
        EXPECT_EQ(CountedItem::live, 4);
        for (int i = 1; i < 4; ++i) { queue.push(CountedItem(i)); }
        queue.pop(); queue.pop();
        EXPECT_EQ(CountedItem::live, 4);
        queue.push(CountedItem(4)); queue.push(CountedItem(5));
        CountedItem::copies_left = 0;
        queue.push(CountedItem(6));
        CountedItem::copies_left = -1;
        EXPECT_EQ(queue.size(), 5U);
        EXPECT_EQ(CountedItem::live, 8);
        for (int i = 2; i <= 4; ++i) {
            EXPECT_EQ(queue.front().value, i);
            queue.pop();
        }
        EXPECT_EQ(CountedItem::live, 8);
        queue.clear();
        EXPECT_TRUE(queue.empty());
        EXPECT_EQ(CountedItem::live, 8);
    }
    EXPECT_EQ(CountedItem::live, 0);
}

TEST(Queue, WrappedPopAndClearReleaseOwnedResources) {
    Queue<std::shared_ptr<int>> queue;
    for (int i = 1; i <= 4; ++i) queue.push(std::make_shared<int>(i));
    std::weak_ptr<int> first = queue.front();
    queue.pop();
    EXPECT_TRUE(first.expired());

    // The next insertion reuses the slot after wrapping around the buffer.
    queue.push(std::make_shared<int>(5));
    first = queue.front();
    queue.push(std::make_shared<int>(6)); // grow after wrapping
    queue.pop();
    EXPECT_TRUE(first.expired());
    std::weak_ptr<int> last = queue.front();
    queue.clear();
    EXPECT_TRUE(last.expired());
    queue.push(std::make_shared<int>(4));
    EXPECT_EQ(*queue.front(), 4);
}

TEST(Queue, CopyAssignmentFailureAndSelfAssignment) {
    {
        Queue<CountedItem> source;
        for (int i = 1; i <= 6; ++i) source.push(CountedItem(i));
        source.pop();
        source.pop();
        Queue<CountedItem> target;
        target.push(CountedItem(42));
        const auto live = CountedItem::live;
        CountedItem::copies_left = 1;
        EXPECT_THROW(target = source, std::runtime_error);
        CountedItem::copies_left = -1;
        EXPECT_EQ(CountedItem::live, live);
        EXPECT_EQ(target.front().value, 42);
        EXPECT_EQ(target.size(), 1U);
        EXPECT_EQ(source.front().value, 3);

        target = source;
        auto* same = &target;
        target = *same;
        target = std::move(*same);
        for (int i = 3; i <= 6; ++i) {
            ASSERT_EQ(target.front().value, i);
            target.pop();
        }
        EXPECT_TRUE(target.empty());
        EXPECT_EQ(source.front().value, 3);
        auto copy = target;
        EXPECT_TRUE(copy.empty());
        copy.push(CountedItem(7));
        target = std::move(copy);
        EXPECT_TRUE(copy.empty());
        EXPECT_EQ(target.front().value, 7);
    }
    EXPECT_EQ(CountedItem::live, 0);
}

struct ThrowingAssignmentItem {
    static inline int fail_on = -1;
    int value{0};

    ThrowingAssignmentItem() = default;
    explicit ThrowingAssignmentItem(int number) : value(number) {}
    ThrowingAssignmentItem(const ThrowingAssignmentItem&) = default;
    ThrowingAssignmentItem(ThrowingAssignmentItem&&) noexcept = default;

    ThrowingAssignmentItem& operator=(const ThrowingAssignmentItem& other) {
        if (other.value == fail_on) throw std::runtime_error("copy assignment");
        value = other.value;
        return *this;
    }
    ThrowingAssignmentItem& operator=(ThrowingAssignmentItem&&) {
        throw std::runtime_error("move assignment must not be selected");
    }
};

TEST(Queue, ThrowingMoveAssignmentDoesNotFallBackToCopying) {
    Queue<ThrowingAssignmentItem> queue;
    EXPECT_THROW(queue.push(ThrowingAssignmentItem(1)), std::runtime_error);
    EXPECT_TRUE(queue.empty());
}

TEST(Queue, FailedWrappedGrowthLeavesQueueDestructibleAndReusable) {
    Queue<MoveOnlyItem> queue;
    for (int i = 0; i < 4; ++i) queue.push(MoveOnlyItem(i));
    queue.pop();
    queue.pop();
    queue.push(MoveOnlyItem(4));
    queue.push(MoveOnlyItem(5));
    MoveOnlyItem::fail_on = 3;
    EXPECT_THROW(queue.push(MoveOnlyItem(6)), std::runtime_error);
    MoveOnlyItem::fail_on = -1;
    EXPECT_EQ(queue.size(), 4U);
    EXPECT_EQ(queue.front().value, -1); // moved before the next assignment failed
    queue.pop();
    EXPECT_EQ(queue.front().value, 3);
    queue.clear();
    queue.push(MoveOnlyItem(7));
    EXPECT_EQ(queue.front().value, 7);
}

TEST(DynamicArray, ThrowingMoveAssignmentDoesNotFallBackToCopying) {
    DynamicArray<ThrowingAssignmentItem> array(4);
    array.resize(2);
    array[0].value = 1;
    array[1].value = 2;
    const auto* original = array.begin();

    EXPECT_THROW(array.reserve(8), std::runtime_error);
    EXPECT_THROW(array.push_back(ThrowingAssignmentItem(3)), std::runtime_error);
    EXPECT_EQ(array.begin(), original);
    EXPECT_EQ(array.capacity(), 4U);
    EXPECT_EQ(array.size(), 2U);
    EXPECT_EQ(array[0].value, 1);
    EXPECT_EQ(array[1].value, 2);
}
