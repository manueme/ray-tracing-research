#ifndef SCENE_GLSL
#define SCENE_GLSL
layout(binding = 0, set = SCENE_SET) uniform _SceneProperties
{
    mat4 prevView;
    mat4 currentView;
    mat4 prevProjection;
    mat4 currentProjection;
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