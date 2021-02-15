#ifndef EXPOSURE_FUNCTIONS_GLSL
#define EXPOSURE_FUNCTIONS_GLSL

float GAMMA = 2.2;

vec3 linear_tone_mapping(vec3 color, float exposure)
{
  color = clamp(exposure * color, 0., 1.);
  color = pow(color, vec3(1. / GAMMA));
  return color;
}

#endif // EXPOSURE_FUNCTIONS_GLSL
