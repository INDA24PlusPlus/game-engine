#pragma once

#include <math.h>
typedef struct {
    float x, y, z;
} Vec3;

#define vec3(X, Y, Z) ((Vec3) {.x=X, .y=Y, .z=Z})
#define vec3_zero vec3(0.0f, 0.0f, 0.0f)
#define vec3_length_threshold 0.001f

#define VEC3_FMT "[%.3f, %.3f, %.3f]"
#define EXPAND_VEC3(v) (v).x, (v).y, (v).z
#define print_vec3(STR, v) printf(STR VEC3_FMT "\n", EXPAND_VEC3(v))

static inline float vec3_dot(const Vec3 a, const Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float vec3_length(const Vec3 vec) {
    return sqrtf(vec3_dot(vec, vec));
}

static inline Vec3 vec3_normalize(const Vec3 vec) {
    float length = vec3_length(vec);
    if (vec3_length_threshold < length) {
        return vec3(vec.x / length, vec.y / length, vec.z / length);
    }

    return vec3_zero;
}

static inline Vec3 vec3_add(const Vec3 a, const Vec3 b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline Vec3 vec3_subtract(const Vec3 a, const Vec3 b) {
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline Vec3 vec3_cross(const Vec3 a, const Vec3 b) {
    return vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
}
