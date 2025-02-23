#include "InstancedMesh.h"

#include "Camera.h"
#include "Mesh.h"
#include "Renderer.h"

namespace rr {

struct InstancedMeshVertexAttributes {
    glm::vec3 position;
    glm::vec3 normal;
};

static_assert(sizeof(InstanceData) == 80, "InstanceData size is not as expected.");
static_assert(offsetof(InstanceData, transform) == 0, "Transform offset is not zero.");
static_assert(offsetof(InstanceData, color) == 64, "Color offset is not as expected.");
// static_assert(alignof(InstancedMeshVertexAttributes) % 16 == 0, "InstancedMeshVertexAttributes alignment is not as
// expected.");

static std::vector<InstancedMeshVertexAttributes> create_vertex_attributes(const Mesh& mesh) {
    std::vector<InstancedMeshVertexAttributes> vertex_attributes;
    vertex_attributes.reserve(3 * mesh.num_faces());

    assert(!mesh.normal_faces.empty());

    size_t num_faces = mesh.num_faces();
    for (size_t i = 0; i < num_faces; ++i) {
        const auto& f  = mesh.position_faces[i];
        const auto& nf = mesh.normal_faces[i];

        assert(f.size() == nf.size());
        assert(f.size() >= 3);

        // For faces with more than 3 vertices, we need to triangulate
        // We use fan triangulation: connect first vertex to all other vertices in sequence
        size_t num_triangles = f.size() - 2; // Number of triangles after fan triangulation

        for (size_t j = 0; j < num_triangles; ++j) {
            // size of vertex attributes
            vertex_attributes.push_back({mesh.positions[f[0]], mesh.normals[nf[0]]});
            vertex_attributes.push_back({mesh.positions[f[j + 1]], mesh.normals[nf[j + 1]]});
            vertex_attributes.push_back({mesh.positions[f[j + 2]], mesh.normals[nf[j + 2]]});
        }
    }
    return vertex_attributes;
}

InstancedMesh::InstancedMesh(const Mesh& mesh, size_t num_instances, const Renderer& renderer)
    : Drawable(&renderer, BoundingBox(mesh.positions)), m_mesh(mesh),
      m_instance_data(num_instances, InstanceData{glm::mat4(1.0f), glm::vec4(0.5, 0.5, 0.5, 1.0f)}) {

    configure_render_pipeline();
}

void InstancedMesh::release() {
    if (m_vertex_buffer == nullptr) {
        return;
    }
    wgpuBufferDestroy(m_vertex_buffer);
    wgpuBufferRelease(m_vertex_buffer);
    wgpuBufferDestroy(m_instance_buffer);
    wgpuBufferRelease(m_instance_buffer);
    wgpuBufferDestroy(m_uniform_buffer);
    wgpuBufferRelease(m_uniform_buffer);
    wgpuBindGroupRelease(m_bind_group);
    wgpuRenderPipelineRelease(m_pipeline);
}

InstancedMesh::~InstancedMesh() {
    release();
}

const char* shaderCode = R"(
struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) instance_transform_0: vec4f,
    @location(3) instance_transform_1: vec4f,
    @location(4) instance_transform_2: vec4f,
    @location(5) instance_transform_3: vec4f,
    @location(6) instance_color: vec4f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) world_normal: vec3f,
    @location(1) color: vec4f,
    @location(2) world_pos: vec3f,
    @location(3) view_pos: vec3f,
}

struct Uniforms {
    projection_matrix: mat4x4f,
    view_matrix: mat4x4f,
}

struct Light {
    position: vec3f,
    color: vec3f,
    intensity: f32,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

fn calculate_lighting(light: Light, normal: vec3f, view_pos: vec3f, view_dir: vec3f) -> vec3f {
    let light_dir = normalize(light.position - view_pos);

    // Reduced diffuse multiplier
    let diff = max(dot(normal, light_dir), 0.0);
    let diffuse = diff * light.color * 0.8;  // Reduced from 1.5

    // Reduced specular
    let reflect_dir = reflect(-light_dir, normal);
    let spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);
    let specular = spec * vec3f(0.3) * light.color;  // Reduced from 0.7

    // Keep the gentle distance falloff
    let distance = length(light.position - view_pos);
    let attenuation = 1.0 / (1.0 + 0.0005 * distance);

    return (diffuse + specular) * light.intensity * attenuation;
}

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    let model_matrix = mat4x4f(
        input.instance_transform_0,
        input.instance_transform_1,
        input.instance_transform_2,
        input.instance_transform_3
    );

    let world_pos = model_matrix * vec4f(input.position, 1.0);
    output.position = uniforms.projection_matrix * uniforms.view_matrix * world_pos;
    output.view_pos = (uniforms.view_matrix * world_pos).xyz;
    output.world_pos = world_pos.xyz;
    let world_normal = normalize((model_matrix * vec4f(input.normal, 0.0)).xyz);
    output.world_normal = (uniforms.view_matrix * vec4f(world_normal, 0.0)).xyz;
    output.color = input.instance_color;

    return output;
}

@fragment
fn fs_main(@builtin(front_facing) is_front: bool, input: VertexOutput) -> @location(0) vec4f {
    let normal = (f32(is_front) * 2.0 - 1.0) * normalize(input.world_normal);
    let view_dir = normalize(-input.view_pos);

    // Reduced light intensities
    let key_light = Light(
        vec3f(10.0, 10.0, 10.0),
        vec3f(1.0, 0.98, 0.95),     // warm white
        0.8                         // Reduced from 1.4
    );

    let fill_light = Light(
        vec3f(-6.0, 4.0, 8.0),
        vec3f(0.9, 0.9, 1.0),      // cool white
        0.4                        // Reduced from 0.7
    );

    let back_light = Light(
        vec3f(-2.0, 6.0, -8.0),
        vec3f(1.0, 1.0, 1.0),      // white
        0.3                        // Reduced from 0.5
    );

    let base_color = input.color.rgb;
    var result = vec3f(0.0);

    result += calculate_lighting(key_light, normal, input.view_pos, view_dir) * base_color;
    result += calculate_lighting(fill_light, normal, input.view_pos, view_dir) * base_color;

    let rim_effect = 1.0 - max(dot(view_dir, normal), 0.0);
    result += calculate_lighting(back_light, normal, input.view_pos, view_dir) * rim_effect * base_color;

    // Reduced ambient
    let ambient = vec3f(0.15) * base_color;  // Reduced from 0.25
    result += ambient;

    return vec4f(result, input.color.a);
}
)";

void InstancedMesh::configure_render_pipeline() {
    release();
    const Renderer& renderer = *m_renderer;

    std::vector<InstancedMeshVertexAttributes> vertex_attributes = create_vertex_attributes(m_mesh);
    m_num_attr_verts = vertex_attributes.size();

    // Create vertex buffer
    WGPUBufferDescriptor vb_desc = {};
    vb_desc.size = vertex_attributes.size() * sizeof(InstancedMeshVertexAttributes);
    vb_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    vb_desc.mappedAtCreation = false;
    m_vertex_buffer = wgpuDeviceCreateBuffer(renderer.m_device, &vb_desc);
    wgpuQueueWriteBuffer(renderer.m_queue, m_vertex_buffer, 0, vertex_attributes.data(), vb_desc.size);

    // Create instance buffer
    WGPUBufferDescriptor ib_desc = {};
    ib_desc.size = m_instance_data.size() * sizeof(InstanceData);
    ib_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    ib_desc.mappedAtCreation = false;
    m_instance_buffer = wgpuDeviceCreateBuffer(renderer.m_device, &ib_desc);

    // Create uniform buffer
    WGPUBufferDescriptor ub_desc = {};
    ub_desc.size = sizeof(InstancedMeshUniforms);
    ub_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    ub_desc.mappedAtCreation = false;
    m_uniform_buffer = wgpuDeviceCreateBuffer(renderer.m_device, &ub_desc);

    // Create shader module
    WGPUShaderModuleDescriptor shader_desc = {};
    WGPUShaderModuleWGSLDescriptor shader_code_desc = {};
    shader_code_desc.chain.next = nullptr;
    shader_code_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shader_desc.nextInChain = &shader_code_desc.chain;
    shader_code_desc.code = to_string_view(shaderCode);
    WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(renderer.m_device, &shader_desc);

    // Vertex attributes
    std::vector<WGPUVertexAttribute> vertex_attribs;

    // Per-vertex attributes
    WGPUVertexAttribute position_attrib;
    position_attrib.shaderLocation = 0;
    position_attrib.format = WGPUVertexFormat_Float32x3;
    position_attrib.offset = offsetof(InstancedMeshVertexAttributes, position);
    vertex_attribs.push_back(position_attrib);

    WGPUVertexAttribute normal_attrib;
    normal_attrib.shaderLocation = 1;
    normal_attrib.format = WGPUVertexFormat_Float32x3;
    normal_attrib.offset = offsetof(InstancedMeshVertexAttributes, normal);
    vertex_attribs.push_back(normal_attrib);

    // Per-instance attributes (transform matrix rows and color)
    for (uint32_t i = 0; i < 4; i++) {
        WGPUVertexAttribute matrix_row_attrib;
        matrix_row_attrib.shaderLocation = 2 + i;
        matrix_row_attrib.format = WGPUVertexFormat_Float32x4;
        matrix_row_attrib.offset = sizeof(float) * 4 * i;
        vertex_attribs.push_back(matrix_row_attrib);
    }

    WGPUVertexAttribute color_attrib;
    color_attrib.shaderLocation = 6;
    color_attrib.format = WGPUVertexFormat_Float32x4;
    color_attrib.offset = sizeof(glm::mat4);
    vertex_attribs.push_back(color_attrib);

    // Vertex buffer layout
    WGPUVertexBufferLayout vertex_buffer_layout = {};
    vertex_buffer_layout.attributeCount = 2; // position and normal
    vertex_buffer_layout.attributes = vertex_attribs.data();
    vertex_buffer_layout.arrayStride = sizeof(InstancedMeshVertexAttributes);
    vertex_buffer_layout.stepMode = WGPUVertexStepMode_Vertex;

    // Instance buffer layout
    WGPUVertexBufferLayout instance_buffer_layout = {};
    instance_buffer_layout.attributeCount = 5; // 4 for transform matrix rows + 1 for color
    instance_buffer_layout.attributes = vertex_attribs.data() + 2; // Skip vertex attributes
    instance_buffer_layout.arrayStride = sizeof(InstanceData);
    instance_buffer_layout.stepMode = WGPUVertexStepMode_Instance;

    std::vector<WGPUVertexBufferLayout> buffer_layouts = {vertex_buffer_layout, instance_buffer_layout};

    // Pipeline descriptor
    WGPURenderPipelineDescriptor pipeline_desc = {};
    pipeline_desc.vertex.bufferCount = buffer_layouts.size();
    pipeline_desc.vertex.buffers = buffer_layouts.data();
    pipeline_desc.vertex.module = shader_module;
    pipeline_desc.vertex.entryPoint = to_string_view("vs_main");
    pipeline_desc.vertex.constantCount = 0;
    pipeline_desc.vertex.constants = nullptr;

    pipeline_desc.primitive.topology         = WGPUPrimitiveTopology_TriangleList;
    pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipeline_desc.primitive.frontFace        = WGPUFrontFace_CCW;
    // pipelineDesc.primitive.cullMode         = WGPUCullMode_Back;
    pipeline_desc.primitive.cullMode = WGPUCullMode_None;

    WGPUFragmentState fragmentState = {};
    pipeline_desc.fragment           = &fragmentState;
    fragmentState.module            = shader_module;
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
    colorTarget.format               = renderer.m_swap_chain_format;
    colorTarget.blend                = &blendState;
    colorTarget.writeMask            = WGPUColorWriteMask_All;

    fragmentState.targetCount = 1;
    fragmentState.targets     = &colorTarget;

    WGPUDepthStencilState depthStencilState = {};
    depthStencilState.depthCompare          = WGPUCompareFunction_Less;
    depthStencilState.depthWriteEnabled     = WGPUOptionalBool_True;
    depthStencilState.format                = renderer.m_depth_texture_format;
    depthStencilState.stencilReadMask       = 0;
    depthStencilState.stencilWriteMask      = 0;

    pipeline_desc.depthStencil = &depthStencilState;

    pipeline_desc.multisample.count                  = 1;
    pipeline_desc.multisample.mask                   = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = false;

    // Create binding layout
    WGPUBindGroupLayoutEntry bindingLayout = {};
    bindingLayout.binding                  = 0;
    bindingLayout.visibility               = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindingLayout.buffer.type              = WGPUBufferBindingType_Uniform;
    bindingLayout.buffer.minBindingSize    = sizeof(InstancedMeshUniforms);

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.entryCount                    = 1;
    bindGroupLayoutDesc.entries                       = &bindingLayout;
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(renderer.m_device, &bindGroupLayoutDesc);

    // Create pipeline layout
    WGPUPipelineLayoutDescriptor layoutDesc = {};
    layoutDesc.bindGroupLayoutCount         = 1;
    layoutDesc.bindGroupLayouts             = &bindGroupLayout;
    WGPUPipelineLayout layout               = wgpuDeviceCreatePipelineLayout(renderer.m_device, &layoutDesc);
    pipeline_desc.layout                     = layout;

    // Create bind group
    WGPUBindGroupEntry binding = {};
    binding.binding            = 0;
    binding.buffer             = m_uniform_buffer;
    binding.offset             = 0;
    binding.size               = sizeof(InstancedMeshUniforms);

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout                  = bindGroupLayout;
    bindGroupDesc.entryCount              = 1;
    bindGroupDesc.entries                 = &binding;
    m_bind_group                           = wgpuDeviceCreateBindGroup(renderer.m_device, &bindGroupDesc);

    m_pipeline = wgpuDeviceCreateRenderPipeline(renderer.m_device, &pipeline_desc);
    wgpuShaderModuleRelease(shader_module);

    on_camera_update();
}

void InstancedMesh::upload_instance_data() {
    wgpuQueueWriteBuffer(m_renderer->m_queue, m_instance_buffer, 0, m_instance_data.data(),
                         m_instance_data.size() * sizeof(InstanceData));
}

void InstancedMesh::draw(WGPURenderPassEncoder render_pass) {
    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline);

    // Bind vertex buffer to slot 0
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, m_vertex_buffer, 0,
                                         m_num_attr_verts * sizeof(InstancedMeshVertexAttributes));

    // Bind instance buffer to slot 1
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, m_instance_buffer, 0,
                                         m_instance_data.size() * sizeof(InstanceData));

    // Set binding group for uniforms
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, m_bind_group, 0, nullptr);

    // Draw call with instancing
    // Parameters:
    // 1. Number of vertices per instance
    // 2. Number of instances
    // 3. First vertex
    // 4. First instance
    wgpuRenderPassEncoderDraw(render_pass, uint32_t(m_num_attr_verts), uint32_t(m_instance_data.size()), 0, 0);
}

void InstancedMesh::on_camera_update() {
    m_uniforms.view_matrix = m_renderer->m_camera.transform();

    float aspect_ratio          = static_cast<float>(m_renderer->m_width) / static_cast<float>(m_renderer->m_height);
    float far_plane             = 100.0f;
    float near_plane            = 0.01f;
    float fov                   = glm::radians(45.0f);
    m_uniforms.projection_matrix = glm::perspective(fov, aspect_ratio, near_plane, far_plane);

    wgpuQueueWriteBuffer(m_renderer->m_queue, m_uniform_buffer, 0, &m_uniforms, sizeof(InstancedMeshUniforms));
}

} // namespace rr
