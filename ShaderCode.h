#pragma once

const char* shaderCode = R"shader(
struct VertexInput {
        @location(0) position: vec3f,
        @location(1) normal: vec3f,
        @location(2) bary: vec3f,
        @location(3) wire_limits: vec3f,
};

struct VertexOutput {
        @builtin(position) position: vec4f,
        @location(0) bary: vec3f,
        @location(1) normal: vec3f,
        @location(2) world_pos: vec3f,    // For light calculations
        @location(3) view_pos: vec3f,     // For view-dependent effects
        @location(4) wire_limits: vec3f,
};

struct MyUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
};

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;

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

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
        var out: VertexOutput;
        let modelPos = uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
        out.world_pos = modelPos.xyz;
        out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * modelPos;
        out.normal = normalize((uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz);
        out.bary = in.bary;
        out.wire_limits = in.wire_limits;
        out.view_pos = (uMyUniforms.viewMatrix * modelPos).xyz;
        return out;
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

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
        let normal = normalize(in.normal);
        let view_dir = normalize(-in.view_pos);

        // Define material properties
        let material = Material(
            0.2,    // ambient
            0.7,    // diffuse
            0.5,    // specular
            16.0    // shininess
        );

        // Key light (main illumination)
        let keyLight = Light(
            normalize(vec3f(-0.5, -0.8, -0.5)),  // direction
            vec3f(1.0, 0.98, 0.95),              // color (warm white)
            0.7                                  // intensity
        );

        // Fill light
        let fillLight = Light(
            normalize(vec3f(0.8, -0.2, 0.3)),    // direction
            vec3f(0.9, 0.9, 1.0),                // color (cool white)
            0.5 * 0.7                            // intensity
        );

        // Rim light
        let rimLight = Light(
            normalize(vec3f(-0.2, 0.5, 0.8)),    // direction
            vec3f(1.0, 1.0, 1.0),                // color
            0.3 * 0.7                            // intensity
        );

        // Calculate lighting contributions
        let key_contribution = calculate_blinn_phong(normal, keyLight, material, view_dir, in.world_pos);
        let fill_contribution = calculate_blinn_phong(normal, fillLight, material, view_dir, in.world_pos);
        let rim_contribution = calculate_blinn_phong(normal, rimLight, material, view_dir, in.world_pos);

        // Edge enhancement using fresnel
        let fresnel = pow(1.0 - abs(dot(normal, view_dir)), 3.0) * 0.2;

        // Combine all lighting
        let mesh_color = vec3f(0.5, 0.5, 0.5);
        let wireframe_color = vec3f(0.0, 0.0, 0.0);

        let total_lighting = key_contribution + fill_contribution + rim_contribution;
        let frag_color = mesh_color * total_lighting + fresnel;

        let d = fwidth(in.bary);
        let factor = smoothstep(vec3(0.0), d*1.5, in.bary);
        let nearest = min(min(factor.x, factor.y), factor.z);
        let color = mix(wireframe_color, frag_color, nearest);

        // Tone mapping and gamma correction
        let mapped_color = aces_tone_mapping(color);
        let corrected_color = pow(mapped_color, vec3f(1.0/2.2));

        return vec4f(corrected_color, 1.0);
}
)shader";