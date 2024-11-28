// contains all the unser interface funtions for the renderrex library

#include "Drawable.h"
#include "Renderer.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include "glm/gtc/matrix_transform.hpp"
#include <GLFW/glfw3.h> // TODO: Remove this dependency
#include <iostream>

namespace rr {

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
        @location(1) normal: vec3f,
        @location(2) color: vec3f,
};

struct VertexOutput {
        @builtin(position) position: vec4f,
        @location(0) color: vec3f,
        @location(1) normal: vec3f,
};

struct MyUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
};

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
        var out: VertexOutput;
        out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
        out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
        out.color = in.color;
        return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
        let normal = normalize(in.normal);

        // Key light (main illumination)
        let keyLightColor = vec3f(1.0, 0.98, 0.95);  // Slightly warm white
        let keyLightDirection = normalize(vec3f(-0.5, -0.8, -0.5));
        let keyIntensity = 1.0;

        // Fill light (reduces harsh shadows)
        let fillLightColor = vec3f(0.9, 0.9, 1.0);   // Slightly cool white
        let fillLightDirection = normalize(vec3f(0.8, -0.2, 0.3));
        let fillIntensity = 0.5;

        // Rim light (helps outline shapes)
        let rimLightColor = vec3f(1.0, 1.0, 1.0);    // Pure white
        let rimLightDirection = normalize(vec3f(-0.2, 0.5, 0.8));
        let rimIntensity = 0.3;

        // Ambient light (ensures no part is completely dark)
        let ambientIntensity = 0.2;

        // Calculate each light contribution
        let keyLight = max(0.0, -dot(keyLightDirection, normal)) * keyIntensity * keyLightColor;
        let fillLight = max(0.0, -dot(fillLightDirection, normal)) * fillIntensity * fillLightColor;
        let rimLight = max(0.0, -dot(rimLightDirection, normal)) * rimIntensity * rimLightColor;
        let ambient = vec3f(ambientIntensity);

        // Combine all lighting
        let totalLighting = keyLight + fillLight + rimLight + ambient;

        // Apply lighting to color
        let color = in.color * totalLighting;

        // Tone mapping to prevent over-exposure
        let mapped_color = color / (color + 1.0);

        // Gamma correction (using 2.2 for standard sRGB)
        let corrected_color = pow(mapped_color, vec3f(1.0/2.2));

        return vec4f(corrected_color, 1.0);
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

void Mesh::draw(WGPURenderPassEncoder renderPass) {
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
    m_uniforms.modelMatrix = glm::mat4(1.0f);
    m_uniforms.color       = {0.8f, 0.6f, 0.3f, 1.0f};

    float ratio       = 1.0f;
    float focalLength = 2.0;
    float near        = 0.01f;
    float far         = 100.0f;
    float divider     = 1 / (focalLength * (far - near));
    m_uniforms.projectionMatrix =
        transpose(glm::mat4x4(1.0, 0.0, 0.0, 0.0, 0.0, ratio, 0.0, 0.0, 0.0, 0.0, far * divider, -far * near * divider,
                              0.0, 0.0, 1.0 / focalLength, 0.0));

    wgpuQueueWriteBuffer(queue, m_uniformBuffer, 0, &m_uniforms, sizeof(MyUniforms));
}

} // namespace rr
