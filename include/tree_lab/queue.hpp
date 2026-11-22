#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

template <typename T>
class Queue {
private:
    std::unique_ptr<T[]> slots_;
    std::size_t capacity_{0};
    std::size_t head_{0};
    std::size_t size_{0};

    std::size_t index(std::size_t offset) const noexcept {
        return (head_ + offset) % capacity_;
    }

    void grow() {
        if (capacity_ > std::numeric_limits<std::size_t>::max() / 2)
            throw std::length_error("Queue too large");

        const auto capacity = capacity_ == 0 ? 4 : 2 * capacity_;
        auto next = std::make_unique<T[]>(capacity);
        for (std::size_t i = 0; i < size_; ++i) {
            next[i] = std::move(slots_[index(i)]);
        }
        slots_ = std::move(next);
        capacity_ = capacity;
        head_ = 0;
    }

public:
    Queue() = default;

    Queue(const Queue& other) : capacity_(other.capacity_), size_(other.size_) {
        if (capacity_ == 0) return;

        slots_ = std::make_unique<T[]>(capacity_);
        for (std::size_t i = 0; i < size_; ++i) {
            slots_[i] = other.slots_[other.index(i)];
        }
    }

    Queue(Queue&& other) noexcept
        : slots_(std::move(other.slots_)), capacity_(std::exchange(other.capacity_, 0)),
          head_(std::exchange(other.head_, 0)), size_(std::exchange(other.size_, 0)) {}

    Queue& operator=(const Queue& other) {
        if (this != &other) {
            Queue copy(other);
            slots_ = std::move(copy.slots_);
            capacity_ = copy.capacity_;
            head_ = copy.head_;
            size_ = copy.size_;
        }
        return *this;
    }

    Queue& operator=(Queue&& other) noexcept {
        if (this != &other) {
            slots_ = std::move(other.slots_);
            capacity_ = std::exchange(other.capacity_, 0);
            head_ = std::exchange(other.head_, 0);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    void swap(Queue& other) noexcept {
    slots_.swap(other.slots_);
    std::swap(capacity_, other.capacity_);
    std::swap(head_, other.head_);
    std::swap(size_, other.size_);
    }

    void push(T value) {
        if (size_ == capacity_) grow();
        slots_[index(size_)] = std::move(value);
        ++size_;
    }
    
    void pop() {
        if (empty()) throw std::out_of_range("Empty Queue");
        slots_[head_] = T{};
        head_ = index(1);
        --size_;
    }

    T& front() {
        if (empty()) throw std::out_of_range("Empty Queue");
        return slots_[head_];
    }

    const T& front() const {
        if (empty()) throw std::out_of_range("Empty Queue");
        return slots_[head_];
    }

    bool empty() const noexcept { return size_ == 0; }
    std::size_t size() const noexcept { return size_; }

    void clear() {
        while (!empty()) pop();
    }
};
