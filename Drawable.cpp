// contains all the unser interface funtions for the renderrex library

#include "Drawable.h"
#include "Camera.h"
#include "Renderer.h"
#include "ShaderCode.h"
#include <iostream>

namespace rr {

// Have the compiler check byte alignment
static_assert(sizeof(MyUniforms) % 16 == 0);

std::vector<VertexAttributes> convert_to_vertex_attributes(const std::vector<glm::vec3>& positions,
                                                           /* const std::vector<glm::vec3>& normals,
                                                           const std::vector<glm::vec3>& colors,*/
                                                           const std::vector<std::array<int, 3>>& triangles) {

    std::vector<VertexAttributes> vertex_attributes(3 * triangles.size());
    for (size_t i = 0; i < triangles.size(); i++) {
        glm::vec3 pts[3] = {positions[triangles[i][0]], positions[triangles[i][1]], positions[triangles[i][2]]};
        glm::vec3 normal = glm::cross(pts[1] - pts[0], pts[2] - pts[0]);
        normal           = glm::normalize(normal);
        for (int j = 0; j < 3; j++) {
            glm::vec3 bary(0.0f);
            bary[j]                               = 1.0f;
            vertex_attributes[3 * i + j].position = pts[j];
            vertex_attributes[3 * i + j].normal   = normal;
            vertex_attributes[3 * i + j].bary     = bary;
        }
    }

    return vertex_attributes;
}

Mesh::Mesh(const std::vector<glm::vec3>& positions, const std::vector<std::array<int, 3>>& triangles,
           const Renderer& renderer)
    : Drawable(&renderer, BoundingBox(positions)) {
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

void shaderCompilationCallback(WGPUCompilationInfoRequestStatus, WGPUCompilationInfo const* compilationInfo, void*) {
    if (compilationInfo) {
        for (uint32_t i = 0; i < compilationInfo->messageCount; ++i) {
            WGPUCompilationMessage const& message = compilationInfo->messages[i];

            const char* messageType;
            switch (message.type) {
            case WGPUCompilationMessageType_Error:
                messageType = "Error";
                break;
            case WGPUCompilationMessageType_Warning:
                messageType = "Warning";
                break;
            case WGPUCompilationMessageType_Info:
                messageType = "Info";
                break;
            default:
                messageType = "Unknown";
                break;
            }

            std::cerr << messageType << " at line " << message.lineNum << ", column " << message.linePos << ": "
                      << message.message << std::endl;
        }
    }
}

WGPUShaderModule createShaderModule(WGPUDevice device, const char* shaderSource) {
    WGPUShaderModuleDescriptor     shaderDesc{};
    WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};

    shaderCodeDesc.chain.next  = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    shaderDesc.nextInChain     = &shaderCodeDesc.chain;
    shaderCodeDesc.code        = shaderSource;

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    wgpuShaderModuleGetCompilationInfo(shaderModule, shaderCompilationCallback, nullptr);

    return shaderModule;
}

void Mesh::configure_render_pipeline(const std::vector<VertexAttributes>& vertex_attributes, const Renderer& renderer) {

    WGPUShaderModule shaderModule = createShaderModule(renderer.m_device, shaderCode);

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
    vertexAttribs[2].offset         = offsetof(VertexAttributes, bary);

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

void Mesh::draw(WGPURenderPassEncoder render_pass) {
    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, m_vertexBuffer, 0,
                                         m_vertex_attributes.size() * sizeof(VertexAttributes));

    // Set binding group
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, m_bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDraw(render_pass, uint32_t(m_vertex_attributes.size()), 1, 0, 0);
}

void Mesh::on_camera_update() {
    m_uniforms.viewMatrix = m_renderer->m_camera.transform();

    m_uniforms.modelMatrix = glm::mat4(1.0f);
    m_uniforms.color       = {0.f, 0.0f, 0.0f, 1.0f};

    float aspect_ratio          = 1;
    float far_plane             = 100.0f;
    float near_plane            = 0.01f;
    float fov                   = glm::radians(45.0f);
    m_uniforms.projectionMatrix = glm::perspective(fov, aspect_ratio, near_plane, far_plane);

    wgpuQueueWriteBuffer(m_renderer->m_queue, m_uniformBuffer, 0, &m_uniforms, sizeof(MyUniforms));
}

} // namespace rr
