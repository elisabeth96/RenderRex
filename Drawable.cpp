// contains all the unser interface funtions for the renderrex library

#include "Drawable.h"
#include "Camera.h"
#include "Mesh.h"
#include "Primitives.h"
#include "Renderer.h"
#include "ShaderCode.h"

#include <iostream>

namespace rr {

/**
 * A structure that describes the data layout in the vertex buffer
 * We do not instantiate it but use it in `sizeof` and `offsetof`
 */
struct VertexAttributes {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 bary;
    glm::vec3 edge_mask;
};

std::vector<VertexAttributes> create_vertex_attributes(const Mesh& mesh) {
    std::vector<VertexAttributes> vertex_attributes;
    vertex_attributes.reserve(3 * mesh.num_faces());

    assert(!mesh.normal_faces.empty());

    size_t num_faces = mesh.num_faces();
    for (size_t i = 0; i < num_faces; ++i) {
        const auto& f  = mesh.position_faces[i];
        const auto& nf = mesh.normal_faces[i];

        // For faces with more than 3 vertices, we need to triangulate
        // We use fan triangulation: connect first vertex to all other vertices in sequence
        size_t num_triangles = f.size() - 2; // Number of triangles after fan triangulation

        for (size_t j = 0; j < num_triangles; ++j) {
            // For each triangle in the fan, we need to determine which edges are real
            // and which are internal triangulation edges

            // A triangle has vertices: center(0), j+1, j+2
            // An edge is real if:
            // - It's part of the original face boundary
            // - For fan triangulation, this means either:
            //   a) It connects to consecutive vertices in the original face
            //   b) It's the first or last edge connected to the center vertex

            glm::vec3 edge_mask(1.0f); // Start with all edges masked (internal)

            // Edge bc (between vertices j+1 and j+2)
            // This is a real edge if j+1 and j+2 were consecutive in original face
            edge_mask[0] = 0.0f; // bc is always a real edge in the original face

            // Edge ca (between vertex j+2 and center)
            // This is a real edge if it's the last triangle in the fan
            if (j == num_triangles - 1) {
                edge_mask[1] = 0.0f; // Last edge back to center is real
            }

            // Edge ab (between center and vertex j+1)
            // This is a real edge if it's the first triangle in the fan
            if (j == 0) {
                edge_mask[2] = 0.0f; // First edge from center is real
            }

            // Add the three vertices for this triangle
            // Center vertex (same for all triangles in the fan)
            vertex_attributes.push_back({mesh.positions[f[0]], mesh.normals[nf[0]],
                                         glm::vec3(1.0f, 0.0f, 0.0f), // Barycentric coordinates (1,0,0)
                                         edge_mask});

            // First edge vertex
            vertex_attributes.push_back({mesh.positions[f[j + 1]], mesh.normals[nf[j + 1]],
                                         glm::vec3(0.0f, 1.0f, 0.0f), // Barycentric coordinates (0,1,0)
                                         edge_mask});

            // Second edge vertex
            vertex_attributes.push_back({mesh.positions[f[j + 2]], mesh.normals[nf[j + 2]],
                                         glm::vec3(0.0f, 0.0f, 1.0f), // Barycentric coordinates (0,0,1)
                                         edge_mask});
        }
    }

    return vertex_attributes;
}

RenderMesh::RenderMesh(const Mesh& mesh, const Renderer& renderer)
    : Drawable(&renderer, BoundingBox(mesh.positions)), m_mesh(mesh) {

    configure_render_pipeline();
}

void RenderMesh::release() {
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

RenderMesh::~RenderMesh() {
    release();
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

void RenderMesh::configure_render_pipeline() {
    release();
    const Renderer& renderer = *m_renderer;

    std::vector<VertexAttributes> vertex_attributes = create_vertex_attributes(m_mesh);

    for (auto& attribute : m_attributes) {
        attribute.second->generate_attributes(vertex_attributes);
    }

    m_num_attr_verts = vertex_attributes.size();

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

    // Bary attribute
    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[2].offset         = offsetof(VertexAttributes, bary);

    // Edge mask attribute
    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format         = WGPUVertexFormat_Float32x3;
    vertexAttribs[3].offset         = offsetof(VertexAttributes, edge_mask);

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

    // This has to be called here because the camera uniforms are cleared when reconfiguring
    // the pipeline. When the mesh is registered this is called from the renderer, but when
    // an attribute is registered the pipeline is reconfigured and the camera uniforms are lost.
    on_camera_update();
}

void RenderMesh::draw(WGPURenderPassEncoder render_pass) {
    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, m_vertexBuffer, 0,
                                         m_num_attr_verts * sizeof(VertexAttributes));

    // Set binding group
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, m_bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDraw(render_pass, uint32_t(m_num_attr_verts), 1, 0, 0);
}

void RenderMesh::on_camera_update() {
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

FaceVectorAttribute* RenderMesh::add_face_attribute(std::string name, const std::vector<glm::vec3>& vs) {
    std::vector<glm::vec3> face_centers(m_mesh.num_faces());

    double average_edge_length = 0.0;
    size_t total_edges = 0;  // Keep track of actual number of edges processed

    for (size_t i = 0; i < m_mesh.num_faces(); ++i) {
        const auto& f = m_mesh.position_faces[i];

        // Calculate face center
        glm::vec3 center(0.0f);
        for (size_t j = 0; j < f.size(); ++j) {
            center += m_mesh.positions[f[j]];
        }
        center /= float(f.size());
        face_centers[i] = center;

        // Calculate length of each edge in the face
        for (size_t j = 0; j < f.size(); ++j) {
            // Get current vertex and next vertex (wrapping around)
            const glm::vec3& p0 = m_mesh.positions[f[j]];
            const glm::vec3& p1 = m_mesh.positions[f[(j + 1) % f.size()]];

            // Calculate edge length
            float edge_length = glm::length(p1 - p0);

            // Update running average using Welford's algorithm
            total_edges++;
            double delta = edge_length - average_edge_length;
            average_edge_length += delta / total_edges;
        }
    }

    printf("Average edge length: %f\n", average_edge_length);

    std::unique_ptr<FaceVectorAttribute> attribute = std::make_unique<FaceVectorAttribute>(vs);
    attribute->m_face_centers                      = std::move(face_centers);
    attribute->m_scale                             = 0.5f * float(average_edge_length);

    auto& slot = m_attributes[name];
    slot       = std::move(attribute);

    configure_render_pipeline();

    return dynamic_cast<FaceVectorAttribute*>(slot.get());
}

void FaceVectorAttribute::generate_attributes(std::vector<VertexAttributes>& vertex_attributes) {
    static Mesh cylinder = create_cylinder().triangulate();
    static Mesh cone = create_cone().triangulate();

    // Scale factors for cylinder and cone
    float vector_length = m_scale;
    float cylinder_radius = 0.05f * vector_length;
    float cylinder_length = 0.7f * vector_length;  // Cylinder takes 70% of total length
    float cone_radius = 0.15f * vector_length;     // Cone is wider than cylinder
    float cone_length = 0.3f * vector_length;      // Cone takes 30% of total length

    for (size_t i = 0; i < m_vectors.size(); ++i) {
        glm::vec3 v = m_vectors[i];
        float v_length = glm::length(v);
        glm::vec3 v_normalized = v / v_length;
        glm::vec3 start = m_face_centers[i];
        glm::vec3 cylinder_end = start + v_normalized * cylinder_length;

        // Calculate rotation
        glm::vec3 y(0, 1, 0);
        glm::vec3 axis = glm::cross(y, v_normalized);
        float length = glm::length(axis);
        float angle = 0.0f;
        if (length < 1e-6f) {
            axis = glm::vec3(1, 0, 0);
            angle = (v_normalized.y < 0) ? glm::pi<float>() : 0.0f;
        } else {
            axis = axis / length;
            angle = acos(glm::dot(y, v_normalized));
        }
        glm::mat3 rot(glm::angleAxis(angle, axis));

        // Draw cylinder - offset the position by half the cylinder length to start from face center
        glm::mat4 cylinder_scale = glm::scale(glm::vec3(cylinder_radius, cylinder_length, cylinder_radius));
        glm::vec3 cylinder_pos = start + v_normalized * (cylinder_length * 0.5f);
        glm::mat4 cylinder_tf = glm::translate(cylinder_pos) * glm::rotate(angle, axis) * cylinder_scale;

        for (size_t j = 0; j < cylinder.num_faces(); ++j) {
            const auto& f = cylinder.position_faces[j];
            const auto& nf = cylinder.normal_faces[j];
            assert(f.size() == 3);
            for (size_t k = 0; k < 3; ++k) {
                glm::vec3 p = cylinder.positions[f[k]];
                p = glm::vec3(cylinder_tf * glm::vec4(p, 1));
                glm::vec3 n = cylinder.normals[nf[k]];
                n = rot * n;
                glm::vec3 b(1);
                glm::vec3 mask(1);
                vertex_attributes.push_back({p, n, b, mask});
            }
        }

        // Draw cone - position should be at the end of the cylinder
        glm::mat4 cone_scale = glm::scale(glm::vec3(cone_radius, cone_length, cone_radius));
        // Move cone up by half its length since it's also centered
        glm::vec3 cone_pos = cylinder_end + v_normalized * (cone_length * 0.5f);
        glm::mat4 cone_tf = glm::translate(cone_pos) * glm::rotate(angle, axis) * cone_scale;

        for (size_t j = 0; j < cone.num_faces(); ++j) {
            const auto& f = cone.position_faces[j];
            const auto& nf = cone.normal_faces[j];
            assert(f.size() == 3);
            for (size_t k = 0; k < 3; ++k) {
                glm::vec3 p = cone.positions[f[k]];
                p = glm::vec3(cone_tf * glm::vec4(p, 1));
                glm::vec3 n = cone.normals[nf[k]];
                n = rot * n;
                glm::vec3 b(1);
                glm::vec3 mask(1);
                vertex_attributes.push_back({p, n, b, mask});
            }
        }
    }
}

} // namespace rr
