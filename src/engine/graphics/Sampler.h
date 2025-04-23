#pragma once

#include "../core.h"

namespace engine {

class Sampler {
  public:
    enum class Filter {
        nearest,
        linear,
    };

    enum class MipmapMode {
        nearest,
        linear,
    };

    enum class AddressMode {
        repeat,
        clamp_to_edge,
        mirrored_repeat,
    };

    void init(Filter mag_filter,
              Filter min_filter,
              MipmapMode mipmap_mode,
              AddressMode address_mode_u,
              AddressMode address_mode_v,
              AddressMode address_mode_w,
              f32 max_anisotropy);

    u32 m_handle;
    Filter m_mag_filter;
    Filter m_min_filter;
    MipmapMode m_mipmap_mode;
    AddressMode m_address_mode_u;
    AddressMode m_address_mode_v;
    AddressMode m_address_mode_w;
    f32 m_max_anisotropy;
};

}; // namespace engine