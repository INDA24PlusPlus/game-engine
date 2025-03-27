#pragma once

#include "types.h"

template <typename T>
class Stack {
    public:
        Stack(u32 capacity);
        ~Stack();
        void push(T value);
        T pop();
        T peek();
        bool is_empty();
        u32 get_size();
        u32 get_capacity();
        void clear();
};

void assert(bool condition, const char* message);
