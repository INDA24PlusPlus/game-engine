#include "gui.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <format>
#include <glm/gtc/type_ptr.hpp>

#include "GLFW/glfw3.h"
#include "glm/ext/quaternion_geometric.hpp"
#include "state.h"
#include "engine/utils/logging.h"

// NOTE: This is all debug stuff. Feel free to use global data.

void gui::init(GLFWwindow* window, f32 content_scale) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    u32 font_size = (u32)(16 - 3  * (content_scale - 1));
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Medium.ttf", font_size);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();
}

void gui::deinit() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void gui::build(State &state) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    char fmt_buf[1024];

    ImGui::NewFrame();

    ImGui::SetNextWindowPos({ ImGui::GetFontSize(), state.fb_height - 4.0f * ImGui::GetFontSize() });
    ImGui::Begin("Metrics", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    f32 avg_delta_time = 0.0f;
    for (f32 v : state.prev_delta_times) {
        avg_delta_time += v;
    }
    avg_delta_time *= (1.0f / (f32)state.prev_delta_times.size());
    ImGui::Text("%d FPS (%.3f ms)", (int)(1.0f / avg_delta_time), avg_delta_time * 1000.0f);
    ImGui::End();

    ImGui::Begin("Camera", nullptr);
    ImGui::DragFloat4("Camera orientation", (f32*)&state.camera.m_orientation);
    ImGui::DragFloat3("Camera position", (f32*)&state.camera.m_pos);
    ImGui::DragFloat("Camera position", (f32*)&state.camera.m_speed);
    ImGui::End();

    static i32 selected_node = 0;
    ImGui::Begin("Node Editor", nullptr);
    ImGui::SliderInt("Selected Node", &selected_node, 0, state.scene.m_nodes.size() - 1);
    auto& node = state.scene.m_nodes[selected_node];
    ImGui::DragFloat3("Translation", (f32*)glm::value_ptr(node.translation));
    ImGui::DragFloat4("Rotation", (f32*)glm::value_ptr(node.rotation), 0.05f);
    node.rotation = glm::normalize(node.rotation);
    ImGui::DragFloat3("Scale", (f32*)glm::value_ptr(node.scale));
    ImGui::End();

    if (ImGui::Begin("Renderer settings", nullptr)) {
        static bool vsync = true;
        if (ImGui::Checkbox("VSync", &vsync)) {
            glfwSwapInterval(vsync);
            INFO("Changed vsync status");
        }

        static int texture_filtering_rate = 1;
        auto msg_len = std::format_to_n(fmt_buf, sizeof(fmt_buf) - 1, "{}x", texture_filtering_rate);
        fmt_buf[msg_len.size] = 0;
        if (ImGui::BeginCombo("Texture Filtering", fmt_buf)) {
            for (int i = 1; i <= state.renderer.get_max_texture_filtering_level(); i <<= 1) {
                auto msg_len = std::format_to_n(fmt_buf, sizeof(fmt_buf) - 1, "{}x", i);
                fmt_buf[msg_len.size] = 0;
                bool selected = i == texture_filtering_rate;
                if (ImGui::Selectable(fmt_buf, selected)) {
                    texture_filtering_rate = i;
                    state.renderer.set_texture_filtering_level((f32)i);
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    ImGui::End();

}

void gui::render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
