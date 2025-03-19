#ifndef _CORE_H
#define _CORE_H

#include <utility>

#include <utility>
#define assert(cond) do { if (!(cond)) __builtin_trap(); } while(0)
#define KB(n) (((uint64_t)(n)) << 10)
#define MB(n) (((uint64_t)(n)) << 20)

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#define asan_poison_memory_region(addr, size) \
    __asan_poison_memory_region((addr), (size))
#define asan_unpoison_memory_region(addr, size) \
   __asan_unpoison_memory_region((addr), (size))
#else
#define asan_poison_memory_region(addr, size) \
    ((void)(addr), (void)(size))
#define asan_unpoison_memory_region(addr, size) \
    ((void)(addr), (void)(size))
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u32 b32;

typedef float f32;
typedef double f64;


template <typename T>
constexpr void swap(T* a, T* b) {
    T tmp = *a;
    *a = *b;
    *b = tmp;
}

template <typename T>
constexpr T min(T a, T b) {
    return a < b ? a : b;
}

template <typename T>
constexpr T clamp(T value, T min, T max) {
    assert(min < max);

    if (value < min) {
        return min;
    } else if (value > max) {
        return max;
    }
    return value;
}

template <typename F>
struct Defer {
    Defer( F f ) : f( f ) {}
    ~Defer( ) { f( ); }
    F f;
};

template <typename F>
Defer<F> makeDefer( F f ) {
    return Defer<F>( f );
};

#define __defer( line ) defer_ ## line
#define _defer( line ) __defer( line )

struct defer_dummy { };
template<typename F>
Defer<F> operator+( defer_dummy, F&& f )
{
    return makeDefer<F>( std::forward<F>( f ) );
}

#define defer auto _defer( __LINE__ ) = defer_dummy( ) + [ & ]( )

template <typename T>
struct Slice { 
    T* ptr;
    size_t len;

    T& operator[](size_t index) {
#ifndef DISABLE_BOUNDS_CHECKING
        assert(index < len);
#endif
        return ptr[index];
    }
};

#endif
