#pragma once

#include "../core.h"

#include <glm/glm.hpp>
#include <span>

namespace engine {

class GPURingBuffer {
public:
    void init(u32 capacity, u32 num_frames_in_flight);
    void deinit();
    void write(std::span<u8> bytes);
    void new_frame();
    u64 get_current_region_offset();
    u32 m_handle;
    u32 m_capacity;
private:
    u32 m_num_frames_in_flight;
    u32 m_curr_frame;
    u32 m_curr_pos;
    std::span<u8> m_curr_slice;
    std::span<u8> m_gpu_allocation;
};


};
