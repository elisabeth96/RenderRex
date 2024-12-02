#pragma once

#include "Drawable.h"
#include "Mesh.h"

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

class Attribute {
public:
    virtual ~Attribute() = default;
    virtual void generate_attributes(std::vector<VisualMeshVertexAttributes>&) = 0;
};

class FaceVectorAttribute : public Attribute {
public:
    explicit FaceVectorAttribute(const std::vector<glm::vec3>& vs) : m_vectors(vs) {}

    void generate_attributes(std::vector<VisualMeshVertexAttributes>& vertex_attributes) override;

    void set_color(const glm::vec3& color) {}

    std::vector<glm::vec3> m_vectors;
    std::vector<glm::vec3> m_face_centers;
    float                  m_scale = 0.f;

    glm::vec3 m_color = glm::vec3(0.882, 0.902, 0.376);
};

class VisualMesh : public Drawable {
public:
    VisualMesh(const Mesh& mesh, const Renderer& renderer);
    ~VisualMesh() override;

    void release();

    void configure_render_pipeline();

    void draw(WGPURenderPassEncoder render_pass) override;

    void on_camera_update() override;

    FaceVectorAttribute* add_face_attribute(std::string name, const std::vector<glm::vec3>& vs);

private:
    WGPUBuffer         m_vertexBuffer  = nullptr;
    WGPUBuffer         m_uniformBuffer = nullptr;
    WGPUBindGroup      m_bindGroup     = nullptr;
    WGPURenderPipeline m_pipeline      = nullptr;

    VisualMeshUniforms m_uniforms;
    Mesh       m_mesh;
    size_t     m_num_attr_verts = 0;

    std::unordered_map<std::string, std::unique_ptr<Attribute>> m_attributes;
};

} // namespace rr