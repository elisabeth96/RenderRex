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

struct VisualMeshOptions {
    float show_wireframe = 1.0f;
    float opacity        = 1.0f;
    float show_mesh      = 1.0f;
    float padding        = 0.0f;
};

struct VisualMeshUniforms {
    glm::mat4x4       projection_matrix;
    glm::mat4x4       view_matrix;
    glm::mat4x4       model_matrix    = glm::mat4x4(1.0f);
    glm::vec4         wireframe_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.00f);
    VisualMeshOptions options;
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

    void update_face_colors();

    void set_hide_mesh(bool hide) {
        m_uniforms.options.show_mesh = hide ? 0.0f : 1.0f;
        m_uniforms_dirty             = true;
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
        if (!m_visible)
            return;
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
        if (!m_visible)
            return;
        m_line_mesh->draw(render_pass);
        m_vertices_mesh->draw(render_pass);
    }

    void on_camera_update() override {
        m_line_mesh->on_camera_update();
        m_vertices_mesh->on_camera_update();
    }

    void set_color(const glm::vec3& color) {
        m_line_mesh->set_color(color);
        m_vertices_mesh->set_color(color);
        m_line_mesh->upload_instance_data();
        m_vertices_mesh->upload_instance_data();
    };

    void set_radius(float radius) {
        m_radius = radius;
        compute_transforms();
    };

    void compute_transforms() {
        std::vector<glm::mat4x4> transforms;

        for (auto& line : m_lines) {
            glm::vec3 p1 = m_positions[line.first];
            glm::vec3 p2 = m_positions[line.second];
            glm::vec3 p  = (p1 + p2) / 2.0f;
            glm::vec3 d  = p2 - p1;
            float     l  = glm::length(d);

            if (l < 1e-6f)
                continue; // Skip zero-length lines

            d = d / l; // Normalize
            glm::vec3 default_dir(0.0f, 1.0f, 0.0f);

            glm::mat4 scaling     = glm::scale(glm::mat4(1), glm::vec3(m_radius, l, m_radius));
            glm::mat4 translation = glm::translate(glm::mat4(1), p);

            // Handle rotation more robustly
            float     dot = glm::dot(default_dir, d);
            glm::mat4 rotation;

            if (dot > 0.9999f) {
                // Vectors are nearly parallel
                rotation = glm::mat4(1.0f);
            } else if (dot < -0.9999f) {
                // Vectors are nearly anti-parallel
                rotation = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
            } else {
                float     angle         = std::acos(dot);
                glm::vec3 rotation_axis = glm::normalize(glm::cross(default_dir, d));
                rotation                = glm::rotate(glm::mat4(1.0f), angle, rotation_axis);
            }

            glm::mat4 t = translation * rotation * scaling;
            transforms.push_back(t);
        }
        m_line_mesh->set_instance_data(transforms, m_color);
        m_line_mesh->upload_instance_data();

        std::vector<glm::mat4x4> vertex_transforms;
        for (const auto& pos : m_positions) {
            glm::mat4 scaling     = glm::scale(glm::mat4(1), glm::vec3(m_radius, m_radius, m_radius));
            glm::mat4 translation = glm::translate(glm::mat4(1), pos);
            vertex_transforms.push_back(translation * scaling);
        }
        m_vertices_mesh->set_instance_data(vertex_transforms, m_color);
        m_vertices_mesh->upload_instance_data();
    }

    void update_ui(std::string name, int index) override {
        ImGui::Checkbox(name.c_str(), &m_visible);
        if (m_visible) {
            std::string label_radius = "set radius ##" + std::to_string(index);
            if (ImGui::SliderFloat(label_radius.c_str(), &m_radius, 0.005f, 1.0f)) {
                set_radius(m_radius);
            }
            std::string label_color = "change color ##" + std::to_string(index);
            if (ImGui::ColorEdit3(label_color.c_str(), (float*)&m_color)) { // Edit 3 floats representing a color
                glm::vec3 newColor(m_color.x, m_color.y, m_color.z);
                set_color(newColor);
            }
        }
    }

    bool                                    m_visible = true;
    std::unique_ptr<InstancedMesh>          m_line_mesh;
    std::unique_ptr<InstancedMesh>          m_vertices_mesh;
    float                                   m_radius = 0.01f;
    glm::vec3                               m_color  = glm::vec3(0.45f, 0.55f, 0.60f);
    const std::vector<glm::vec3>&           m_positions;
    const std::vector<std::pair<int, int>>& m_lines;
};
} // namespace rr