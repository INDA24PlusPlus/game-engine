#include "Sampler.h"

#include <cassert>
#include <glad/glad.h>
#include <utility>

namespace engine {

static u32 filter_to_opengl(Sampler::Filter filter) {
    switch (filter) {
        case Sampler::Filter::linear: return GL_LINEAR;
        case Sampler::Filter::nearest: return GL_NEAREST;
    }
}

static u32 address_mode_to_opengl(Sampler::AddressMode mode) {
    switch (mode) {
        case Sampler::AddressMode::clamp_to_edge: return GL_CLAMP_TO_EDGE;
        case Sampler::AddressMode::mirrored_repeat: return GL_MIRRORED_REPEAT;
        case Sampler::AddressMode::repeat: return GL_REPEAT;
    }
}

static u32 filter_and_mipmap_mode_to_opengl(Sampler::Filter filter,
                                            Sampler::MipmapMode mipmap_mode) {
    if (mipmap_mode == Sampler::MipmapMode::linear) {
        if (filter == Sampler::Filter::linear) {
            return GL_LINEAR_MIPMAP_LINEAR;
        } else if (filter == Sampler::Filter::nearest) {
            return GL_NEAREST_MIPMAP_LINEAR;
        }
    } else if (mipmap_mode == Sampler::MipmapMode::nearest) {
        if (filter == Sampler::Filter::linear) {
            return GL_LINEAR_MIPMAP_NEAREST;
        } else if (filter == Sampler::Filter::nearest) {
            return GL_NEAREST_MIPMAP_NEAREST;
        }
    }

    assert(0);
    std::unreachable();
}

void Sampler::init(Filter mag_filter,
                   Filter min_filter,
                   MipmapMode mipmap_mode,
                   AddressMode address_mode_u,
                   AddressMode address_mode_v,
                   AddressMode address_mode_w,
                   f32 max_anisotropy) {

    u32 gl_mag_filter = filter_to_opengl(mag_filter);
    u32 gl_min_filter = filter_and_mipmap_mode_to_opengl(min_filter, mipmap_mode);
    u32 gl_address_mode_u = address_mode_to_opengl(address_mode_u);
    u32 gl_address_mode_v = address_mode_to_opengl(address_mode_v);
    u32 gl_address_mode_w = address_mode_to_opengl(address_mode_w);

    glCreateSamplers(1, &m_handle);
    glSamplerParameteri(m_handle, GL_TEXTURE_MAG_FILTER, gl_mag_filter);
    glSamplerParameteri(m_handle, GL_TEXTURE_MIN_FILTER, gl_min_filter);
    glSamplerParameteri(m_handle, GL_TEXTURE_WRAP_S, gl_address_mode_u);
    glSamplerParameteri(m_handle, GL_TEXTURE_WRAP_T, gl_address_mode_v);
    glSamplerParameteri(m_handle, GL_TEXTURE_WRAP_R, gl_address_mode_w);
    glSamplerParameterf(m_handle, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);
}

}; // namespace engine