#pragma once

#include "Drawable.h"
#include "Mesh.h"
#include "Property.h"

#include "glm/glm.hpp"
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
    Mesh       m_mesh;

private:
    WGPUBuffer         m_vertexBuffer  = nullptr;
    WGPUBuffer         m_uniformBuffer = nullptr;
    WGPUBindGroup      m_bindGroup     = nullptr;
    WGPURenderPipeline m_pipeline      = nullptr;

    VisualMeshUniforms m_uniforms;
    size_t     m_num_attr_verts = 0;

    std::unordered_map<std::string, std::unique_ptr<Property>> m_properties;
};

} // namespace rr