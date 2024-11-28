// contains all the unser interface funtions for the renderrex library
#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include "glm/glm.hpp"

#include <array>
#include <string>
#include <vector>
#include <webgpu/webgpu.h>

namespace rr {

class Renderer;
struct CameraState;

class Drawable {
public:
    virtual ~Drawable() = default;

    virtual void draw(WGPURenderPassEncoder renderPass) = 0;

    virtual void set_view_matrix(const CameraState& state, WGPUQueue queue) = 0;

private:
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

/**
 * A structure that describes the data layout in the vertex buffer
 * We do not instantiate it but use it in `sizeof` and `offsetof`
 */
struct VertexAttributes {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

class Mesh : public Drawable {
public:
    Mesh(std::vector<glm::vec3>& positions, std::vector<std::array<int, 3>>& triangles, const Renderer& renderer);
    ~Mesh();

    void configure_render_pipeline(const std::vector<VertexAttributes>& vertex_attributes, const Renderer& renderer);
    void draw(WGPURenderPassEncoder renderPass) override;
    void set_view_matrix(const CameraState& state, WGPUQueue queue) override;

private:
    WGPUBuffer                    m_vertexBuffer;
    WGPUBuffer                    m_uniformBuffer;
    WGPUBindGroup                 m_bindGroup;
    WGPURenderPipeline            m_pipeline;

    MyUniforms                    m_uniforms;
    std::vector<VertexAttributes> m_vertex_attributes;
};

} // namespace rr
