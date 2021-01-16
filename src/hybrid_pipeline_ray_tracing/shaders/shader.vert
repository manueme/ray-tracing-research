#version 460

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(binding = 0) uniform SceneProperties
{
    mat4 projection;
    mat4 model;
    mat4 view;
    mat4 viewInverse;
}
scene;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec3 outBitangent;
layout(location = 4) out vec3 outEyePos;

void main()
{
    // Normal in world space
    const mat3 worldNormal = transpose(inverse(mat3(scene.model)));
    const mat3 viewNormal = mat3(scene.viewInverse);

    const vec3 hitNormal = inNormal;
    outNormal = normalize(viewNormal * hitNormal);
    const vec3 hitTangent = normalize(inTangent - dot(inTangent, hitNormal) * hitNormal);
    outTangent = normalize(worldNormal * hitTangent);
    const vec3 hitBitangent = normalize(cross(hitNormal, hitTangent));
    outBitangent = hitBitangent;

    outUV = inTexCoord;

    mat4 modelView = scene.view * scene.model;
    vec4 pos = modelView * vec4(inPos, 1.0);
    gl_Position = scene.projection * pos;
    outEyePos = vec3(modelView * pos);
}
