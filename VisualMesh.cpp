#include "VisualMesh.h"

#include "Camera.h"
#include "InstancedMesh.h"
#include "Mesh.h"
#include "Primitives.h"
#include "Property.h"
#include "Renderer.h"
#include "ShaderCode.h"

#include <iostream>

namespace rr {

static std::vector<VisualMeshVertexAttributes> create_vertex_attributes(const Mesh& mesh, glm::vec3 color) {
    std::vector<VisualMeshVertexAttributes> vertex_attributes;
    vertex_attributes.reserve(3 * mesh.num_faces());

    assert(!mesh.normal_faces.empty());

    size_t num_faces = mesh.num_faces();
    for (size_t i = 0; i < num_faces; ++i) {
        const auto& f  = mesh.position_faces[i];
        const auto& nf = mesh.normal_faces[i];

        // For faces with more than 3 vertices, we need to triangulate
        // We use fan triangulation: connect first vertex to all other vertices in sequence
        size_t num_triangles = f.size() - 2;

        for (size_t j = 0; j < num_triangles; ++j) {
            // For each triangle in the fan, we need to determine which edges are real
            // and which are internal triangulation edges

            // A triangle has vertices: center(0), j+1, j+2
            // An edge is real if:
            // - It's part of the original face boundary
            // - For fan triangulation, this means either:
            //   a) It connects to consecutive vertices in the original face
            //   b) It's the first or last edge connected to the center vertex

            glm::vec3 edge_mask(1.0f);

            edge_mask[0] = 0.0f;

            if (j == num_triangles - 1) {
                edge_mask[1] = 0.0f;
            }

            if (j == 0) {
                edge_mask[2] = 0.0f;
            }

            vertex_attributes.push_back({mesh.positions[f[0]], mesh.normals[nf[0]],
                                         glm::vec3(1.0f, 0.0f, 0.0f), // Barycentric coordinates (1,0,0)
                                         edge_mask, color});

            vertex_attributes.push_back({mesh.positions[f[j + 1]], mesh.normals[nf[j + 1]],
                                         glm::vec3(0.0f, 1.0f, 0.0f), // Barycentric coordinates (0,1,0)
                                         edge_mask, color});

            vertex_attributes.push_back({mesh.positions[f[j + 2]], mesh.normals[nf[j + 2]],
                                         glm::vec3(0.0f, 0.0f, 1.0f), // Barycentric coordinates (0,0,1)
                                         edge_mask, color});
        }
    }

    return vertex_attributes;
}

VisualMesh::VisualMesh(const Mesh& mesh, const Renderer& renderer)
    : Drawable(&renderer, BoundingBox(mesh.positions)), m_mesh(mesh) {

    configure_render_pipeline();
}

void VisualMesh::release() {
    // Release resources
    if (m_vertexBuffer == nullptr) {
        return;
    }
    wgpuBufferDestroy(m_vertexBuffer);
    wgpuBufferRelease(m_vertexBuffer);
    wgpuBufferDestroy(m_uniformBuffer);
    wgpuBufferRelease(m_uniformBuffer);
    wgpuBindGroupRelease(m_bindGroup);
    wgpuRenderPipelineRelease(m_pipeline);
}

VisualMesh::~VisualMesh() {
    release();
}

void shaderCompilationCallback(WGPUCompilationInfoRequestStatus, WGPUCompilationInfo const* compilationInfo, void*,
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
                      << to_string(message.message) << std::endl;
        }
    }
}

WGPUShaderModule createShaderModule(WGPUDevice device, const char* shaderSource) {
    WGPUShaderModuleDescriptor     shaderDesc{};
    WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};

    shaderCodeDesc.chain.next  = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderDesc.nextInChain     = &shaderCodeDesc.chain;
    shaderCodeDesc.code        = to_string_view(shaderSource);

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    WGPUCompilationInfoCallbackInfo callback_info = {};
    callback_info.callback                        = shaderCompilationCallback;
    callback_info.mode                            = WGPUCallbackMode_AllowSpontaneous;

    wgpuShaderModuleGetCompilationInfo(shaderModule, callback_info);

    return shaderModule;
}

void VisualMesh::configure_render_pipeline() {
    release();
    const Renderer& renderer = *m_renderer;

    m_vertex_attributes = create_vertex_attributes(m_mesh, m_mesh_color);

    WGPUShaderModule shaderModule = createShaderModule(renderer.m_device, shaderCode);

    // Vertex fetch
    std::vector<WGPUVertexAttribute> vertexAttribs(5);

    // Position attribute
    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[0].offset         = 0;

    // Normal attribute
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[1].offset         = offsetof(VisualMeshVertexAttributes, normal);

    // Bary attribute
    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[2].offset         = offsetof(VisualMeshVertexAttributes, bary);

    // Edge mask attribute
    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[3].offset         = offsetof(VisualMeshVertexAttributes, edge_mask);

    vertexAttribs[4].shaderLocation = 4;
    vertexAttribs[4].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[4].offset         = offsetof(VisualMeshVertexAttributes, color);

    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributeCount         = (uint32_t)vertexAttribs.size();
    vertexBufferLayout.attributes             = vertexAttribs.data();
    vertexBufferLayout.arrayStride            = sizeof(VisualMeshVertexAttributes);

    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    WGPURenderPipelineDescriptor pipelineDesc = {};

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers     = &vertexBufferLayout;

    pipelineDesc.vertex.module        = shaderModule;
    pipelineDesc.vertex.entryPoint    = to_string_view("vs_main");
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants     = nullptr;

    pipelineDesc.primitive.topology         = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace        = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode         = WGPUCullMode_None;

    WGPUFragmentState fragmentState = {};
    pipelineDesc.fragment           = &fragmentState;
    fragmentState.module            = shaderModule;
    fragmentState.entryPoint        = to_string_view("fs_main");
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
    depthStencilState.depthWriteEnabled     = WGPUOptionalBool_True;
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
    bindingLayout.buffer.minBindingSize    = sizeof(VisualMeshUniforms);

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
    bufferDesc.size                 = m_vertex_attributes.size() * sizeof(VisualMeshVertexAttributes);
    bufferDesc.usage                = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    bufferDesc.mappedAtCreation     = false;
    m_vertexBuffer                  = wgpuDeviceCreateBuffer(renderer.m_device, &bufferDesc);
    wgpuQueueWriteBuffer(renderer.m_queue, m_vertexBuffer, 0, m_vertex_attributes.data(), bufferDesc.size);

    // Create uniform buffer
    bufferDesc.size             = sizeof(VisualMeshUniforms);
    bufferDesc.usage            = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    bufferDesc.mappedAtCreation = false;
    m_uniformBuffer             = wgpuDeviceCreateBuffer(renderer.m_device, &bufferDesc);

    // Create a binding
    WGPUBindGroupEntry binding{};
    binding.binding = 0;
    binding.buffer  = m_uniformBuffer;
    binding.offset  = 0;
    binding.size    = sizeof(VisualMeshUniforms);

    // A bind group contains one or multiple bindings
    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout                  = bindGroupLayout;
    bindGroupDesc.entryCount              = bindGroupLayoutDesc.entryCount;
    bindGroupDesc.entries                 = &binding;
    m_bindGroup                           = wgpuDeviceCreateBindGroup(renderer.m_device, &bindGroupDesc);

    m_pipeline = wgpuDeviceCreateRenderPipeline(renderer.m_device, &pipelineDesc);
    wgpuShaderModuleRelease(shaderModule);

    // This has to be called here because the camera uniforms are cleared when reconfiguring
    // the pipeline. When the mesh is registered this is called from the renderer, but when
    // an attribute is registered the pipeline is reconfigured and the camera uniforms are lost.
    on_camera_update();
}

void VisualMesh::draw(WGPURenderPassEncoder render_pass) {
    if (!m_visable) {
        return;
    }

    if (m_attributes_dirty) {
        size_t size = m_vertex_attributes.size() * sizeof(VisualMeshVertexAttributes);
        wgpuQueueWriteBuffer(Renderer::get().m_queue, m_vertexBuffer, 0, m_vertex_attributes.data(), size);
        m_attributes_dirty = false;
    }

    if (m_uniforms_dirty) {
        wgpuQueueWriteBuffer(m_renderer->m_queue, m_uniformBuffer, 0, &m_uniforms, sizeof(VisualMeshUniforms));
        m_uniforms_dirty = false;
    }

    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, m_vertexBuffer, 0,
                                         m_vertex_attributes.size() * sizeof(VisualMeshVertexAttributes));

    // Set binding group
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, m_bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDraw(render_pass, uint32_t(m_vertex_attributes.size()), 1, 0, 0);

    for (auto& [name, prop] : m_vector_properties) {
        prop->draw(render_pass);
    }
}

void VisualMesh::on_camera_update() {
    float aspect_ratio = static_cast<float>(m_renderer->m_width) / static_cast<float>(m_renderer->m_height);
    float far_plane    = 100.0f;
    float near_plane   = 0.01f;
    float fov          = glm::radians(45.0f);

    m_uniforms.viewMatrix       = m_renderer->m_camera.transform();
    m_uniforms.modelMatrix      = glm::mat4(1.0f);
    m_uniforms.projectionMatrix = glm::perspective(fov, aspect_ratio, near_plane, far_plane);
    m_uniforms_dirty            = true;

    // update vector properties
    for (auto& [name, prop] : m_vector_properties) {
        prop->on_camera_update();
    }
}

FaceVectorProperty* VisualMesh::add_face_vectors(std::string_view name, const std::vector<glm::vec3>& vs) {
    auto  property = std::make_unique<FaceVectorProperty>(this, vs);
    auto& slot     = m_vector_properties[std::string(name)];
    slot           = std::move(property);
    return slot.get();
}

FaceColorProperty* VisualMesh::add_face_colors(std::string_view name, const std::vector<glm::vec3>& colors) {
    auto  property = std::make_unique<FaceColorProperty>(this, colors);
    auto& slot     = m_color_properties[std::string(name)];
    slot           = std::move(property);
    return slot.get();
}

VisualPointCloud::VisualPointCloud(const std::vector<glm::vec3>& positions, const Renderer& renderer)
    : Drawable(&renderer, BoundingBox(positions)) {

    Mesh sphere_mesh = create_sphere(10, 10);
    sphere_mesh.scale({m_init_radius, m_init_radius, m_init_radius});
    set_smooth_normals(sphere_mesh);
    m_spheres = std::make_unique<InstancedMesh>(sphere_mesh, positions.size(), renderer);
    std::vector<glm::mat4x4> transforms;

    for (const auto& p : positions) {
        glm::mat4x4 t(1.0f);
        t[3][0] = p.x;
        t[3][1] = p.y;
        t[3][2] = p.z;
        transforms.push_back(t);
    }
    m_spheres->set_instance_data(transforms, {1, 0, 0});
    m_spheres->upload_instance_data();
    //  configure_render_pipeline();
}

} // namespace rr
