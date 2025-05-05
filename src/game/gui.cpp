#include "gui.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>
#include <format>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#include "engine/scene/Node.h"
#include "engine/scene/Scene.h"
#include "engine/utils/logging.h"
#include "state.h"

#ifdef ENABLE_EDITOR

// For opening a file dialog.
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#define STRICT
#define NOMINMAX
#include <windows.h>
#endif

#include <json.hpp>
using json = nlohmann::json;
#endif

// NOTE: This is all debug stuff. Feel free to use global data.

void gui::init(GLFWwindow* window, f32 content_scale) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    u32 font_size = (u32)(16 - 3 * (content_scale - 1));
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Medium.ttf", font_size);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();
}

void gui::deinit() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

struct NodeEditor {
    u32 selected_node;
    bool edit_mode;

    bool show_prefab_popup;
    u32 prefab_popup_node;

    bool show_save_popup;
    u32 node_to_save;
};

static void draw_node(State& state, NodeEditor& editor, u32 node_index) {
    static char name_buffer[256];

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node_index == editor.selected_node) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool is_leaf = state.hierarchy.m_nodes[node_index].children.size() == 0;
    if (is_leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool opened;

    if (node_index == editor.selected_node && editor.edit_mode) {
        // Create a unique ID for this input field
        ImGui::PushID(node_index);

        // If we're just entering edit mode, copy the node name to the buffer
        if (ImGui::GetActiveID() != ImGui::GetID("node_name_input")) {
            std::string_view src = state.hierarchy.m_nodes[node_index].name;
            size_t copy_len = std::min(src.length(), sizeof(name_buffer) - 1);
            std::memcpy(name_buffer, src.data(), copy_len);
            name_buffer[copy_len] = '\0';
        }

        // Proper indentation for the edit field
        ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

        // Add the input text field
        if (ImGui::InputText(
                "##node_name_input", name_buffer, sizeof(name_buffer),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            // Update the node name when Enter is pressed
            state.hierarchy.m_nodes[node_index].name = name_buffer;
            editor.edit_mode = false;
        }

        // Exit edit mode if focus is lost or Escape is pressed
        if (ImGui::IsItemDeactivatedAfterEdit() ||
            (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsItemFocused())) {
            editor.edit_mode = false;
        }

        ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
        ImGui::PopID();

        // We still need to know if the node would be open in the tree
        opened = state.hierarchy.m_nodes[node_index].children.size() > 0 &&
                 (flags & ImGuiTreeNodeFlags_DefaultOpen);
    } else {
        opened = ImGui::TreeNodeEx(state.hierarchy.m_nodes[node_index].name.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            editor.selected_node = node_index;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                editor.edit_mode = true;
            }
        }

        // Right-click to open context menu
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            editor.selected_node = node_index;
            ImGui::OpenPopup("node_context_menu");
        }

        // Context menu
        if (editor.selected_node == node_index && ImGui::BeginPopup("node_context_menu")) {
            if (ImGui::MenuItem("Rename")) {
                editor.edit_mode = true;
            }

            if (ImGui::MenuItem("Save as Prefab")) {
                editor.show_save_popup = true;
                editor.node_to_save = node_index;
            }

            if (ImGui::MenuItem("Insert Prefab")) {
                editor.show_prefab_popup = true;
                editor.prefab_popup_node = node_index;
                ImGui::CloseCurrentPopup();
                ImGui::SetNextWindowPos(ImGui::GetMousePos());
            }

            if (ImGui::MenuItem("Add Child")) {
                u32 new_node_index = state.hierarchy.m_nodes.size();
                state.hierarchy.m_nodes.push_back({
                    .kind = engine::Node::Kind::node,
                    .name = std::format("Untitled Node {}", state.hierarchy.m_nodes.size()),
                    .rotation = glm::quat(1, 0, 0, 0),
                    .scale = glm::vec3(1),
                });

                state.hierarchy.m_nodes[node_index].children.push_back(new_node_index);

                if (is_leaf) {
                    opened = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
    }

    if (opened && !is_leaf) {
        for (u32 i = 0; i < state.hierarchy.m_nodes[node_index].children.size(); i++) {
            draw_node(state, editor, state.hierarchy.m_nodes[node_index].children[i]);
        }
        ImGui::TreePop();
    }
}

static void draw_prefab_selector(State& state, NodeEditor& editor) {
    if (!editor.show_prefab_popup) return;
    const auto& manifest = state.scene.m_manifest;
    static size_t prefab_selected = 0;

    ImGui::OpenPopup("prefab_selector");
    if (ImGui::BeginPopup("prefab_selector")) {
        if (ImGui::BeginCombo("Selected Prefab",
                              manifest.get_name_data(manifest.m_prefab_names[0]).data())) {
            for (size_t i = 0; i < manifest.m_prefab_names.size(); ++i) {
                bool selected = i == prefab_selected;
                auto name_data = manifest.get_name_data(manifest.m_prefab_names[i]).data();

                if (ImGui::Selectable(name_data, selected)) {
                    prefab_selected = i;

                    state.hierarchy.instantiate_prefab(state.scene, state.scene.m_prefabs[i],
                                                       engine::NodeHandle(editor.selected_node));

                    editor.show_prefab_popup = false;
                    ImGui::CloseCurrentPopup();
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Cancel")) {
            editor.show_prefab_popup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else {
        editor.show_prefab_popup = false;
    }
}

static void write_node_to_json(State& state, std::span<engine::ImmutableNode> immutable,
                               u32 node_index, json& nodes) {
    const auto& node = immutable[node_index];
    json json_node = json::object();
    json_node["name"] = state.scene.m_manifest.get_name_data(node.name);
    json_node["translation"] = {node.translation[0], node.translation[1], node.translation[2]};
    json_node["rotation"] = {node.rotation[0], node.rotation[1], node.rotation[2],
                             node.rotation[3]};
    json_node["scale"] = {node.scale[0], node.scale[1], node.scale[2]};

    if (node.mesh_index != UINT32_MAX) {
        json_node["mesh"] = state.scene.m_manifest.get_mesh_name_data(node.mesh_index);
    }

    if (node.num_children != 0) {
        json children = json::array();
        for (size_t i = 0; i < node.num_children; ++i) {
            children.push_back(node.child_index + i);
        }

        json_node["children"] = children;
        nodes.push_back(json_node);
        for (size_t i = 0; i < node.num_children; ++i) {
            write_node_to_json(state, immutable, node.child_index + i, nodes);
        }
    } else {
        nodes.push_back(json_node);
    }
}

static void save_as_prefab(State& state, u32 node_index, std::ofstream& out) {
    const auto& node = state.hierarchy.m_nodes[node_index];
    json prefab;

    if (node.name.size() == 0) {
        prefab["name"] = "unnamed";
    } else {
        prefab["name"] = node.name;
    }

    // The conversion to a immutable node hierarchy ensures that the root node is always at index 0
    // and that all childs have indices larger than their parents and thus it is trivial to
    // serialize as json.
    std::vector<engine::ImmutableNode> immutable;
    immutable.resize(1);
    state.hierarchy.to_immutable_internal(state.scene.m_manifest, immutable, 0, node_index);
    prefab["root"] = 0;

    json nodes = json::array();
    write_node_to_json(state, immutable, 0, nodes);
    prefab["nodes"] = nodes;

    out << prefab.dump(4);
}

static void draw_save_popup(State& state, NodeEditor& editor) {
    if (!editor.show_save_popup) return;
#ifdef ENABLE_EDITOR
#ifdef _WIN32
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0Text Files (*.txt)\0*.txt\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrTitle = L"Save Prefab";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"json";

    // Show the Save File Dialog
    if (GetSaveFileNameW(&ofn)) {
        auto file_path = std::wstring(szFile);
        std::ofstream out(file_path);
        if (out.is_open()) {
            save_as_prefab(state, editor.node_to_save, out);
        } else {
            WARN("Failed to open file while trying to save a prefab");
        }
    }

    editor.show_save_popup = false;
#else
    WARN(
        "Saving files for Linux not yet implemented TODO: Provide ImGUI fallback or do the proper "
        "thing and search for system file dialog");
#endif
#endif
}

static void draw_node_editor(State& state) {
    static NodeEditor editor;

    ImGui::Begin("Node Editor");
    if (state.hierarchy.m_nodes.size() != 0) {
        draw_node(state, editor, 0);
    }
    draw_prefab_selector(state, editor);
    draw_save_popup(state, editor);
    ImGui::End();

    ImGui::Begin("Node Properties", nullptr);
    if (state.hierarchy.m_nodes.size() != 0) {
        auto& node = state.hierarchy.m_nodes[editor.selected_node];
        ImGui::DragFloat3("Translation", (f32*)glm::value_ptr(node.translation));
        ImGui::DragFloat4("Rotation", (f32*)glm::value_ptr(node.rotation), 0.05f);
        node.rotation = glm::normalize(node.rotation);
        ImGui::DragFloat3("Scale", (f32*)glm::value_ptr(node.scale));

        static const char* preview_values[2] = {
            "Node",
            "Mesh",
        };

        if (ImGui::BeginCombo("Node kind", preview_values[(u32)node.kind])) {
            static int selected_kind = 0;
            for (int i = 0; i < 2; ++i) {
                bool selected = i == selected_kind;
                if (ImGui::Selectable(preview_values[i], selected)) {
                    selected_kind = i;
                    node.mesh_index = 0;
                    node.kind = (engine::Node::Kind)i;
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (node.kind == engine::Node::Kind::mesh) {
            size_t mesh_index = node.mesh_index;
            auto mesh_name = state.scene.m_manifest.m_mesh_names[mesh_index];
            auto mesh_name_data = state.scene.m_manifest.get_name_data(mesh_name).data();

            if (ImGui::BeginCombo("Mesh", mesh_name_data)) {
                for (size_t i = 0; i < state.scene.m_meshes.size(); ++i) {
                    bool selected = i == mesh_index;
                    auto selected_name = state.scene.m_manifest.m_mesh_names[i];
                    if (ImGui::Selectable(
                            state.scene.m_manifest.get_name_data(selected_name).data(), selected)) {
                        node.mesh_index = i;
                        mesh_index = i;
                    }

                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }
    }
    ImGui::End();
}

void gui::build(State& state) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    char fmt_buf[1024];

    ImGui::NewFrame();

    ImGui::SetNextWindowPos({ImGui::GetFontSize(), state.fb_height - 4.0f * ImGui::GetFontSize()});
    ImGui::Begin("Metrics", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_AlwaysAutoResize);
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

    draw_node_editor(state);

    if (ImGui::Begin("Renderer settings", nullptr)) {
        static bool vsync = true;
        if (ImGui::Checkbox("VSync", &vsync)) {
            glfwSwapInterval(vsync);
            INFO("Changed vsync status");
        }

        static int texture_filtering_rate = 1;
        auto msg_len =
            std::format_to_n(fmt_buf, sizeof(fmt_buf) - 1, "{}x", texture_filtering_rate);
        fmt_buf[msg_len.size] = 0;
        if (ImGui::BeginCombo("Texture Filtering", fmt_buf)) {
            for (int i = 1; i <= state.renderer.get_max_texture_filtering_level(); i <<= 1) {
                auto msg_len = std::format_to_n(fmt_buf, sizeof(fmt_buf) - 1, "{}x", i);
                fmt_buf[msg_len.size] = 0;
                bool selected = i == texture_filtering_rate;
                if (ImGui::Selectable(fmt_buf, selected)) {
                    texture_filtering_rate = i;
                    state.renderer.set_texture_filtering_level(state.scene, (f32)i);
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
