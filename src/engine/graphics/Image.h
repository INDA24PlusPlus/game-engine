#pragma once

#include "../core.h"
#include "../Scene.h"

namespace engine {

class Image {
public: 
    void init(const ImageInfo& image_info);
    void deinit();
    void upload(u8* data);


    u32 m_handle;
    ImageInfo m_info;
private:
    void upload_compressed(u8* data);
    void upload_cubemap(u8* data);
    void upload_cubemap_compressed(u8* data);
};

}