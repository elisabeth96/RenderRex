#pragma once

#include <cassert>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>

namespace rr {

template <typename T, size_t N> class SmallVector {
    static_assert(std::is_trivial_v<T>, "SmallVector only supports trivial types");

    // Inline buffer for small size optimization
    alignas(T) unsigned char inline_buffer[sizeof(T) * N];
    T*     data_ptr; // Points to either inline_buffer or heap allocation
    size_t size_;
    size_t capacity_;

    bool is_using_inline_buffer() const {
        return reinterpret_cast<const unsigned char*>(data_ptr) == inline_buffer;
    }

public:
    SmallVector() : data_ptr(reinterpret_cast<T*>(inline_buffer)), size_(0), capacity_(N) {}

    SmallVector(size_t num, const T& value = T{})
        : data_ptr(reinterpret_cast<T*>(inline_buffer)), size_(num), capacity_(N) {
        if (num > N) {
            // Need heap allocation
            capacity_ = num;
            data_ptr  = static_cast<T*>(std::malloc(sizeof(T) * num));
            if (!data_ptr) {
                throw std::bad_alloc();
            }
        }

        // Fill with the value
        for (size_t i = 0; i < num; ++i) {
            data_ptr[i] = value;
        }
    }

    SmallVector(std::initializer_list<T> init)
        : data_ptr(reinterpret_cast<T*>(inline_buffer)), size_(init.size()), capacity_(N) {

        if (init.size() > N) {
            // Need heap allocation
            capacity_ = init.size();
            data_ptr  = static_cast<T*>(std::malloc(sizeof(T) * init.size()));
            if (!data_ptr) {
                throw std::bad_alloc();
            }
        }

        // Copy elements from initializer list
        std::memcpy(data_ptr, init.begin(), sizeof(T) * init.size());
    }

    ~SmallVector() {
        if (!is_using_inline_buffer()) {
            std::free(data_ptr);
        }
    }

    // Rest of the implementation remains the same...
    // [Previous implementation code continues here...]

    // Move constructor
    SmallVector(SmallVector&& other) noexcept : size_(other.size_), capacity_(other.capacity_) {
        if (other.is_using_inline_buffer()) {
            data_ptr = reinterpret_cast<T*>(inline_buffer);
            std::memcpy(inline_buffer, other.inline_buffer, sizeof(T) * size_);
        } else {
            data_ptr       = other.data_ptr;
            other.data_ptr = reinterpret_cast<T*>(other.inline_buffer);
        }
        other.size_     = 0;
        other.capacity_ = N;
    }

    // Move assignment
    SmallVector& operator=(SmallVector&& other) noexcept {
        if (this != &other) {
            if (!is_using_inline_buffer()) {
                std::free(data_ptr);
            }

            size_     = other.size_;
            capacity_ = other.capacity_;

            if (other.is_using_inline_buffer()) {
                data_ptr = reinterpret_cast<T*>(inline_buffer);
                std::memcpy(inline_buffer, other.inline_buffer, sizeof(T) * size_);
            } else {
                data_ptr       = other.data_ptr;
                other.data_ptr = reinterpret_cast<T*>(other.inline_buffer);
            }
            other.size_     = 0;
            other.capacity_ = N;
        }
        return *this;
    }

    // Copy constructor
    SmallVector(const SmallVector& other)
        : size_(other.size_), capacity_(other.is_using_inline_buffer() ? N : other.capacity_) {
        if (other.is_using_inline_buffer()) {
            // Copy to our inline buffer
            data_ptr = reinterpret_cast<T*>(inline_buffer);
            std::memcpy(inline_buffer, other.inline_buffer, sizeof(T) * size_);
        } else {
            // Allocate heap memory and copy
            data_ptr = static_cast<T*>(std::malloc(sizeof(T) * other.capacity_));
            if (!data_ptr) {
                throw std::bad_alloc();
            }
            std::memcpy(data_ptr, other.data_ptr, sizeof(T) * size_);
        }
    }

    // Copy assignment operator
    SmallVector& operator=(const SmallVector& other) {
        if (this != &other) {
            // Free existing heap allocation if we have one
            if (!is_using_inline_buffer()) {
                std::free(data_ptr);
            }

            size_ = other.size_;

            if (other.is_using_inline_buffer()) {
                // Copy to our inline buffer
                data_ptr  = reinterpret_cast<T*>(inline_buffer);
                capacity_ = N;
                std::memcpy(inline_buffer, other.inline_buffer, sizeof(T) * size_);
            } else {
                // Allocate heap memory and copy
                capacity_ = other.capacity_;
                data_ptr  = static_cast<T*>(std::malloc(sizeof(T) * other.capacity_));
                if (!data_ptr) {
                    throw std::bad_alloc();
                }
                std::memcpy(data_ptr, other.data_ptr, sizeof(T) * size_);
            }
        }
        return *this;
    }

    void push_back(const T& value) {
        if (size_ == capacity_) {
            size_t new_capacity = capacity_ * 2;

            if (is_using_inline_buffer()) {
                // First expansion from inline buffer to heap
                T* new_buffer = static_cast<T*>(std::malloc(sizeof(T) * new_capacity));
                if (!new_buffer) {
                    throw std::bad_alloc();
                }
                std::memcpy(new_buffer, inline_buffer, sizeof(T) * size_);
                data_ptr = new_buffer;
            } else {
                // Use realloc for subsequent expansions
                T* new_buffer = static_cast<T*>(std::realloc(data_ptr, sizeof(T) * new_capacity));
                if (!new_buffer) {
                    throw std::bad_alloc();
                }
                data_ptr = new_buffer;
            }

            capacity_ = new_capacity;
        }

        data_ptr[size_] = value;
        ++size_;
    }

    void pop_back() {
        if (size_ > 0) {
            --size_;
        }
    }

    T& operator[](size_t index) {
        assert(index < size_);
        return data_ptr[index];
    }
    const T& operator[](size_t index) const {
        assert(index < size_);
        return data_ptr[index];
    }

    T& at(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data_ptr[index];
    }

    const T& at(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data_ptr[index];
    }

    size_t size() const {
        return size_;
    }
    size_t capacity() const {
        return capacity_;
    }
    bool empty() const {
        return size_ == 0;
    }

    T* begin() {
        return data_ptr;
    }
    T* end() {
        return data_ptr + size_;
    }
    const T* begin() const {
        return data_ptr;
    }
    const T* end() const {
        return data_ptr + size_;
    }
};

} // namespace rr