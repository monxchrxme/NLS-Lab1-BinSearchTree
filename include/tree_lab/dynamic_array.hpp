#pragma once

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

template <typename T>
class DynamicArray {
private:
    std::unique_ptr<T[]> data_;
    std::size_t size_{0};
    std::size_t capacity_{0};

public:
    DynamicArray() = default;

    explicit DynamicArray(std::size_t initial_capacity) {
        reserve(initial_capacity);
    }

    DynamicArray(std::initializer_list<T> values) {
        reserve(values.size());
        for (const T& value : values) {
            data_[size_] = value;
            ++size_;
        }
    }

    DynamicArray(const DynamicArray& other) {
        reserve(other.capacity_);
        for (const T& value : other) {
            data_[size_] = value;
            ++size_;
        }
    }

    DynamicArray(DynamicArray&& other) noexcept
        : data_(std::move(other.data_)), size_(std::exchange(other.size_, 0)),
          capacity_(std::exchange(other.capacity_, 0)) {}

    DynamicArray& operator=(const DynamicArray& other) {
        if (this != &other) {
            DynamicArray copy(other);
            data_ = std::move(copy.data_);
            size_ = copy.size_;
            capacity_ = copy.capacity_;
        }
        return *this;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);
        }
        return *this;
    }

    ~DynamicArray() = default;

    void swap(DynamicArray& other) noexcept {
        data_.swap(other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }

    T* begin() noexcept { return data_.get(); }
    const T* begin() const noexcept { return data_.get(); }
    T* end() noexcept { return begin() + size_; }
    const T* end() const noexcept { return begin() + size_; }

    T& at(std::size_t index) {
        if (index >= size_) throw std::out_of_range("DynamicArray index");
        return data_[index];
    }

    const T& at(std::size_t index) const {
        if (index >= size_) throw std::out_of_range("DynamicArray index");
        return data_[index];
    }

    T& operator[](std::size_t index) { return at(index); }
    const T& operator[](std::size_t index) const { return at(index); }

    T& back() {
        if (empty()) throw std::out_of_range("Empty DynamicArray");
        return data_[size_ - 1];
    }

    const T& back() const {
        if (empty()) throw std::out_of_range("Empty DynamicArray");
        return data_[size_ - 1];
    }

    void reserve(std::size_t count) {
        if (count <= capacity_) return;
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::length_error("DynamicArray too large");

        auto next = std::make_unique<T[]>(count);
        for (std::size_t i = 0; i < size_; ++i) {
            next[i] = std::move(data_[i]);
        }
        data_ = std::move(next);
        capacity_ = count;
    }

    void resize(std::size_t count) {
        while (size_ > count) pop_back();

        reserve(count);
        size_ = count;
    }

    void push_back(T value) {
        if (size_ == capacity_) {
            if (capacity_ > std::numeric_limits<std::size_t>::max() / 2)
                throw std::length_error("DynamicArray too large");

            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        data_[size_] = std::move(value);
        ++size_;
    }

    void pop_back() {
        if (empty()) throw std::out_of_range("Empty DynamicArray");
        data_[size_ - 1] = T{};
        --size_;
    }

    void clear() {
        while (!empty()) pop_back();
    }
};
