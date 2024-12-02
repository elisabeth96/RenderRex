#pragma once

#include "Drawable.h"
#include "Mesh.h"

#include "glm/glm.hpp"
#include <unordered_map>

namespace rr {

struct InstancedMeshUniforms {
    // We add transform matrices
    glm::mat4x4 projectionMatrix;
    glm::mat4x4 viewMatrix;
};

struct InstanceData {
    glm::mat4x4 transform;
    glm::vec4   color;
};

static_assert(sizeof(InstancedMeshUniforms) % 16 == 0);
static_assert(sizeof(InstanceData) % 16 == 0);

class InstancedMesh : public Drawable {
public:
    InstancedMesh(Mesh mesh, size_t num_instances, const Renderer& renderer);
    ~InstancedMesh() override;

    void release();

    void configure_render_pipeline();

    void draw(WGPURenderPassEncoder render_pass) override;

    void on_camera_update() override;

    void set_transforms(const std::vector<glm::mat4x4>& transforms) {
        for (size_t i = 0; i < m_instance_data.size(); ++i) {
            m_instance_data[i].transform = transforms[i];
        }
    }

    void set_translations(const std::vector<glm::vec3>& translations) {
        for (size_t i = 0; i < m_instance_data.size(); ++i) {
            m_instance_data[i].transform[3] = glm::vec4(translations[i], 1.0f);
        }
    }

    void set_colors(const std::vector<glm::vec4>& colors) {
        for (size_t i = 0; i < m_instance_data.size(); ++i) {
            m_instance_data[i].color = colors[i];
        }
    }

    void set_color(const glm::vec4& color) {
        for (auto& instance : m_instance_data) {
            instance.color = color;
        }
    }

private:
    WGPUBuffer         m_vertexBuffer   = nullptr;
    WGPUBuffer         m_uniformBuffer  = nullptr;
    WGPUBindGroup      m_bindGroup      = nullptr;
    WGPURenderPipeline m_pipeline       = nullptr;
    WGPUBuffer         m_instanceBuffer = nullptr;

    InstancedMeshUniforms m_uniforms;

    Mesh   m_mesh;
    size_t m_num_attr_verts = 0;

    std::vector<InstanceData> m_instance_data;
};

} // namespace rr
