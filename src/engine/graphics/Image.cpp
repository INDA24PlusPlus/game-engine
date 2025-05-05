#include "Image.h"

#include <cassert>
#include <glad/glad.h>

namespace engine {

static u32 to_opengl_format(ImageInfo::Format format) {
    switch (format) {
        case ImageInfo::Format::BC7_RGBA: return GL_COMPRESSED_RGBA_BPTC_UNORM;
        case ImageInfo::Format::BC7_RGBA_SRGB: return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
        case ImageInfo::Format::BC5_RG: return GL_COMPRESSED_RG_RGTC2;

        case ImageInfo::Format::RGBA16F: return GL_RGBA16F;
        case ImageInfo::Format::RGB16F: return GL_RGB16F;

        case ImageInfo::Format::RGBA32F: return GL_RGBA32F;
        case ImageInfo::Format::RGB32F: return GL_RGB32F;
        case ImageInfo::Format::RGB8_UNORM: return GL_RGB8;

        default: assert(0);
    }
}

static u32 to_opengl_format_external(ImageInfo::Format format) {
    switch (format) {
        // Compressed formats have the same value
        case ImageInfo::Format::BC7_RGBA: return GL_COMPRESSED_RGBA_BPTC_UNORM;
        case ImageInfo::Format::BC7_RGBA_SRGB: return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
        case ImageInfo::Format::BC5_RG: return GL_COMPRESSED_RG_RGTC2;

        // Uncompressed formats we just provide how many channels and in what order.
        case ImageInfo::Format::RGB8_UNORM:
        case ImageInfo::Format::RGB16F:
        case ImageInfo::Format::RGB32F: return GL_RGB;

        case ImageInfo::Format::RGBA32F:
        case ImageInfo::Format::RGBA16F: return GL_RGBA;

        default: assert(0);
    }
}

static u32 to_opengl_type(ImageInfo::Format format) {
    switch (format) {
        case ImageInfo::Format::RGB8_UNORM:
        case ImageInfo::Format::BC5_RG:
        case ImageInfo::Format::BC7_RGBA:
        case ImageInfo::Format::BC7_RGBA_SRGB: return GL_UNSIGNED_BYTE;

        case ImageInfo::Format::RGBA16F:
        case ImageInfo::Format::RGB16F: return GL_HALF_FLOAT;

        case ImageInfo::Format::RGBA32F: return GL_FLOAT;
        case ImageInfo::Format::RGB32F: return GL_FLOAT;

        default: assert(0);
    }
}

void Image::init(const ImageInfo &info) {
    if (info.is_cubemap) assert(info.num_faces == 6);

    u32 texture_target = info.is_cubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    u32 format = to_opengl_format(info.format);

    glCreateTextures(texture_target, 1, &m_handle);
    glTextureStorage2D(m_handle, info.num_levels, format, info.width, info.height);
    m_info = info;
}

void Image::deinit() {
    glDeleteTextures(1, &m_handle);
}

void Image::upload(u8 *data) {
    if (m_info.is_cubemap) {
        if (m_info.is_compressed) {
            upload_cubemap_compressed(data);
        } else {
            upload_cubemap(data);
        }

        return;
    }

    if (m_info.is_compressed) {
        upload_compressed(data);
        return;
    }

    u32 format = to_opengl_format_external(m_info.format);
    u32 opengl_type = to_opengl_type(m_info.format);

    for (u32 level = 0; level < m_info.num_levels; ++level) {
        u32 level_offset = m_info.level_offset(0, level);
        u32 level_width = m_info.width >> level;
        u32 level_height = m_info.height >> level;
        u8 *level_data = data + level_offset;
        glTextureSubImage2D(m_handle,
                            level,
                            0,
                            0,
                            level_width,
                            level_height,
                            format,
                            opengl_type,
                            level_data);
    }
}

void Image::upload_compressed(u8 *data) {
    u32 format = to_opengl_format_external(m_info.format);

    for (u32 level = 0; level < m_info.num_levels; ++level) {
        u32 level_offset = m_info.level_offset(0, level);
        u32 level_size = m_info.level_size(level);
        u32 level_width = m_info.width >> level;
        u32 level_height = m_info.height >> level;
        u8 *level_data = data + level_offset;

        glCompressedTextureSubImage2D(m_handle,
                                      level,
                                      0,
                                      0,
                                      level_width,
                                      level_height,
                                      format,
                                      level_size,
                                      level_data);
    }
}

void Image::upload_cubemap(u8 *data) {
    assert(m_info.is_cubemap && !m_info.is_compressed);

    u32 format = to_opengl_format_external(m_info.format);
    u32 opengl_type = to_opengl_type(m_info.format);

    for (u32 face = 0; face < m_info.num_faces; ++face) {
        for (u32 level = 0; level < m_info.num_levels; ++level) {
            u32 level_offset = m_info.level_offset(face, level);
            u32 level_width = m_info.width >> level;
            u32 level_height = m_info.height >> level;
            u8 *level_data = data + level_offset;
            glTextureSubImage3D(m_handle,
                                level,
                                0,
                                0,
                                face,
                                level_width,
                                level_height,
                                1,
                                format,
                                opengl_type,
                                level_data);
        }
    }
}

void Image::upload_cubemap_compressed(u8 *data) {
    assert(m_info.is_cubemap && m_info.is_compressed);
    u32 format = to_opengl_format_external(m_info.format);

    for (u32 face = 0; face < m_info.num_faces; ++face) {
        for (u32 level = 0; level < m_info.num_levels; ++level) {
            u32 level_offset = m_info.level_offset(face, level);
            u32 level_size = m_info.level_size(level);
            u32 level_width = m_info.width >> level;
            u32 level_height = m_info.height >> level;
            u8 *level_data = data + level_offset;

            glCompressedTextureSubImage3D(m_handle,
                                          level,
                                          0,
                                          0,
                                          face,
                                          level_width,
                                          level_height,
                                          1,
                                          format,
                                          level_size,
                                          level_data);
        }
    }
}

} // namespace engine
