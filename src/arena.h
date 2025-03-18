#ifndef _ARENA_H
#define _ARENA_H

#include <format>

#include "core.h"
#include "platform.h"

#define align_pow2_up(addr, alignment) (((addr) + ((alignment) - 1)) & ~((alignment) - 1))

struct Arena {
    u8* base;
    size_t size;
    size_t pos;
};

struct Region {
    Arena* arena;
    size_t pop_pos;
};

static Arena arena_init(u8* backing_mem, size_t size) {
    return {
        .base = backing_mem,
        .size = size,
        .pos = 0,
    };
}

static inline u8* arena_push(Arena* arena, size_t size, size_t alignment) { 
    assert((alignment & (alignment - 1)) == 0 && "Alignment must be a power of 2");
    u8* aligned_addr = (u8*)align_pow2_up((size_t)(arena->base) + arena->pos, alignment);
    if (aligned_addr + size >= arena->base + arena->size) {
        platform::fatal(std::format("Arena ran out of memory on allocation with size: {}. Arena size: {}", size, arena->size));
    }

    arena->pos += (size_t)(aligned_addr - arena->base) + size;
    return aligned_addr;
}

static inline void arena_pop_to(Arena* arena, size_t pos) {
    arena->pos = pos;
}

[[nodiscard]] static inline Region arena_begin_region(Arena* arena) {
    return {
        .arena = arena,
        .pop_pos = arena->pos,
    };
}

static inline void region_reset(Region* region) {
    arena_pop_to(region->arena, region->pop_pos);
}

template<typename T>
static inline T* arena_create_aligned(Arena* arena, size_t alignment) {
    return (T*)arena_push(arena, sizeof(T), alignment);
}

template<typename T>
static inline T* arena_create(Arena* arena) {
    return arena_create_aligned<T>(arena, alignof(T) > 8 ? alignof(T) : 8);
}


template<typename T>
static inline Slice<T> arena_alloc_aligned(Arena* arena, size_t count, size_t alignment) {
    auto ptr = (T*)arena_push(arena, sizeof(T) * count, alignment);
    return {
        .ptr = ptr,
        .len = count,
    };
}

template<typename T>
static inline Slice<T> arena_alloc(Arena* arena, size_t count) {
    return arena_alloc_aligned<T>(arena, count, alignof(T) > 8 ? alignof(T) : 8);
}


#endif
