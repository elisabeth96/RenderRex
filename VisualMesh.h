#pragma once

#include "Drawable.h"
#include "InstancedMesh.h"
#include "Mesh.h"
#include "Property.h"
#include "Renderer.h"

#include <array>
#include <imgui.h>
#include <unordered_map>

namespace rr {

struct VisualMeshUniforms {
    glm::mat4x4 projection_matrix;
    glm::mat4x4 view_matrix;
    glm::mat4x4 model_matrix = glm::mat4x4(1.0f);
    glm::vec4   wireframe_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.00f);
    glm::ivec4  show_wireframe  = glm::ivec4(1, 0, 0, 0);
};

static_assert(sizeof(VisualMeshUniforms) % 16 == 0);

struct VisualMeshVertexAttributes {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 bary;
    glm::vec3 edge_mask;
    glm::vec3 color;
};

class VisualMesh : public Drawable {
public:
    VisualMesh(const Mesh& mesh, const Renderer& renderer);
    ~VisualMesh() override;

    void release();

    void configure_render_pipeline();

    void draw(WGPURenderPassEncoder render_pass) override;

    void on_camera_update() override;

    void update_ui(std::string name, int index) override;

    void set_transform(const glm::mat4& transform) override;

    const glm::mat4* get_transform() const override;

    FaceVectorProperty* add_face_vectors(std::string_view name, const std::vector<glm::vec3>& vectors);

    FaceColorProperty* add_face_colors(std::string_view name, const std::vector<glm::vec3>& colors);

    void update_face_colors() {
        bool using_face_property = false;
        for (auto& [name, prop] : m_color_properties) {
            if (prop->is_enabled()) {
                using_face_property                    = true;
                const std::vector<glm::vec3>& colors = prop->get_colors();

                size_t num_attributes = m_vertex_attributes.size();
                for (size_t i = 0; i < num_attributes; ++i) {
                    m_vertex_attributes[i].color = colors[i / 3];
                }
                break;
            }
        }

        if (!using_face_property) {
            size_t num_attributes = m_vertex_attributes.size();
            for (size_t i = 0; i < num_attributes; ++i) {
                m_vertex_attributes[i].color = m_mesh_color;
            }
        }
        m_attributes_dirty = true;
    }

    Mesh m_mesh;

private:
    WGPUBuffer         m_vertex_buffer  = nullptr;
    WGPUBuffer         m_uniform_buffer = nullptr;
    WGPUBindGroup      m_bind_group     = nullptr;
    WGPURenderPipeline m_pipeline       = nullptr;

    bool               m_uniforms_dirty = false;
    VisualMeshUniforms m_uniforms;

    bool      m_show_wireframe = true;
    glm::vec3 m_mesh_color     = glm::vec3(0.45f, 0.55f, 0.60f);

    bool                                    m_attributes_dirty = false;
    std::vector<VisualMeshVertexAttributes> m_vertex_attributes;

    std::unordered_map<std::string, std::unique_ptr<FaceVectorProperty>> m_vector_properties;
    std::unordered_map<std::string, std::unique_ptr<FaceColorProperty>>  m_color_properties;
};

class VisualPointCloud : public Drawable {
public:
    VisualPointCloud(const std::vector<glm::vec3>& positions, const Renderer& renderer);

    void draw(WGPURenderPassEncoder render_pass) override {
        m_spheres->draw(render_pass);
    };

    void on_camera_update() override {
        m_spheres->on_camera_update();
    };

    void set_color(const glm::vec3& color) {
        m_spheres->set_color(color);
        m_spheres->upload_instance_data();
    };

    void set_radius(float radius) {
        float                      scale         = radius / m_init_radius;
        std::vector<InstanceData>& instance_data = m_spheres->get_instance_data();
        for (auto& s : instance_data) {
            s.transform[0][0] = scale;
            s.transform[1][1] = scale;
            s.transform[2][2] = scale;
        }
        m_spheres->upload_instance_data();
    };

    void update_ui(std::string name, int index) override {
        ImGui::Checkbox(name.c_str(), &m_visible);
        if (m_visible) {
            std::string label_radius = "scale radius ##" + std::to_string(index);
            ImGui::SliderFloat(label_radius.c_str(), &m_radius, 0.5f, 10.5f);
            std::string label_color = "change color ##" + std::to_string(index);
            ImGui::ColorEdit3(label_color.c_str(), (float*)&m_color); // Edit 3 floats representing a color
            glm::vec3 newColor(m_color.x, m_color.y, m_color.z);
            set_color(newColor);
            set_radius(m_radius * m_init_radius);
        } else {
            // Handle the case when the checkbox is unchecked
            set_radius(0.0f);
        }
    }

    std::unique_ptr<InstancedMesh> m_spheres;
    float                          m_init_radius = 0.001f;
    // for Imgui interface:
    ImVec4 m_color  = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    float  m_radius = 1.0f;
};

class VisualLineNetwork : public Drawable {
public:
    VisualLineNetwork(const std::vector<glm::vec3>& positions, const std::vector<std::pair<int, int>>& lines,
                      const Renderer& renderer);

    void draw(WGPURenderPassEncoder render_pass) override {
        m_lines->draw(render_pass);
    }

    void on_camera_update() override {
        m_lines->on_camera_update();
    }

    void set_color(const glm::vec3& color) {
        m_lines->set_color(color);
        m_lines->upload_instance_data();
    };

    /* void set_radius(float radius) {
        float                      scale         = radius;
        std::vector<InstanceData>& instance_data = m_lines->get_instance_data();
        for (auto& s : instance_data) {
            s.transform[0][0] = scale;
            s.transform[1][1] = scale;
            s.transform[2][2] = scale;
        }
        m_lines->upload_instance_data();
    };*/

    void update_ui(std::string name, int index) override {
        ImGui::Checkbox(name.c_str(), &m_visible);
        if (m_visible) {
            // std::string label_radius = "set radius ##" + std::to_string(index);
            // ImGui::SliderFloat(label_radius.c_str(), &m_radius, 0.5f, 10.5f);
            std::string label_color = "change color ##" + std::to_string(index);
            ImGui::ColorEdit3(label_color.c_str(), (float*)&m_color); // Edit 3 floats representing a color
            glm::vec3 newColor(m_color.x, m_color.y, m_color.z);
            set_color(newColor);
            // set_radius(m_radius * m_init_radius);
        } else {
            // Handle the case when the checkbox is unchecked
            // set_radius(0.0f);
        }
    }

    bool m_visible = true;
    std::unique_ptr<InstancedMesh> m_lines;
    float                          m_radius = 0.01f;
    glm::vec3                      m_color  = glm::vec3(0.45f, 0.55f, 0.60f);
};
} // namespace rr