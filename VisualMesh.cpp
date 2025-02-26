#include "VisualMesh.h"

#include "Camera.h"
#include "InstancedMesh.h"
#include "Mesh.h"
#include "Primitives.h"
#include "Property.h"
#include "Renderer.h"
#include "ShaderCode.h"
#include "Utils.h"

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
    if (m_vertex_buffer == nullptr) {
        return;
    }
    wgpuBufferDestroy(m_vertex_buffer);
    wgpuBufferRelease(m_vertex_buffer);
    wgpuBufferDestroy(m_uniform_buffer);
    wgpuBufferRelease(m_uniform_buffer);
    wgpuBindGroupRelease(m_bind_group);
    wgpuRenderPipelineRelease(m_pipeline);
}

VisualMesh::~VisualMesh() {
    release();
}

void shaderCompilationCallback(WGPUCompilationInfoRequestStatus, WGPUCompilationInfo const* compilation_info, void*,
                               void*) {
    if (compilation_info) {
        for (uint32_t i = 0; i < compilation_info->messageCount; ++i) {
            WGPUCompilationMessage const& message = compilation_info->messages[i];

            const char* message_type;
            switch (message.type) {
            case WGPUCompilationMessageType_Error:
                message_type = "Error";
                break;
            case WGPUCompilationMessageType_Warning:
                message_type = "Warning";
                break;
            case WGPUCompilationMessageType_Info:
                message_type = "Info";
                break;
            default:
                message_type = "Unknown";
                break;
            }

            std::cerr << message_type << " at line " << message.lineNum << ", column " << message.linePos << ": "
                      << to_string(message.message) << std::endl;
        }
    }
}

WGPUShaderModule createShaderModule(WGPUDevice device, const char* shader_source) {
    WGPUShaderModuleDescriptor     shader_desc{};
    WGPUShaderModuleWGSLDescriptor shader_code_desc{};

    shader_code_desc.chain.next  = nullptr;
    shader_code_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shader_desc.nextInChain      = &shader_code_desc.chain;
    shader_code_desc.code        = to_string_view(shader_source);

    WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(device, &shader_desc);

    WGPUCompilationInfoCallbackInfo callback_info = {};
    callback_info.callback                        = shaderCompilationCallback;
    callback_info.mode                            = WGPUCallbackMode_AllowSpontaneous;

    wgpuShaderModuleGetCompilationInfo(shader_module, callback_info);

    return shader_module;
}

void VisualMesh::configure_render_pipeline() {
    release();
    const Renderer& renderer = *m_renderer;

    m_vertex_attributes = create_vertex_attributes(m_mesh, m_mesh_color);

    WGPUShaderModule shader_module = createShaderModule(renderer.m_device, shaderCode);

    // Vertex fetch
    std::vector<WGPUVertexAttribute> vertex_attribs(5);

    // Position attribute
    vertex_attribs[0].shaderLocation = 0;
    vertex_attribs[0].format         = WGPUVertexFormat_Float32x3;
    vertex_attribs[0].offset         = 0;

    // Normal attribute
    vertex_attribs[1].shaderLocation = 1;
    vertex_attribs[1].format         = WGPUVertexFormat_Float32x3;
    vertex_attribs[1].offset         = offsetof(VisualMeshVertexAttributes, normal);

    // Bary attribute
    vertex_attribs[2].shaderLocation = 2;
    vertex_attribs[2].format         = WGPUVertexFormat_Float32x3;
    vertex_attribs[2].offset         = offsetof(VisualMeshVertexAttributes, bary);

    // Edge mask attribute
    vertex_attribs[3].shaderLocation = 3;
    vertex_attribs[3].format         = WGPUVertexFormat_Float32x3;
    vertex_attribs[3].offset         = offsetof(VisualMeshVertexAttributes, edge_mask);

    vertex_attribs[4].shaderLocation = 4;
    vertex_attribs[4].format         = WGPUVertexFormat_Float32x3;
    vertex_attribs[4].offset         = offsetof(VisualMeshVertexAttributes, color);

    WGPUVertexBufferLayout vertex_buffer_layout = {};
    vertex_buffer_layout.attributeCount         = (uint32_t)vertex_attribs.size();
    vertex_buffer_layout.attributes             = vertex_attribs.data();
    vertex_buffer_layout.arrayStride            = sizeof(VisualMeshVertexAttributes);

    vertex_buffer_layout.stepMode = WGPUVertexStepMode_Vertex;

    WGPURenderPipelineDescriptor pipeline_desc = {};

    pipeline_desc.vertex.bufferCount = 1;
    pipeline_desc.vertex.buffers     = &vertex_buffer_layout;

    pipeline_desc.vertex.module        = shader_module;
    pipeline_desc.vertex.entryPoint    = to_string_view("vs_main");
    pipeline_desc.vertex.constantCount = 0;
    pipeline_desc.vertex.constants     = nullptr;

    pipeline_desc.primitive.topology         = WGPUPrimitiveTopology_TriangleList;
    pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipeline_desc.primitive.frontFace        = WGPUFrontFace_CCW;
    pipeline_desc.primitive.cullMode         = WGPUCullMode_None;

    WGPUFragmentState fragment_state = {};
    pipeline_desc.fragment           = &fragment_state;
    fragment_state.module            = shader_module;
    fragment_state.entryPoint        = to_string_view("fs_main");
    fragment_state.constantCount     = 0;
    fragment_state.constants         = nullptr;

    WGPUBlendState blend_state  = {};
    blend_state.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend_state.color.operation = WGPUBlendOperation_Add;

    blend_state.alpha.srcFactor = WGPUBlendFactor_One;
    blend_state.alpha.dstFactor = WGPUBlendFactor_Zero;
    blend_state.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState color_target = {};
    color_target.format               = renderer.m_swap_chain_format;
    color_target.blend                = &blend_state;
    color_target.writeMask            = WGPUColorWriteMask_All;

    fragment_state.targetCount = 1;
    fragment_state.targets     = &color_target;

    WGPUDepthStencilState depth_stencil_state = {};
    depth_stencil_state.depthCompare          = WGPUCompareFunction_Less;
    depth_stencil_state.depthWriteEnabled     = WGPUOptionalBool_True;
    depth_stencil_state.format                = renderer.m_depth_texture_format;
    depth_stencil_state.stencilReadMask       = 0;
    depth_stencil_state.stencilWriteMask      = 0;

    pipeline_desc.depthStencil = &depth_stencil_state;

    pipeline_desc.multisample.count                  = 1;
    pipeline_desc.multisample.mask                   = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = false;

    // Create binding layout (don't forget to = Default)
    WGPUBindGroupLayoutEntry binding_layout = {};
    binding_layout.binding                  = 0;
    binding_layout.visibility               = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    binding_layout.buffer.type              = WGPUBufferBindingType_Uniform;
    binding_layout.buffer.minBindingSize    = sizeof(VisualMeshUniforms);

    // Create a bind group layout
    WGPUBindGroupLayoutDescriptor bind_group_layout_desc{};
    bind_group_layout_desc.entryCount     = 1;
    bind_group_layout_desc.entries        = &binding_layout;
    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(renderer.m_device, &bind_group_layout_desc);

    // Create the pipeline layout
    WGPUPipelineLayoutDescriptor layout_desc{};
    layout_desc.bindGroupLayoutCount = 1;
    layout_desc.bindGroupLayouts     = (WGPUBindGroupLayout*)&bind_group_layout;
    WGPUPipelineLayout layout        = wgpuDeviceCreatePipelineLayout(renderer.m_device, &layout_desc);
    pipeline_desc.layout             = layout;

    // Create vertex buffer
    WGPUBufferDescriptor buffer_desc = {};
    buffer_desc.size                 = m_vertex_attributes.size() * sizeof(VisualMeshVertexAttributes);
    buffer_desc.usage                = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    buffer_desc.mappedAtCreation     = false;
    m_vertex_buffer                  = wgpuDeviceCreateBuffer(renderer.m_device, &buffer_desc);
    wgpuQueueWriteBuffer(renderer.m_queue, m_vertex_buffer, 0, m_vertex_attributes.data(), buffer_desc.size);

    // Create uniform buffer
    buffer_desc.size             = sizeof(VisualMeshUniforms);
    buffer_desc.usage            = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    buffer_desc.mappedAtCreation = false;
    m_uniform_buffer             = wgpuDeviceCreateBuffer(renderer.m_device, &buffer_desc);

    // Create a binding
    WGPUBindGroupEntry binding{};
    binding.binding = 0;
    binding.buffer  = m_uniform_buffer;
    binding.offset  = 0;
    binding.size    = sizeof(VisualMeshUniforms);

    // A bind group contains one or multiple bindings
    WGPUBindGroupDescriptor bind_group_desc = {};
    bind_group_desc.layout                  = bind_group_layout;
    bind_group_desc.entryCount              = bind_group_layout_desc.entryCount;
    bind_group_desc.entries                 = &binding;
    m_bind_group                            = wgpuDeviceCreateBindGroup(renderer.m_device, &bind_group_desc);

    m_pipeline = wgpuDeviceCreateRenderPipeline(renderer.m_device, &pipeline_desc);
    wgpuShaderModuleRelease(shader_module);

    // This has to be called here because the camera uniforms are cleared when reconfiguring
    // the pipeline. When the mesh is registered this is called from the renderer, but when
    // an attribute is registered the pipeline is reconfigured and the camera uniforms are lost.
    on_camera_update();
}

void VisualMesh::draw(WGPURenderPassEncoder render_pass) {
    if (!m_visible_mesh && !m_show_wireframe)
        return;

    if (m_attributes_dirty) {
        size_t size = m_vertex_attributes.size() * sizeof(VisualMeshVertexAttributes);
        wgpuQueueWriteBuffer(Renderer::get().m_queue, m_vertex_buffer, 0, m_vertex_attributes.data(), size);
        m_attributes_dirty = false;
    }

    if (m_uniforms_dirty) {
        wgpuQueueWriteBuffer(m_renderer->m_queue, m_uniform_buffer, 0, &m_uniforms, sizeof(VisualMeshUniforms));
        m_uniforms_dirty = false;
    }

    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, m_vertex_buffer, 0,
                                         m_vertex_attributes.size() * sizeof(VisualMeshVertexAttributes));

    // Set binding group
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, m_bind_group, 0, nullptr);
    wgpuRenderPassEncoderDraw(render_pass, uint32_t(m_vertex_attributes.size()), 1, 0, 0);

    for (auto& [name, prop] : m_vector_properties) {
        prop->draw(render_pass);
    }
}

void VisualMesh::on_camera_update() {
    m_uniforms.view_matrix       = m_renderer->m_camera.transform();
    m_uniforms.projection_matrix = m_renderer->m_projection;
    m_uniforms_dirty             = true;

    // update vector properties
    for (auto& [name, prop] : m_vector_properties) {
        prop->on_camera_update();
    }
}

void VisualMesh::update_ui(std::string, int) {
    bool update_uniforms = false;
    bool update_color    = false;
    if (m_show_options) {

        update_color |= ImGui::ColorEdit3("Color", (float*)&m_mesh_color);
        update_uniforms |= ImGui::ColorEdit3("Wireframe Color", (float*)&m_uniforms.wireframe_color);

        if (update_color) {
            auto it = std::find_if(m_color_properties.begin(), m_color_properties.end(),
                                   [](const auto& pair) { return pair.second->is_enabled(); });
            if (it == m_color_properties.end()) {
                for (auto& attr : m_vertex_attributes) {
                    attr.color = m_mesh_color;
                }
                m_attributes_dirty = true;
            }
        }
        // face color properties
        if (ImGui::TreeNode("Face Color Properties")) {

            for (auto& [name, prop] : m_color_properties) {
                bool is_enabled          = prop->is_enabled();
                bool face_colors_changed = ImGui::Checkbox(name.c_str(), &is_enabled);
                prop->set_enabled(is_enabled);
                if (face_colors_changed) {
                    update_face_colors(name);
                }
            }
            ImGui::TreePop();
        }

        // vector properties
        std::string changed_name;
        if (ImGui::TreeNode("Vector Properties")) {
            for (auto& [name, prop] : m_vector_properties) {
                bool is_enabled = prop->is_enabled();
                if (ImGui::Checkbox(name.c_str(), &is_enabled)) {
                    changed_name = name;
                    prop->set_enabled(is_enabled);
                }
            }
            ImGui::TreePop();
        }
        if (!changed_name.empty()) {
            for (auto& [name, prop] : m_vector_properties) {
                if (name != changed_name) {
                    prop->set_enabled(false);
                }
            }
        }
    }
    if (update_uniforms)
        m_uniforms_dirty = true;
}

void VisualMesh::set_transform(const glm::mat4& transform) {
    m_uniforms.model_matrix = transform;
    m_uniforms_dirty        = true;
    
    // Update all vector properties to apply the new transform
    for (auto& [name, prop] : m_vector_properties) {
        prop->m_instance_data_dirty = true;
    }
}

const glm::mat4* VisualMesh::get_transform() const {
    return &m_uniforms.model_matrix;
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

void VisualMesh::update_face_colors(const std::string& changed_name) {
    bool using_face_property = false;
    for (auto& [name, prop] : m_color_properties) {
        if (name == changed_name && prop->is_enabled()) {
            using_face_property                  = true;
            const std::vector<glm::vec3>& colors = prop->get_colors();

            size_t num_attributes = m_vertex_attributes.size();
            for (size_t i = 0; i < num_attributes; ++i) {
                m_vertex_attributes[i].color = colors[i / 3];
            }
        } else {
            prop->set_enabled(false);
        }
    }

    if (!using_face_property) {
        size_t num_attributes = m_vertex_attributes.size();
        for (size_t i = 0; i < num_attributes; ++i) {
            m_vertex_attributes[i].color = m_mesh_color;
        }
    }
    m_attributes_dirty = true;
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
    m_spheres->set_instance_data(transforms, {m_color.x, m_color.y, m_color.z});
    m_spheres->upload_instance_data();
    //  configure_render_pipeline();
}

VisualLineNetwork::VisualLineNetwork(const std::vector<glm::vec3>&           positions,
                                     const std::vector<std::pair<int, int>>& lines, const Renderer& renderer)
    : Drawable(&renderer, BoundingBox(positions)), m_positions(positions), m_lines(lines) {
    Mesh line_mesh   = create_cylinder(16).triangulate();
    m_line_mesh      = std::make_unique<InstancedMesh>(line_mesh, lines.size(), renderer);
    Mesh sphere_mesh = create_sphere(16, 16);
    set_smooth_normals(sphere_mesh);
    m_vertices_mesh = std::make_unique<InstancedMesh>(sphere_mesh, positions.size(), renderer);
    compute_transforms();
}

} // namespace rr
