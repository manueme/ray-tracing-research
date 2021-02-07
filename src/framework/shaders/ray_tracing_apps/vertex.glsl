/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef VERTEX_GLSL
#define VERTEX_GLSL

layout(binding = 0, set = VERTEX_SET) buffer Vertices { vec4 v[]; }
vertices;
layout(binding = 1, set = VERTEX_SET) buffer _Indices { uint i[]; }
indices;
layout(binding = 2, set = VERTEX_SET) readonly buffer _Instances { ShaderMeshInstance i[]; }
instanceInfo;

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec3 tangent;
    vec2 uv;
    float padding0;
};

Vertex unpack(const uint index)
{
    vec4 d0 = vertices.v[3 * index];
    vec4 d1 = vertices.v[3 * index + 1];
    vec4 d2 = vertices.v[3 * index + 2];

    Vertex v;
    v.pos = d0.xyz;
    v.normal = vec3(d0.w, d1.x, d1.y);
    v.tangent = vec3(d1.z, d1.w, d2.x);
    v.uv = vec2(d2.y, d2.z);
    return v;
}

struct Surface {
    vec3 barycentricCoords;
    Vertex v0;
    Vertex v1;
    Vertex v2;
};

Surface get_surface_instance(uint instanceId, uint primitiveId, vec2 sampleCoords)
{
    Surface p;
    uint indexBase = instanceInfo.i[instanceId].indexBase + (3 * primitiveId);
    uint vertexBase = instanceInfo.i[instanceId].vertexBase;
    const ivec3 index
        = ivec3(indices.i[indexBase], indices.i[indexBase + 1], indices.i[indexBase + 2]);
    p.v0 = unpack(vertexBase + index.x);
    p.v1 = unpack(vertexBase + index.y);
    p.v2 = unpack(vertexBase + index.z);
    p.barycentricCoords
        = vec3(1.0f - sampleCoords.x - sampleCoords.y, sampleCoords.x, sampleCoords.y);
    return p;
}

vec3 get_surface_normal(const Surface s)
{
#ifdef FLAT_SHADING
    vec3 normal = cross(s.v1.pos - s.v0.pos, s.v2.pos - s.v0.pos);
#else
    vec3 normal = (s.v0.normal * s.barycentricCoords.x + s.v1.normal * s.barycentricCoords.y
        + s.v2.normal * s.barycentricCoords.z);
#endif
    return normalize(normal);
}

vec3 get_surface_tangent(const Surface s)
{
    return normalize(s.v0.tangent * s.barycentricCoords.x + s.v1.tangent * s.barycentricCoords.y
        + s.v2.tangent * s.barycentricCoords.z);
}

void get_surface_tangent_space(
    const Surface s, out vec3 normal, out vec3 tangent, out vec3 bitangent)
{
    normal = get_surface_normal(s);
    tangent = get_surface_tangent(s);
    // re-orthogonalize T with respect to N
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    bitangent = normalize(cross(normal, tangent));
}

vec3 get_surface_pos(const Surface s)
{
    return s.v0.pos * s.barycentricCoords.x + s.v1.pos * s.barycentricCoords.y
        + s.v2.pos * s.barycentricCoords.z;
}

vec2 get_surface_uv(const Surface s)
{
    return s.v0.uv * s.barycentricCoords.x + s.v1.uv * s.barycentricCoords.y
        + s.v2.uv * s.barycentricCoords.z;
}

float get_surface_area(const Surface s)
{
    return 0.5f * length(cross(s.v1.pos.xyz - s.v0.pos.xyz, s.v2.pos.xyz - s.v0.pos.xyz));
}

#endif // VERTEX_GLSL
