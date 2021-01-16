#version 450

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform _SceneProperties
{
    mat4 viewInverse;
    mat4 projInverse;
    int frameIteration;
    int frame;
    bool frameChanged;
}
scene;
layout(set = 1, binding = 0) uniform sampler2D rtInputColor;
// layout(set = 1, binding = 1) uniform sampler2D rtInputNormal;
// layout(set = 2, binding = 0, r32f) uniform image2D convLayer1;

layout(push_constant) uniform shaderInformation { float aspectRatio; }
push;

in vec4 gl_FragCoord;

vec4 gaussianFilter()
{
    const vec2 texCoord = gl_FragCoord.xy;
    vec4 sum = vec4(0.0);

    // Gaussian kernel
    // 1 2 1
    // 2 4 2
    // 1 2 1
    const float kernel[9] = float[9](1.0f, 2.0f, 1.0f, 2.0f, 4.0f, 2.0f, 1.0f, 2.0f, 1.0f);

    sum += texelFetch(rtInputColor, ivec2(texCoord.x - 1, texCoord.y - 1), 0) * kernel[0];
    sum += texelFetch(rtInputColor, ivec2(texCoord.x - 1, texCoord.y), 0) * kernel[1];
    sum += texelFetch(rtInputColor, ivec2(texCoord.x - 1, texCoord.y + 1), 0) * kernel[2];

    sum += texelFetch(rtInputColor, ivec2(texCoord.x, texCoord.y - 1), 0) * kernel[3];
    sum += texelFetch(rtInputColor, ivec2(texCoord.x, texCoord.y), 0) * kernel[4];
    sum += texelFetch(rtInputColor, ivec2(texCoord.x, texCoord.y + 1), 0) * kernel[5];

    sum += texelFetch(rtInputColor, ivec2(texCoord.x + 1, texCoord.y - 1), 0) * kernel[6];
    sum += texelFetch(rtInputColor, ivec2(texCoord.x + 1, texCoord.y), 0) * kernel[7];
    sum += texelFetch(rtInputColor, ivec2(texCoord.x + 1, texCoord.y + 1), 0) * kernel[8];

    sum /= 16.0;
    sum.a = 1.0;

    return sum;
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

    // Chromatic aberration effect
    vec2 centerToUv = q - vec2(0.5);
    vec3 aberr;
    aberr.x = texelFetch(rtInputColor, ivec2((0.5 + centerToUv * 0.995) * res), 0).x;
    aberr.y = texelFetch(rtInputColor, ivec2((0.5 + centerToUv * 0.997) * res), 0).y;
    aberr.z = texelFetch(rtInputColor, ivec2((0.5 + centerToUv) * res), 0).z;
    fragColor = vec4(pow(vignette * aberr, vec3(0.2 + 1.0 / 2.2)), 1.0);
    // ###
}
