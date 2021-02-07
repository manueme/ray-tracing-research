#ifndef SCENE_GLSL
#define SCENE_GLSL
layout(binding = 0, set = SCENE_SET) uniform _SceneProperties
{
    mat4 viewInverse;
    mat4 projInverse;
    vec4 overrideSunDirection;
    int frameIteration;
    int frame;
    int frameChanged;
}
scene;
#endif // SCENE_GLSL