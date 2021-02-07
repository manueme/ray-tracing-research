/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef HYBRID_PIPELINE_CONSTANTS
#define HYBRID_PIPELINE_CONSTANTS

#include "../framework/shaders/shared_constants.h"

#define AS_FLAG_EVERYTHING 0xFF

#define RT_PAYLOAD_SHADOW_LOCATION 0
#define RT_PAYLOAD_LOCATION 1

// MonteCarlo app shader table groups
// Main Ray Generation Group
#define SBT_RAY_GEN_GROUP 0
#define SBT_RAY_GEN_INDEX 0
// Ray Miss Group
#define SBT_MISS_GROUP 1
#define SBT_MISS_INDEX 1
// Shadow Ray Miss Group
#define SBT_SHADOW_MISS_GROUP 2
#define SBT_SHADOW_MISS_INDEX 2
// Ray Hit Group
#define SBT_HIT_GROUP 3
#define SBT_ANY_HIT_INDEX 3
#define SBT_CLOSEST_HIT_INDEX 4
// Shadow Ray Hit Group
#define SBT_SHADOW_HIT_GROUP 4
#define SBT_SHADOW_ANY_HIT_INDEX 5
// Group count
#define SBT_NUM_SHADER_GROUPS 5
// ----

// Set locations, bindings are the same for all sets:
#define ACC_STRUCTURE_SET 0
#define SCENE_SET 1
#define VERTEX_SET 2
#define MATERIALS_AND_TEXTURES_SET 3
#define LIGHTS_SET 4
// ---

#endif // HYBRID_PIPELINE_CONSTANTS
