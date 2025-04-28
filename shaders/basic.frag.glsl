#version 460 core

layout(location = 0)
out vec4 o_color;

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 in_frag_pos;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in mat3 in_TBN;

layout(constant_id = 0) const uint env_map_mip_count = 0;

layout(std140, binding = 1) uniform MaterialUBO {
    vec4 u_base_color_factor;
    vec4 u_metallic_roughness_normal_occlusion;
    vec3 u_camera_pos;
    uint u_material_flags;
    vec3 u_emissive_factor;
};

layout(std140, binding = 2) uniform LightPosUBO {
    vec3 light_positions[5];
};

layout(binding = 0)
uniform sampler2D s_texture;

layout(binding = 1)
uniform sampler2D s_metallic_roughness;

layout(binding = 2)
uniform sampler2D s_normal_map;

layout(binding = 3)
uniform sampler2D s_occlusion_map;

layout(binding = 4)
uniform sampler2D s_emission_map;

layout(binding = 5)
uniform sampler2D s_brdf_lut;

layout(binding = 6)
uniform samplerCube s_env_map_irradiance;

layout(binding = 7)
uniform samplerCube s_env_map;

#include "pbr.glsl"

const uint MATERIAL_FLAG_BASE_COLOR_TEXTURE = 1 << 0;
const uint MATERIAL_FLAG_METALLIC_ROUGNESS_TEXTURE = 1 << 1;
const uint MATERIAL_FLAG_NORMAL_MAP = 1 << 2;
const uint MATERIAL_FLAG_OCCLUSION_MAP = 1 << 3;
const uint MATERIAL_FLAG_EMISSION_MAP = 1 << 4;

vec4 get_base_color() {
    if ((u_material_flags & MATERIAL_FLAG_BASE_COLOR_TEXTURE) != 0) {
        return texture(s_texture, in_uv) * u_base_color_factor;
    }

    return u_base_color_factor;
}

vec4 get_metallic_roughness() {
    if ((u_material_flags & MATERIAL_FLAG_METALLIC_ROUGNESS_TEXTURE) != 0) {
        return texture(s_metallic_roughness, in_uv);
    }

    return vec4(1.0);
}

vec3 get_normal() {
    vec3 normal = normalize(in_normal);
    if ((u_material_flags & MATERIAL_FLAG_NORMAL_MAP) != 0) {
        vec2 normal_sample = texture(s_normal_map, in_uv).rg;
        o_color = vec4(normal_sample, 0.0, 1.0);

        normal_sample = normal_sample * 2.0 - 1.0;
        normal_sample = normal_sample * u_metallic_roughness_normal_occlusion.z;
        float normal_z = sqrt(1.0 - normal_sample.x * normal_sample.x - normal.y * normal_sample.y);
        vec3 reconstructed = vec3(normal_sample, normal_z);
        normal = (in_TBN * reconstructed);
    }

    return normal;
}

float get_ao() {
    if ((u_material_flags & MATERIAL_FLAG_OCCLUSION_MAP) != 0) {
        return texture(s_occlusion_map, in_uv).r * u_metallic_roughness_normal_occlusion.w;
    }

    return u_metallic_roughness_normal_occlusion.w;
}

vec3 get_emission() {
    if ((u_material_flags & MATERIAL_FLAG_EMISSION_MAP) != 0) {
        return texture(s_emission_map, in_uv).rgb * u_emissive_factor;
    }

    return u_emissive_factor;
}

vec3 light_pos_fixed[4] = {
    vec3(-10.0,  10.0, 10.0),
    vec3( 10.0,  10.0, 10.0),
    vec3(-10.0, -10.0, 10.0),
    vec3( 10.0, -10.0, 10.0)
};

void main() {
    vec3 normal = get_normal();
    vec4 mr_sample = get_metallic_roughness();
    vec4 albedo = get_base_color();
    float ao = get_ao();

    PBRInfo pbrInputs = calculatePBRInputsMetallicRoughness(albedo, normal, u_camera_pos, in_frag_pos, mr_sample);

    vec3 specular_color = getIBLRadianceContributionGGX(pbrInputs, 1.0);
    vec3 diffuse_color = getIBLRadianceLambertian(pbrInputs.NdotV, normal, pbrInputs.perceptualRoughness, pbrInputs.diffuseColor, pbrInputs.reflectance0, 1.0);
    vec3 color = specular_color + diffuse_color;
    //vec3 color = vec3(0);

    vec3 lightPos = vec3(0, 0, 5);
    vec3 light_dir = normalize(lightPos - in_frag_pos);
    color += calculatePBRLightContribution(pbrInputs, light_dir, vec3(1.0));

    color = color * ao;
    color += get_emission();

    color = color / (color + vec3(1.0));
    o_color = vec4(color, 1.0);
}
