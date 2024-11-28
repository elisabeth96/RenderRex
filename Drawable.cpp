// contains all the unser interface funtions for the renderrex library

#include "Drawable.h"
#include "Renderer.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include "glm/gtc/matrix_transform.hpp"
#include <GLFW/glfw3.h> // TODO: Remove this dependency
#include <iostream>

namespace rr {

constexpr float M_PI = 3.14159265358979323846f;

// Have the compiler check byte alignment
static_assert(sizeof(MyUniforms) % 16 == 0);

// Converts vector of glm::vec3 to vector of VertexAttributes
std::vector<VertexAttributes> convert_to_vertex_attributes(const std::vector<glm::vec3>& positions,
                                                           /* const std::vector<glm::vec3>& normals,
                                                           const std::vector<glm::vec3>& colors,*/
                                                           const std::vector<std::array<int, 3>>& triangles) {
    std::vector<VertexAttributes> vertex_attributes;
    for (size_t i = 0; i < triangles.size(); i++) {
        glm::vec3 normal = glm::normalize(glm::cross(positions[triangles[i][1]] - positions[triangles[i][0]],
                                                     positions[triangles[i][2]] - positions[triangles[i][0]]));
        for (size_t j = 0; j < 3; j++) {
            VertexAttributes va;
            va.position = positions[triangles[i][j]];
            va.normal   = normal;
            va.color    = {0.6, 0.6, 0.3};
            vertex_attributes.push_back(va);
        }
    }
    return vertex_attributes;
}

Mesh::Mesh(std::vector<glm::vec3>& positions, std::vector<std::array<int, 3>>& triangles, const Renderer& renderer) {
    m_vertex_attributes = convert_to_vertex_attributes(positions, triangles);
    configure_render_pipeline(m_vertex_attributes, renderer);
}

Mesh::~Mesh() {
    // Release resources
    wgpuBufferDestroy(m_vertexBuffer);
    wgpuBufferRelease(m_vertexBuffer);
    wgpuBufferDestroy(m_uniformBuffer);
    wgpuBufferRelease(m_uniformBuffer);
    wgpuBindGroupRelease(m_bindGroup);
    wgpuRenderPipelineRelease(m_pipeline);
}

void Mesh::configure_render_pipeline(const std::vector<VertexAttributes>& vertex_attributes, const Renderer& renderer) {

    const char* shaderSource = R"(
struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f, // new attribute
	@location(2) color: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f, // <--- Add a normal output
};

/**
 * A structure holding the value of our uniforms
 */
struct MyUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
    time: f32,
};

// Instead of the simple uTime variable, our uniform variable is a struct
@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
	// Forward the normal
    out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.color = in.color;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let normal = normalize(in.normal);

	let lightColor1 = vec3f(1.0, 1, 1);
	let lightColor2 = vec3f(1, 1, 1);
	let lightDirection1 = vec3f(0.5, -0.9, 0.1);
	let lightDirection2 = vec3f(0.2, 0.4, 0.3);
	let shading1 = max(0.0, dot(lightDirection1, normal));
	let shading2 = max(0.0, dot(lightDirection2, normal));
	let shading = shading1 * lightColor1 + shading2 * lightColor2;
	let color = in.color * shading;

	// Gamma-correction
	let corrected_color = pow(color, vec3f(2.2));
	return vec4f(corrected_color, 1);
}
)";

    WGPUShaderModuleDescriptor     shaderDesc{};
    WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};
    // Set the chained struct's header
    shaderCodeDesc.chain.next  = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    // Connect the chain
    shaderDesc.nextInChain        = &shaderCodeDesc.chain;
    shaderCodeDesc.code           = shaderSource;
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(renderer.m_device, &shaderDesc);

    // Vertex fetch
    std::vector<WGPUVertexAttribute> vertexAttribs(3);

    // Position attribute
    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[0].offset         = 0;

    // Normal attribute
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[1].offset         = offsetof(VertexAttributes, normal);

    // Color attribute
    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[2].offset         = offsetof(VertexAttributes, color);

    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributeCount         = (uint32_t)vertexAttribs.size();
    vertexBufferLayout.attributes             = vertexAttribs.data();
    vertexBufferLayout.arrayStride            = sizeof(VertexAttributes);

    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    WGPURenderPipelineDescriptor pipelineDesc = {};

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers     = &vertexBufferLayout;

    pipelineDesc.vertex.module        = shaderModule;
    pipelineDesc.vertex.entryPoint    = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants     = nullptr;

    pipelineDesc.primitive.topology         = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace        = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode         = WGPUCullMode_None;

    WGPUFragmentState fragmentState = {};
    pipelineDesc.fragment           = &fragmentState;
    fragmentState.module            = shaderModule;
    fragmentState.entryPoint        = "fs_main";
    fragmentState.constantCount     = 0;
    fragmentState.constants         = nullptr;

    WGPUBlendState blendState  = {};
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;
    blendState.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget = {};
    colorTarget.format               = renderer.m_swapChainFormat;
    colorTarget.blend                = &blendState;
    colorTarget.writeMask            = WGPUColorWriteMask_All;

    fragmentState.targetCount = 1;
    fragmentState.targets     = &colorTarget;

    WGPUDepthStencilState depthStencilState = {};
    depthStencilState.depthCompare          = WGPUCompareFunction_Less;
    depthStencilState.depthWriteEnabled     = true;
    depthStencilState.format                = renderer.m_depthTextureFormat;
    depthStencilState.stencilReadMask       = 0;
    depthStencilState.stencilWriteMask      = 0;

    pipelineDesc.depthStencil = &depthStencilState;

    pipelineDesc.multisample.count                  = 1;
    pipelineDesc.multisample.mask                   = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    // Create binding layout (don't forget to = Default)
    WGPUBindGroupLayoutEntry bindingLayout = {};
    bindingLayout.binding                  = 0;
    bindingLayout.visibility               = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindingLayout.buffer.type              = WGPUBufferBindingType_Uniform;
    bindingLayout.buffer.minBindingSize    = sizeof(MyUniforms);

    // Create a bind group layout
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount      = 1;
    bindGroupLayoutDesc.entries         = &bindingLayout;
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(renderer.m_device, &bindGroupLayoutDesc);

    // Create the pipeline layout
    WGPUPipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = (WGPUBindGroupLayout*)&bindGroupLayout;
    WGPUPipelineLayout layout       = wgpuDeviceCreatePipelineLayout(renderer.m_device, &layoutDesc);
    pipelineDesc.layout             = layout;

    // Create vertex buffer
    WGPUBufferDescriptor bufferDesc = {};
    m_vertex_attributes             = vertex_attributes;
    bufferDesc.size                 = vertex_attributes.size() * sizeof(VertexAttributes);
    bufferDesc.usage                = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    bufferDesc.mappedAtCreation     = false;
    m_vertexBuffer                  = wgpuDeviceCreateBuffer(renderer.m_device, &bufferDesc);
    wgpuQueueWriteBuffer(renderer.m_queue, m_vertexBuffer, 0, vertex_attributes.data(), bufferDesc.size);

    // Create uniform buffer
    bufferDesc.size             = sizeof(MyUniforms);
    bufferDesc.usage            = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    bufferDesc.mappedAtCreation = false;
    m_uniformBuffer             = wgpuDeviceCreateBuffer(renderer.m_device, &bufferDesc);

    // Build transform matrices

    // Translate the view
    glm::vec3 focalPoint(0.0, 0.0, -1.0);
    // Rotate the object
    float angle1 = 2.0f; // arbitrary time
    // Rotate the view point
    float angle2 = 3.0f * M_PI / 4.0f;

    // m_S                    = glm::scale(glm::mat4x4(1.0), glm::vec3(0.3f));
    // m_T1                   = glm::mat4x4(1.0);
    // glm::mat4x4 R1         = glm::rotate(glm::mat4x4(1.0), angle1, glm::vec3(0.0, 0.0, 1.0));
    // m_uniforms.modelMatrix = R1 * m_T1 * m_S;
    m_uniforms.modelMatrix = glm::mat4x4(1.0);

    glm::mat4x4 R2        = glm::rotate(glm::mat4x4(1.0), -angle2, glm::vec3(1.0, 0.0, 0.0));
    glm::mat4x4 T2        = glm::translate(glm::mat4x4(1.0), -focalPoint);
    m_uniforms.viewMatrix = T2 * R2;

    float ratio       = float(renderer.m_width) / float(renderer.m_height);
    float focalLength = 2.0;
    float near        = 0.01f;
    float far         = 100.0f;
    float divider     = 1 / (focalLength * (far - near));
    m_uniforms.projectionMatrix =
        transpose(glm::mat4x4(1.0, 0.0, 0.0, 0.0, 0.0, ratio, 0.0, 0.0, 0.0, 0.0, far * divider, -far * near * divider,
                              0.0, 0.0, 1.0 / focalLength, 0.0));

    m_uniforms.time  = 1.0f;
    m_uniforms.color = {0.0f, 1.0f, 0.4f, 1.0f};
    wgpuQueueWriteBuffer(renderer.m_queue, m_uniformBuffer, 0, &m_uniforms, sizeof(MyUniforms));

    std::cout << "Initial View matrix: " << std::endl;
    std::cout << m_uniforms.viewMatrix[0][0] << " " << m_uniforms.viewMatrix[0][1] << " " << m_uniforms.viewMatrix[0][2]
              << " " << m_uniforms.viewMatrix[0][3] << std::endl;
    std::cout << m_uniforms.viewMatrix[1][0] << " " << m_uniforms.viewMatrix[1][1] << " " << m_uniforms.viewMatrix[1][2]
              << " " << m_uniforms.viewMatrix[1][3] << std::endl;
    std::cout << m_uniforms.viewMatrix[2][0] << " " << m_uniforms.viewMatrix[2][1] << " " << m_uniforms.viewMatrix[2][2]
              << " " << m_uniforms.viewMatrix[2][3] << std::endl;
    std::cout << m_uniforms.viewMatrix[3][0] << " " << m_uniforms.viewMatrix[3][1] << " " << m_uniforms.viewMatrix[3][2]
              << " " << m_uniforms.viewMatrix[3][3] << std::endl;

    // Create a binding
    WGPUBindGroupEntry binding{};
    binding.binding = 0;
    binding.buffer  = m_uniformBuffer;
    binding.offset  = 0;
    binding.size    = sizeof(MyUniforms);

    // A bind group contains one or multiple bindings
    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout                  = bindGroupLayout;
    bindGroupDesc.entryCount              = bindGroupLayoutDesc.entryCount;
    bindGroupDesc.entries                 = &binding;
    m_bindGroup                           = wgpuDeviceCreateBindGroup(renderer.m_device, &bindGroupDesc);

    m_pipeline = wgpuDeviceCreateRenderPipeline(renderer.m_device, &pipelineDesc);
    wgpuShaderModuleRelease(shaderModule);
}

void Mesh::draw(const Renderer& renderer, WGPURenderPassEncoder renderPass) {

    // Update uniform buffer
    // m_uniforms.time = static_cast<float>(glfwGetTime()); // glfwGetTime returns a double
    // Only update the 1-st float of the buffer
    // wgpuQueueWriteBuffer(renderer.m_queue, m_uniformBuffer, offsetof(MyUniforms, time), &m_uniforms.time,
    // sizeof(MyUniforms::time));

    // Update view matrix
    // float       angle1     = m_uniforms.time;
    // glm::mat4x4 R1         = glm::rotate(glm::mat4x4(1.0), angle1, glm::vec3(0.0, 0.0, 1.0));
    // m_uniforms.modelMatrix = R1 * m_T1 * m_S;
    // wgpuQueueWriteBuffer(renderer.m_queue, m_uniformBuffer, offsetof(MyUniforms, modelMatrix),
    // &m_uniforms.modelMatrix, sizeof(MyUniforms::modelMatrix));

    wgpuRenderPassEncoderSetPipeline(renderPass, m_pipeline);

    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_vertexBuffer, 0,
                                         m_vertex_attributes.size() * sizeof(VertexAttributes));

    // Set binding group
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDraw(renderPass, uint32_t(m_vertex_attributes.size()), 1, 0, 0);
}

void Mesh::set_view_matrix(const CameraState& state, WGPUQueue queue) {
    float     cx          = cos(state.angles.x);
    float     sx          = sin(state.angles.x);
    float     cy          = cos(state.angles.y);
    float     sy          = sin(state.angles.y);
    glm::vec3 position    = glm::vec3(cx * cy, sx * cy, sy) * std::exp(-state.zoom);
    m_uniforms.viewMatrix = glm::lookAt(position, glm::vec3(0.0f), glm::vec3(0, 0, 1));

    // print view matrix

    std::cout << "View matrix: " << std::endl;
    std::cout << m_uniforms.viewMatrix[0][0] << " " << m_uniforms.viewMatrix[0][1] << " " << m_uniforms.viewMatrix[0][2]
              << " " << m_uniforms.viewMatrix[0][3] << std::endl;
    std::cout << m_uniforms.viewMatrix[1][0] << " " << m_uniforms.viewMatrix[1][1] << " " << m_uniforms.viewMatrix[1][2]
              << " " << m_uniforms.viewMatrix[1][3] << std::endl;
    std::cout << m_uniforms.viewMatrix[2][0] << " " << m_uniforms.viewMatrix[2][1] << " " << m_uniforms.viewMatrix[2][2]
              << " " << m_uniforms.viewMatrix[2][3] << std::endl;
    std::cout << m_uniforms.viewMatrix[3][0] << " " << m_uniforms.viewMatrix[3][1] << " " << m_uniforms.viewMatrix[3][2]
              << " " << m_uniforms.viewMatrix[3][3] << std::endl;

    wgpuQueueWriteBuffer(queue, m_uniformBuffer, offsetof(MyUniforms, viewMatrix), &m_uniforms.viewMatrix,
                         sizeof(MyUniforms::viewMatrix));
}

} // namespace rr
