/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 450

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform _SceneProperties
{
    mat4 viewInverse;
    mat4 projInverse;
    uint frameIteration;
    uint frame;
    uint frameChanged;
}
scene;
layout(set = 1, binding = 0) uniform sampler2D rtInputColor;

in vec4 gl_FragCoord;

void main()
{
    vec2 res = textureSize(rtInputColor, 0);
    vec2 q = gl_FragCoord.xy / res;

    // Vignette fffect
    vec2 v = -1.0 + 2.0 * q;
    v.x *= res.x / res.y;
    float vignette = smoothstep(4.0, 0.6, length(v));
    // ###

    // Chromatic aberration effect
    vec2 centerToUv = q - vec2(0.5);
    vec3 aberr;
    aberr.x = texelFetch(rtInputColor, ivec2((0.5 + centerToUv * 0.995) * res), 0).x;
    aberr.y = texelFetch(rtInputColor, ivec2((0.5 + centerToUv * 0.997) * res), 0).y;
    aberr.z = texelFetch(rtInputColor, ivec2((0.5 + centerToUv) * res), 0).z;
    fragColor = vec4(pow(vignette * aberr, vec3(0.2 + 1.0 / 2.2)), 1.0);
    // ###
}
