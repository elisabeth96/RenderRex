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


    // In descending order of importance:
    // 1. The wireframe should be at least 1-2 pixel if close enough and never thicker than say 10-20 pixels.
    // 2. It should not be thicker than say 20% of the corresponding height.
    // 3. It should have uniform thickness throughout the mesh
    // 4. It should have a user defined thickness in worldspace units.
    //
    // Note that constraint 1 depends on the view transform, so we can't really enforce it here. Instead,
    // this is done using screen space derivative of the barycentric coordinates in screen space.

    // As a default, we aim for 5% of the average height
    std::vector<glm::vec3> heights(triangles.size());
    double average_height = 0.0;

    std::vector<VertexAttributes> vertex_attributes(3 * triangles.size());
    for (size_t i = 0; i < triangles.size(); i++) {
        glm::vec3 pts[3] = {positions[triangles[i][0]], positions[triangles[i][1]], positions[triangles[i][2]]};
        glm::vec3 normal = glm::cross(pts[1] - pts[0], pts[2] - pts[0]);
        float     area   = glm::length(normal) * 0.5f;
        normal           = glm::normalize(normal);
        glm::vec3 height;
        for (int j = 0; j < 3; j++) {
            glm::vec3 bary(0.0f);
            bary[j] = 1.0f;
            // vertex_attributes.push_back(va);
            vertex_attributes[3 * i + j].position = pts[j];
            vertex_attributes[3 * i + j].normal   = normal;
            vertex_attributes[3 * i + j].bary     = bary;

            float d = glm::distance(pts[(j + 1) % 3], pts[(j + 2) % 3]);
            // area = 0.5 * d * h
            float h    = 2.0f * area / d;
            height[j] = h;
        }

        heights[i] = height;
        average_height += double(height.x) + double(height.y) + double(height.z);
    }

    average_height /= 3.0 * double(triangles.size());
    float target_wt = float(0.05 * average_height);

    for(size_t i = 0; i < triangles.size(); i++) {
        // we want to find x such that x * h = target_wt, so x = target_wt / h.
        // we also want to clamp x to 0.2 to avoid the wire occluding the triangle too much, even
        // at the expense of not having uniform thickness.
        glm::vec3 wl = glm::min(target_wt / heights[i], 0.2f);
        vertex_attributes[3 * i + 0].wire_limits = wl;
        vertex_attributes[3 * i + 1].wire_limits = wl;
        vertex_attributes[3 * i + 2].wire_limits = wl;
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

void shaderCompilationCallback(WGPUCompilationInfoRequestStatus, WGPUCompilationInfo const* compilationInfo,
                               void*) {
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
    std::vector<WGPUVertexAttribute> vertexAttribs(4);

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

    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[3].offset         = offsetof(VertexAttributes, wire_limits);

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

void Mesh::update_camera(const Camera& camera, WGPUQueue queue) {
    m_uniforms.viewMatrix = camera.transform();

    m_uniforms.modelMatrix = glm::mat4(1.0f);
    m_uniforms.color       = {0.f, 0.0f, 0.0f, 1.0f};

    float aspect_ratio          = 1;
    float far_plane             = 100.0f;
    float near_plane            = 0.01f;
    float fov                   = glm::radians(45.0f);
    m_uniforms.projectionMatrix = glm::perspective(fov, aspect_ratio, near_plane, far_plane);

    wgpuQueueWriteBuffer(queue, m_uniformBuffer, 0, &m_uniforms, sizeof(MyUniforms));
}

} // namespace rr
