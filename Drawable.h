// contains all the unser interface funtions for the renderrex library
#pragma once

#include "BoundingBox.h"
#include "Mesh.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "glm/glm.hpp"
#include <webgpu/webgpu.h>

namespace rr {

class Renderer;
class Camera;

class Drawable {
public:
    Drawable(const Renderer* r, BoundingBox bb) : m_renderer(r), m_bbox(bb) {}

    virtual ~Drawable() = default;

    virtual void draw(WGPURenderPassEncoder renderPass) = 0;

    virtual void on_camera_update() = 0;

    const Renderer* m_renderer = nullptr;
    BoundingBox     m_bbox;
};

/**
 * The same structure as in the shader, replicated in C++
 */
struct MyUniforms {
    // We add transform matrices
    glm::mat4x4          projectionMatrix;
    glm::mat4x4          viewMatrix;
    glm::mat4x4          modelMatrix;
    std::array<float, 4> color;
};

static_assert(sizeof(MyUniforms) % 16 == 0);

struct VertexAttributes;

class Attribute {
public:
    virtual void mutate_attributes(std::vector<VertexAttributes>&) {}
    virtual void generate_attributes(std::vector<VertexAttributes>&) {};
};

class FaceVectorAttribute : public Attribute {
public:
    FaceVectorAttribute(const std::vector<glm::vec3>& vs) : m_vectors(vs) {}

    void generate_attributes(std::vector<VertexAttributes>& vertex_attributes) override;

    std::vector<glm::vec3> m_vectors;
    std::vector<glm::vec3> m_face_centers;
    float                  m_scale = 0.f;
};

// class FaceColorAttribute : public Attribute {
// public:
//     std::vector<glm::vec3> face_colors;
//
//     void mutate_attribtues(std::vector<VertexAttributes>& vertex_attributes) override;
// };

class RenderMesh : public Drawable {
public:
    RenderMesh(const Mesh& mesh, const Renderer& renderer);
    ~RenderMesh() override;

    void release();

    void configure_render_pipeline();

    void draw(WGPURenderPassEncoder render_pass) override;

    void on_camera_update() override;

    FaceVectorAttribute* add_face_attribute(std::string name, const std::vector<glm::vec3>& vs);

private:
    WGPUBuffer         m_vertexBuffer = nullptr;
    WGPUBuffer         m_uniformBuffer = nullptr;
    WGPUBindGroup      m_bindGroup = nullptr;
    WGPURenderPipeline m_pipeline = nullptr;

    MyUniforms m_uniforms;
    Mesh       m_mesh;
    size_t     m_num_attr_verts = 0;

    std::unordered_map<std::string, std::unique_ptr<Attribute>> m_attributes;
};

} // namespace rr
