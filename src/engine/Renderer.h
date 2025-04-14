#ifndef _RENDERER_H
#define _RENDERER_H

#include <glm/glm.hpp>
#include "Scene.h"
#include "Camera.h"
#include "core.h"

namespace engine {

class Renderer {
   public:
    typedef void* (*LoadProc)(const char* name);
    Renderer(LoadProc load_proc);
    Renderer() = delete;

    void make_resources_for_scene(const Scene& scene);

    // Will go away.
    void clear();
    void begin_pass(const Camera& camera, u32 width, u32 height);
    void end_pass();
    void draw_mesh(const Scene& scene, MeshHandle mesh,
                   const glm::mat4& transform = glm::mat4(1.0f));

   private:
    u32 load_shader(const char* path, u32 shader_type);

    bool m_scene_loaded;
    bool m_pass_in_progress;
    u32 m_vao;
    u32 m_ibo;
    u32 m_vbo;
    u32 m_fshader;
    u32 m_vshader;
    u32 m_pipeline;
};

}  // namespace engine

#endif
