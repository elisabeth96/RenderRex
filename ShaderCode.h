#pragma once

const char* shaderCode = R"shader(
struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) bary: vec3f,
    @location(3) edge_mask: vec3f,
	@location(4) color: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) bary: vec3f,
    @location(1) edge_mask: vec3f,
    @location(2) world_normal: vec3f,
    @location(3) world_pos: vec3f,
    @location(4) view_pos: vec3f,
	@location(5) color: vec3f,
};

struct MyUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    wireframeColor: vec4f,
    options : vec4f,
};

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;

struct Light {
    position: vec3f,
    color: vec3f,
    intensity: f32,
}

fn calculate_lighting(light: Light, normal: vec3f, view_pos: vec3f, view_dir: vec3f) -> vec3f {
    let light_dir = normalize(light.position - view_pos);

    let diff = max(dot(normal, light_dir), 0.0);
    let diffuse = diff * light.color * 0.8;

    let reflect_dir = reflect(-light_dir, normal);
    let spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);
    let specular = spec * vec3f(0.3) * light.color;

    let distance = length(light.position - view_pos);
    let attenuation = 1.0 / (1.0 + 0.0005 * distance);

    return (diffuse + specular) * light.intensity * attenuation;
}

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let modelPos = uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
    out.world_pos = modelPos.xyz;
    out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * modelPos;
    let world_normal = normalize((uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz);
    out.world_normal = (uMyUniforms.viewMatrix * vec4f(world_normal, 0.0)).xyz;
    out.bary = in.bary;
    out.edge_mask = in.edge_mask;
    out.view_pos = (uMyUniforms.viewMatrix * modelPos).xyz;
	out.color = in.color;
    return out;
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
fn fs_main(@builtin(front_facing) is_front: bool, in: VertexOutput) -> @location(0) vec4f {
    let normal = (f32(is_front) * 2.0 - 1.0) * normalize(in.world_normal);
    let view_dir = normalize(-in.view_pos);

    // Key light (main illumination)
    let key_light = Light(
        vec3f(10.0, 10.0, 10.0),   // position
        vec3f(1.0, 0.98, 0.95),    // warm white
        0.8                        // intensity
    );

    // Fill light
    let fill_light = Light(
        vec3f(-6.0, 4.0, 8.0),       
        vec3f(0.9, 0.9, 1.0),          // cool white
        0.4                      
    );

    // Back light
    let back_light = Light(
        vec3f(-2.0, 6.0, -8.0),       
        vec3f(1.0, 1.0, 1.0),          // white
        0.3                           
    );

    let meshColor = in.color;
    let wireframe_color = uMyUniforms.wireframeColor.xyz;
    var result = vec3f(0.0);

    // Calculate lighting contributions
    result += calculate_lighting(key_light, normal, in.view_pos, view_dir) * meshColor;
    result += calculate_lighting(fill_light, normal, in.view_pos, view_dir) * meshColor;

    let rim_effect = 1.0 - max(dot(view_dir, normal), 0.0);
    result += calculate_lighting(back_light, normal, in.view_pos, view_dir) * rim_effect * meshColor;

    // Add ambient light
    let ambient = vec3f(0.15) * meshColor;
    result += ambient;

    // Tone mapping and gamma correction
    result = aces_tone_mapping(result);
    result = pow(result, vec3f(1.0/2.2));

    // as vec4
	var final_color = result;

    //let show_mesh = uMyUniforms.options.z;
    //if (show_mesh == 0.0) {
    //    final_color = wireframe_color;
    //}

    if (uMyUniforms.options.x == 1.0) {
		// Wire frame calculation (preserved from original)
        let d = fwidth(in.bary);
        let factor = smoothstep(vec3(0.0), d*1.5, in.bary);
        let factor_masked = max(factor, in.edge_mask);
        let nearest = min(min(factor_masked.x, factor_masked.y), factor_masked.z);

        // Mix wireframe with shaded mesh
        final_color = mix(wireframe_color, result, nearest);
	}

    // Tone mapping and gamma correction
    //let mapped_color = aces_tone_mapping(final_color);
    //let corrected_color = pow(mapped_color, vec3f(1.0/2.2));

    return vec4f(final_color, uMyUniforms.options.y);
}
)shader";