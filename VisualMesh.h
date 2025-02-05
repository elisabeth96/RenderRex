#pragma once

#include "Drawable.h"
#include "InstancedMesh.h"
#include "Mesh.h"
#include "Property.h"

#include "glm/glm.hpp"
#include <array>
#include <unordered_map>

namespace rr {

/**
 * The same structure as in the shader, replicated in C++
 */
struct VisualMeshUniforms {
    // We add transform matrices
    glm::mat4x4          projectionMatrix;
    glm::mat4x4          viewMatrix;
    glm::mat4x4          modelMatrix;
    std::array<float, 4> color;
};

static_assert(sizeof(VisualMeshUniforms) % 16 == 0);

struct VisualMeshVertexAttributes;

class VisualMesh : public Drawable {
public:
    VisualMesh(const Mesh& mesh, const Renderer& renderer);
    ~VisualMesh() override;

    void release();

    void configure_render_pipeline();

    void draw(WGPURenderPassEncoder render_pass) override;

    void on_camera_update() override;

    FaceVectorProperty* add_face_attribute(std::string_view name, const std::vector<glm::vec3>& vs);

public:
    Mesh m_mesh;

private:
    WGPUBuffer         m_vertexBuffer  = nullptr;
    WGPUBuffer         m_uniformBuffer = nullptr;
    WGPUBindGroup      m_bindGroup     = nullptr;
    WGPURenderPipeline m_pipeline      = nullptr;

    VisualMeshUniforms m_uniforms;
    size_t             m_num_attr_verts = 0;

    std::unordered_map<std::string, std::unique_ptr<Property>> m_properties;
};

class VisualPointCloud : public Drawable {
public:
    VisualPointCloud(const std::vector<glm::vec3>& positions, const Renderer& renderer);
    //~VisualPointCloud() override;

    // void release() {
    // for (auto& s : m_spheres) {
    // s->release();
    //}
    //};

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

public:
    std::unique_ptr<InstancedMesh> m_spheres;
    float                          m_init_radius = 0.001f;
};

} // namespace rr