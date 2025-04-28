#ifndef _CORE_H
#define _CORE_H

#include <cstdint>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint32_t b32;

typedef float f32;

template<typename Tag>
class TypedHandle {
private:
    u32 value;
public:
    explicit TypedHandle(uint32_t v = 0) : value(v) {}
    uint32_t get_value() const { return value; }
    bool is_valid() const { return value != 0xffffffff; }
    
    auto operator<=>(const TypedHandle&) const = default;
};

#endif