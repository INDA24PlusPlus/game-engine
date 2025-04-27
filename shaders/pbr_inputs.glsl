float get_roughness_factor() {
    return u_metallic_roughness_normal_occlusion.y;
}

float get_metallic_factor() {
    return u_metallic_roughness_normal_occlusion.x;
}

vec4 sample_brdf_lut(vec2 uv) {
    return texture(s_brdf_lut, uv);
}

vec4 sample_env_map_irradiance(vec3 tc) {
    return texture(s_env_map_irradiance, tc);
}

vec4 sample_env_map_lod(vec3 tc, float lod) {
    return textureLod(s_env_map, tc, lod);
}