#pragma once

#include "types.h"
template <typename T>
class FIFO {
    private:
        T* data;
        u32 size;
        u32 capacity;
    public:
        FIFO(u32 capacity) {
            this->capacity = capacity;
            this->size = 0;
            this->data = new T[capacity];
        }

        ~FIFO() {
            delete[] data;
        }

        void push(T value) {
            if (size < capacity) {
                data[size++] = value;
            }
        }

        T pop() {
            if (size > 0) {
                return data[--size];
            }
            return T();
        }

        T peek() {
            if (size > 0) {
                return data[size - 1];
            }
            return T();
        }

        bool is_empty() {
            return size == 0;
        }

        u32 get_size() {
            return size;
        }

        u32 get_capacity() {
            return capacity;
        }

        void clear() {
            size = 0;
        }
};
