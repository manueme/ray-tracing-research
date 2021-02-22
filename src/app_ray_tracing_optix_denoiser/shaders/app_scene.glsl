#ifndef SCENE_GLSL
#define SCENE_GLSL
layout(binding = 0, set = SCENE_SET) uniform _SceneProperties
{
    mat4 view;
    mat4 projection;
    mat4 viewInverse;
    mat4 projInverse;
    vec4 overrideSunDirection;
    uint frameIteration;
    uint frame;
    uint frameChanged;
    float manualExposureAdjust;
}
scene;
#endif // SCENE_GLSL