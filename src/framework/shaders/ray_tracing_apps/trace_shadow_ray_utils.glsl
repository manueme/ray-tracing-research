#ifndef BRDF_GLSL
#define BRDF_GLSL

#include "../utils.glsl"
#include "materials.glsl"
#include "trace_shadow_ray.glsl"
#include "vertex.glsl"

vec3 sample_area_light(in LightProperties light, in vec3 hitPoint, float minHitDistance,
    out vec3 lightDir, float n1, float n2, float n3)
{
    vec3 lightIntensity = vec3(0.0f);
    const MaterialProperties areaMaterial = materials.m[light.areaMaterialIdx];
    const uint lightRandomPrimitiveID = uint(floor(light.areaPrimitiveCount * n1));
    const Surface areaSurface = get_surface_instance(light.areaInstanceId,
        lightRandomPrimitiveID,
        uniform_sample_triangle(n2, n3));
    vec3 lightPosition = get_surface_pos(areaSurface);
    vec3 lightNormal = normalize(get_surface_normal(areaSurface));
    lightDir = hitPoint - lightPosition;
    float distSqr = dot(lightDir, lightDir);
    float dist = sqrt(distSqr);
    lightDir = normalize(lightDir);
    // Estimate full area of the light by multiplying by the number of triangles, this will
    // probably cause some very bright pixels, the clamping at the end of the raygen loop
    // will solve that issue:
    float area = get_surface_area(areaSurface) * light.areaPrimitiveCount;
    // ---
    float cosThetaAreaLight = abs(dot(lightNormal, lightDir));
    if (area == 0.0f || cosThetaAreaLight == 0.0f) {
        return vec3(0.0f);
    }
    float lightPDF = uniform_triangle_pdf(distSqr, cosThetaAreaLight, area);
    if (areaMaterial.emissiveMapIndex >= 0) {
        vec2 lightUV = get_surface_uv(areaSurface);
        lightIntensity
            = texture(textures[nonuniformEXT(areaMaterial.emissiveMapIndex)], lightUV).rgb;
    } else {
        lightIntensity = areaMaterial.emissive.rgb;
    }
    lightIntensity /= lightPDF;
    float maxHitDistance = max(0.0f, dist - 0.001f);
    const float visibility
        = 1.0 - trace_shadow_ray(hitPoint, -lightDir, minHitDistance, maxHitDistance);
    return visibility * lightIntensity;
}

vec3 sample_sun_light(in LightProperties light, in vec3 addDirection, in vec3 hitPoint,
    float minHitDistance, out vec3 lightDir)
{
    lightDir = normalize(light.direction.xyz + addDirection);
    const float visibility
        = 1.0 - trace_shadow_ray(hitPoint, -lightDir, minHitDistance, RAY_MAX_HIT);
    return light.diffuse.rgb * visibility;
}

#endif // BRDF_GLSL