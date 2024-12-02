#include "InstancedMesh.h"

#include "Camera.h"
#include "Mesh.h"
#include "Renderer.h"

#include <iostream>

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

InstancedMesh::InstancedMesh(Mesh mesh, size_t num_instances, const Renderer& renderer)
    : Drawable(&renderer, BoundingBox(mesh.positions)), m_mesh(mesh),
      m_instance_data(num_instances, InstanceData{glm::mat4(1.0f), glm::vec4(0.5, 0.5, 0.5, 1.0f)}) {

    configure_render_pipeline();
}

void InstancedMesh::release() {
    if (m_vertexBuffer == nullptr) {
        return;
    }
    wgpuBufferDestroy(m_vertexBuffer);
    wgpuBufferRelease(m_vertexBuffer);
    wgpuBufferDestroy(m_instanceBuffer);
    wgpuBufferRelease(m_instanceBuffer);
    wgpuBufferDestroy(m_uniformBuffer);
    wgpuBufferRelease(m_uniformBuffer);
    wgpuBindGroupRelease(m_bindGroup);
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

/*
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
    @location(2) world_pos: vec3f,    // For light calculations
    @location(3) view_pos: vec3f,     // For view-dependent effects
}

struct Uniforms {
    projection_matrix: mat4x4f,
    view_matrix: mat4x4f,
}

struct Light {
    direction: vec3f,
    color: vec3f,
    intensity: f32,
}

struct Material {
    ambient: f32,
    diffuse: f32,
    specular: f32,
    shininess: f32,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

// Extract rotation part of view matrix (upper 3x3)
fn get_view_rotation(view_matrix: mat4x4f) -> mat3x3f {
    return mat3x3f(
        view_matrix[0].xyz,
        view_matrix[1].xyz,
        view_matrix[2].xyz
    );
}

fn calculate_blinn_phong(normal: vec3f, light: Light, material: Material, view_dir: vec3f, world_pos: vec3f) -> vec3f {
    let light_dir = normalize(light.direction);

    // Ambient
    let ambient = light.color * material.ambient;

    // Diffuse
    let diff = max(dot(-light_dir, normal), 0.0);
    let diffuse = light.color * (diff * material.diffuse);

    // Specular (Blinn-Phong)
    let halfway_dir = normalize(-light_dir + view_dir);
    let spec = pow(max(dot(normal, halfway_dir), 0.0), material.shininess);
    let specular = light.color * (spec * material.specular);

    return (ambient + diffuse + specular) * light.intensity;
}

fn aces_tone_mapping(color: vec3f) -> vec3f {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), vec3f(0.0), vec3f(1.0));
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
    output.world_pos = world_pos.xyz;
    output.view_pos = (uniforms.view_matrix * world_pos).xyz;

    // Transform normal to world space
    output.world_normal = normalize((model_matrix * vec4f(input.normal, 0.0)).xyz);
    output.color = input.instance_color;

    return output;
}

@fragment
fn fs_main(@builtin(front_facing) is_front: bool, input: VertexOutput) -> @location(0) vec4f {
    // Flip the normal if we're looking at a back face
    let normal = (f32(is_front) * 2.0 - 1.0) * normalize(input.world_normal);
    let view_dir = normalize(-input.view_pos);

    // Get view rotation to make lights camera-relative
    let view_rotation = get_view_rotation(uniforms.view_matrix);

    // Define material properties with reduced intensity
    let material = Material(
        0.1,    // ambient (reduced from 0.2)
        0.5,    // diffuse (reduced from 0.7)
        0.3,    // specular (reduced from 0.5)
        32.0    // shininess (increased for tighter highlights)
    );

    // Camera-space light directions (transformed by view rotation)
    let key_dir = -(view_rotation * vec3f(-0.5, -0.8, -0.5));
    let fill_dir = -(view_rotation * vec3f(0.8, -0.2, 0.3));
    let rim_dir = -(view_rotation * vec3f(-0.2, 0.5, 0.8));

    // Key light (main illumination)
    let keyLight = Light(
        normalize(key_dir),            // camera-relative direction
        vec3f(1.0, 0.98, 0.95),       // color (warm white)
        0.5                           // intensity (reduced from 0.7)
    );

    // Fill light
    let fillLight = Light(
        normalize(fill_dir),           // camera-relative direction
        vec3f(0.9, 0.9, 1.0),         // color (cool white)
        0.25                          // intensity (reduced)
    );

    // Rim light
    let rimLight = Light(
        normalize(rim_dir),            // camera-relative direction
        vec3f(1.0, 1.0, 1.0),         // color
        0.15                          // intensity (reduced)
    );

    // Calculate lighting contributions
    let key_contribution = calculate_blinn_phong(normal, keyLight, material, view_dir, input.world_pos);
    let fill_contribution = calculate_blinn_phong(normal, fillLight, material, view_dir, input.world_pos);
    let rim_contribution = calculate_blinn_phong(normal, rimLight, material, view_dir, input.world_pos);

    // Edge enhancement using fresnel (reduced intensity)
    let fresnel = pow(1.0 - abs(dot(normal, view_dir)), 3.0) * 0.1;  // reduced from 0.2

    // Combine all lighting with instance color
    let total_lighting = key_contribution + fill_contribution + rim_contribution;
    let frag_color = input.color.rgb * total_lighting + fresnel;

    // Additional intensity reduction before tone mapping
    let reduced_intensity = frag_color * 0.8;  // Global intensity reduction

    // Tone mapping and gamma correction
    let mapped_color = aces_tone_mapping(reduced_intensity);
    let corrected_color = pow(mapped_color, vec3f(1.0/2.2));

    return vec4f(corrected_color, input.color.a);
}
)";
 */

void InstancedMesh::configure_render_pipeline() {
    release();
    const Renderer& renderer = *m_renderer;

    std::vector<InstancedMeshVertexAttributes> vertex_attributes = create_vertex_attributes(m_mesh);
    m_num_attr_verts                                             = vertex_attributes.size();

    // Create vertex buffer
    WGPUBufferDescriptor vbDesc = {};
    vbDesc.size                 = vertex_attributes.size() * sizeof(InstancedMeshVertexAttributes);
    vbDesc.usage                = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    vbDesc.mappedAtCreation     = false;
    m_vertexBuffer              = wgpuDeviceCreateBuffer(renderer.m_device, &vbDesc);
    wgpuQueueWriteBuffer(renderer.m_queue, m_vertexBuffer, 0, vertex_attributes.data(), vbDesc.size);

    // Create instance buffer
    WGPUBufferDescriptor ibDesc = {};
    ibDesc.size                 = m_instance_data.size() * sizeof(InstanceData);
    ibDesc.usage                = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    ibDesc.mappedAtCreation     = false;
    m_instanceBuffer            = wgpuDeviceCreateBuffer(renderer.m_device, &ibDesc);

    // Create uniform buffer
    WGPUBufferDescriptor ubDesc = {};
    ubDesc.size                 = sizeof(InstancedMeshUniforms);
    ubDesc.usage                = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    ubDesc.mappedAtCreation     = false;
    m_uniformBuffer             = wgpuDeviceCreateBuffer(renderer.m_device, &ubDesc);

    // Create shader module
    WGPUShaderModuleDescriptor     shaderDesc     = {};
    WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
    shaderCodeDesc.chain.next                     = nullptr;
    shaderCodeDesc.chain.sType                    = WGPUSType_ShaderModuleWGSLDescriptor;
    shaderDesc.nextInChain                        = &shaderCodeDesc.chain;
    shaderCodeDesc.code                           = shaderCode;
    WGPUShaderModule shaderModule                 = wgpuDeviceCreateShaderModule(renderer.m_device, &shaderDesc);

    // Vertex attributes
    std::vector<WGPUVertexAttribute> vertexAttribs;

    static_assert(offsetof(InstancedMeshVertexAttributes, position) == 0, "Position offset is not zero.");
    static_assert(offsetof(InstancedMeshVertexAttributes, normal) == 12, "Normal offset is not as expected.");

    // Per-vertex attributes
    WGPUVertexAttribute positionAttrib;
    positionAttrib.shaderLocation = 0;
    positionAttrib.format         = WGPUVertexFormat_Float32x3;
    positionAttrib.offset         = offsetof(InstancedMeshVertexAttributes, position);
    vertexAttribs.push_back(positionAttrib);

    WGPUVertexAttribute normalAttrib;
    normalAttrib.shaderLocation = 1;
    normalAttrib.format         = WGPUVertexFormat_Float32x3;
    normalAttrib.offset         = offsetof(InstancedMeshVertexAttributes, normal);
    vertexAttribs.push_back(normalAttrib);

    // Per-instance attributes (transform matrix rows and color)
    for (uint32_t i = 0; i < 4; i++) {
        WGPUVertexAttribute matrixRowAttrib;
        matrixRowAttrib.shaderLocation = 2 + i;
        matrixRowAttrib.format         = WGPUVertexFormat_Float32x4;
        matrixRowAttrib.offset         = sizeof(float) * 4 * i;
        vertexAttribs.push_back(matrixRowAttrib);
    }

    WGPUVertexAttribute colorAttrib;
    colorAttrib.shaderLocation = 6;
    colorAttrib.format         = WGPUVertexFormat_Float32x4;
    colorAttrib.offset         = sizeof(glm::mat4);
    vertexAttribs.push_back(colorAttrib);

    // Vertex buffer layout
    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributeCount         = 2; // position and normal
    vertexBufferLayout.attributes             = vertexAttribs.data();
    vertexBufferLayout.arrayStride            = sizeof(InstancedMeshVertexAttributes);
    vertexBufferLayout.stepMode               = WGPUVertexStepMode_Vertex;

    // Instance buffer layout
    WGPUVertexBufferLayout instanceBufferLayout = {};
    instanceBufferLayout.attributeCount         = 5;                        // 4 for transform matrix rows + 1 for color
    instanceBufferLayout.attributes             = vertexAttribs.data() + 2; // Skip vertex attributes
    instanceBufferLayout.arrayStride            = sizeof(InstanceData);
    instanceBufferLayout.stepMode               = WGPUVertexStepMode_Instance;

    std::vector<WGPUVertexBufferLayout> bufferLayouts = {vertexBufferLayout, instanceBufferLayout};

    // Pipeline descriptor
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.vertex.bufferCount           = bufferLayouts.size();
    pipelineDesc.vertex.buffers               = bufferLayouts.data();
    pipelineDesc.vertex.module                = shaderModule;
    pipelineDesc.vertex.entryPoint            = "vs_main";
    pipelineDesc.vertex.constantCount         = 0;
    pipelineDesc.vertex.constants             = nullptr;

    pipelineDesc.primitive.topology         = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace        = WGPUFrontFace_CCW;
    // pipelineDesc.primitive.cullMode         = WGPUCullMode_Back;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;

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
    pipelineDesc.layout                     = layout;

    // Create bind group
    WGPUBindGroupEntry binding = {};
    binding.binding            = 0;
    binding.buffer             = m_uniformBuffer;
    binding.offset             = 0;
    binding.size               = sizeof(InstancedMeshUniforms);

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout                  = bindGroupLayout;
    bindGroupDesc.entryCount              = 1;
    bindGroupDesc.entries                 = &binding;
    m_bindGroup                           = wgpuDeviceCreateBindGroup(renderer.m_device, &bindGroupDesc);

    m_pipeline = wgpuDeviceCreateRenderPipeline(renderer.m_device, &pipelineDesc);
    wgpuShaderModuleRelease(shaderModule);

    on_camera_update();
}

void InstancedMesh::draw(WGPURenderPassEncoder render_pass) {
    // Update instance buffer with current transforms and colors
    wgpuQueueWriteBuffer(m_renderer->m_queue, m_instanceBuffer, 0, m_instance_data.data(),
                         m_instance_data.size() * sizeof(InstanceData));

    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline);

    // Bind vertex buffer to slot 0
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, m_vertexBuffer, 0,
                                         m_num_attr_verts * sizeof(InstancedMeshVertexAttributes));

    // Bind instance buffer to slot 1
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, m_instanceBuffer, 0,
                                         m_instance_data.size() * sizeof(InstanceData));

    // Set binding group for uniforms
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, m_bindGroup, 0, nullptr);

    // Draw call with instancing
    // Parameters:
    // 1. Number of vertices per instance
    // 2. Number of instances
    // 3. First vertex
    // 4. First instance
    wgpuRenderPassEncoderDraw(render_pass, uint32_t(m_num_attr_verts), uint32_t(m_instance_data.size()), 0, 0);
}

void InstancedMesh::on_camera_update() {
    m_uniforms.viewMatrix = m_renderer->m_camera.transform();

    float aspect_ratio          = 1;
    float far_plane             = 100.0f;
    float near_plane            = 0.01f;
    float fov                   = glm::radians(45.0f);
    m_uniforms.projectionMatrix = glm::perspective(fov, aspect_ratio, near_plane, far_plane);

    wgpuQueueWriteBuffer(m_renderer->m_queue, m_uniformBuffer, 0, &m_uniforms, sizeof(InstancedMeshUniforms));
}

} // namespace rr
