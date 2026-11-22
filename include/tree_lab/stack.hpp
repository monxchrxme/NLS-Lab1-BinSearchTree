#pragma once

#include "dynamic_array.hpp"

template <typename T>
class Stack {
private:
    DynamicArray<T> elements_;

public:
    Stack() = default;

    void push(const T& value) { 
        elements_.push_back(value); 
    }

    void push(T&& value) { 
        elements_.push_back(std::move(value)); 
    }

    void pop() { elements_.pop_back(); }

    T& top() { 
        return elements_.back(); 
    }

    const T& top() const { 
        return elements_.back(); 
    }

    bool empty() const noexcept { 
        return elements_.empty(); 
    }

    std::size_t size() const noexcept { 
        return elements_.size();
    }

    void clear() {
        elements_.clear(); 
    }
};
