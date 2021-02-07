#ifndef MATERIALS_GLSL
#define MATERIALS_GLSL

layout(binding = 0, set = MATERIALS_AND_TEXTURES_SET) uniform sampler2D textures[];
layout(binding = 1, set = MATERIALS_AND_TEXTURES_SET) buffer _Materials { MaterialProperties m[]; }
materials;

vec4 get_surface_emissive(MaterialProperties material, vec2 hitUV)
{
    if (material.emissiveMapIndex >= 0) {
        return texture(textures[nonuniformEXT(material.emissiveMapIndex)], hitUV);
    }
    return material.emissive;
}

vec4 get_surface_albedo(MaterialProperties material, vec2 hitUV)
{
    if (material.diffuseMapIndex >= 0) {
        return texture(textures[nonuniformEXT(material.diffuseMapIndex)], hitUV);
    }
    return vec4(material.diffuse.rgb, material.opacity);
}

vec3 get_surface_normal(
    const MaterialProperties material, const vec3 hitNormal, const mat3 TBN, const vec2 hitUV)
{
    if (material.normalMapIndex >= 0) {
        vec3 shadingNormal = texture(textures[nonuniformEXT(material.normalMapIndex)], hitUV).rgb;
        shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
        return TBN * shadingNormal;
    }
    return hitNormal;
}

#endif // MATERIALS_GLSL
