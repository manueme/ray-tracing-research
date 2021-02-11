/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 450
#extension GL_GOOGLE_include_directive : enable

#define SCENE_SET 0 // override SCENE_SET for this particular case
#include "app_scene.glsl"

layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 0) uniform sampler2D rtInputColor;

in vec4 gl_FragCoord;

float GAMMA = 2.2;

vec3 linear_tone_mapping(vec3 color, float exposure)
{
    color = clamp(exposure * color, 0., 1.);
    color = pow(color, vec3(1. / GAMMA));
    return color;
}

void main()
{
    vec2 res = textureSize(rtInputColor, 0);
    vec2 q = gl_FragCoord.xy / res;

    // Vignette fffect
    vec2 v = -1.0 + 2.0 * q;
    v.x *= res.x / res.y;
    float vignette = smoothstep(4.0, 0.6, length(v));
    // ###

    // Tone mapping
    fragColor = vec4(vignette
            * linear_tone_mapping(texelFetch(rtInputColor, ivec2(gl_FragCoord.xy), 0).xyz,
                scene.exposure),
        1.0);
    // ###
}
