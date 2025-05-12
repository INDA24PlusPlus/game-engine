#include "GPURingBuffer.h"

#include <glad/glad.h>

#include <cstring>

#include "../utils/logging.h"


namespace engine {

void GPURingBuffer::init(u32 capacity, u32 num_frames_in_flight) {
    glCreateBuffers(1, &m_handle);
    u32 size = capacity * num_frames_in_flight;
    u32 flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glNamedBufferStorage(m_handle, size, nullptr, flags);
    void* ptr = glMapNamedBufferRange(m_handle, 0, size, flags);
    m_gpu_allocation = std::span((u8*)ptr, capacity * num_frames_in_flight);
    m_curr_slice = std::span(m_gpu_allocation.data(), capacity);
    m_capacity = capacity;
    m_num_frames_in_flight = num_frames_in_flight;
}

void GPURingBuffer::deinit() { glDeleteBuffers(1, &m_handle); }

u64 GPURingBuffer::get_current_region_offset() { return m_curr_frame * m_capacity; }

void GPURingBuffer::new_frame() {
    m_curr_frame = (m_curr_frame + 1) % m_num_frames_in_flight;
    m_curr_slice = std::span(&m_gpu_allocation[m_curr_frame * m_capacity], m_capacity);
    m_curr_pos = 0;
}

void GPURingBuffer::write(std::span<u8> bytes) {
    if (m_curr_pos + bytes.size() > m_capacity) {
        ERROR("GPU Ringbuffer ran out of space. Allocation size {} MB, size per frame {}",
              m_gpu_allocation.size() >> 20, m_capacity >> 20);
        assert(0);
    }
    std::memcpy(&m_curr_slice[m_curr_pos], bytes.data(), bytes.size());
    m_curr_pos += bytes.size();
}

}  // namespace engine