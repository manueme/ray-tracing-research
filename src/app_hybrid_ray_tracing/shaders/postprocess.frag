/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 450

layout(location = 0) out vec4 fragColor;
layout(set = 0, binding = 0) uniform sampler2D rtInputColor;

// in vec4 gl_FragCoord;
layout(location = 0) in vec2 inUV;

#define FXAA_REDUCE_MIN (1.0 / 128.0)
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_SPAN_MAX 8.0

vec4 fxaa_process(vec2 resolution, vec2 uv)
{
    vec3 m = texture(rtInputColor, uv).rgb;
    vec3 nw = textureOffset(rtInputColor, uv, ivec2(-1.0, -1.0)).rgb;
    vec3 ne = textureOffset(rtInputColor, uv, ivec2(1.0, -1.0)).rgb;
    vec3 sw = textureOffset(rtInputColor, uv, ivec2(-1.0, 1.0)).rgb;
    vec3 se = textureOffset(rtInputColor, uv, ivec2(1.0, 1.0)).rgb;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaM = dot(m, luma);
    float lumaNW = dot(nw, luma);
    float lumaNE = dot(ne, luma);
    float lumaSW = dot(sw, luma);
    float lumaSE = dot(se, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    vec2 dir
        = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)), ((lumaNW + lumaSW) - (lumaNE + lumaSE)));

    float dirReduce
        = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX), dir * rcpDirMin)) / resolution;

    vec3 rgbA = 0.5
        * (texture(rtInputColor, uv + dir * (1.0 / 3.0 - 0.5)).xyz
            + texture(rtInputColor, uv + dir * (2.0 / 3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * 0.5
        + 0.25
            * (texture(rtInputColor, uv + dir * -0.5).xyz
                + texture(rtInputColor, uv + dir * 0.5).xyz);
    float lumaB = dot(rgbB, luma);
    if (lumaB < lumaMin || lumaB > lumaMax) {
        return vec4(rgbA, 1.0);
    } else {
        return vec4(rgbB, 1.0);
    }
}

void main()
{
    vec2 resolution = textureSize(rtInputColor, 0);

    // Vignette fffect
    vec2 v = -1.0 + 2.0 * inUV;
    v.x *= resolution.x / resolution.y;
    float vignette = smoothstep(4.0, 0.6, length(v));
    // ###

    // Chromatic aberration effect
    vec2 centerToUv = inUV - vec2(0.5);
    vec3 aberr;
    aberr.x = fxaa_process(resolution, vec2((0.5 + centerToUv * 0.995))).x;
    aberr.y = fxaa_process(resolution, vec2((0.5 + centerToUv * 0.997))).y;
    aberr.z = fxaa_process(resolution, vec2((0.5 + centerToUv))).z;
    fragColor = vec4(pow(vignette * aberr, vec3(0.2 + 1.0 / 2.2)), 1.0);
    // ###
}
