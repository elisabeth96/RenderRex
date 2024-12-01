// contains all the unser interface funtions for the renderrex library
#pragma once

#include "BoundingBox.h"
#include "Mesh.h"

#include "glm/glm.hpp"
#include <array>
#include <string>
#include <vector>
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

class RenderMesh : public Drawable {
public:
    RenderMesh(const Mesh& mesh, const Renderer& renderer);
    ~RenderMesh() override;

    void configure_render_pipeline();
    void draw(WGPURenderPassEncoder render_pass) override;
    void on_camera_update() override;

private:
    WGPUBuffer         m_vertexBuffer;
    WGPUBuffer         m_uniformBuffer;
    WGPUBindGroup      m_bindGroup;
    WGPURenderPipeline m_pipeline;

    MyUniforms m_uniforms;
    Mesh       m_mesh;
    size_t m_num_attr_verts = 0;
};

} // namespace rr
