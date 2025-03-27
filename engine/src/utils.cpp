#include "types.h"
#include <cstdlib>
#include <cstdio>

template <typename T>
class Stack {
    private:
        T* data;
        u32 size;
        u32 capacity;
    public:
        Stack(u32 capacity) {
            this->capacity = capacity;
            this->size = 0;
            this->data = new T[capacity];
        }

        ~Stack() {
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

void assert(bool condition, const char* message) {
    if (!condition) {
        printf("Assertion failed: %s\n", message);
        exit(1);
    }
}
