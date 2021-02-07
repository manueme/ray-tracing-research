#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

layout(binding = 0, set = LIGHTS_SET) buffer _Lights { LightProperties l[]; }
lighting;

#endif // LIGHTS_GLSL