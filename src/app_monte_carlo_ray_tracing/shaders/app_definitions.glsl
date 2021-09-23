/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

// Include this file FIRST in all main shaders

#ifndef APP_DEFINITIONS_GLSL
#define APP_DEFINITIONS_GLSL

#include "../../framework/shaders/shared_definitions.glsl"
#include "../constants.h"

#define SAMPLE_PRIMARY
#define DEPTH_OF_FIELD
#define STORE_DEPTH_MAP
#define SHOW_NANS
#define CAMERA_APERTURE 0.40f

// #define FLAT_SHADING

#endif // APP_DEFINITIONS_GLSL