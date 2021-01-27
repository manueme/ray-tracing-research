/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef PATH_TRACER_CONSTANTS

#define PATH_TRACER_CONSTANTS 1

#define M_PIf 3.14159265358979323846f
#define M_INV_PIf 0.31830988618379067154f
#define M_INV_2PIf 0.15915494309189533577f
#define M_PI_OVER4f 0.78539816339744830961f
#define M_PI_OVER2f 1.57079632679489661923f

#define CAMERA_NEAR 0.2f
#define CAMERA_FAR 800.0f

#define RAY_MIN_HIT 0.01f
#define RAY_MAX_HIT 800.0f

#define AS_FLAG_EVERYTHING 0xFF

#define RT_PAYLOAD_SHADOW 0
#define RT_PAYLOAD_BRDF 1

// MonteCarlo app shader table groups
// Main Ray Generation Group
#define SBT_MC_RAY_GEN_GROUP 0
#define SBT_MC_RAY_GEN_INDEX 0
// Ray Miss Group
#define SBT_MC_MISS_GROUP 1
#define SBT_MC_MISS_INDEX 1
// Shadow Ray Miss Group
#define SBT_MC_SHADOW_MISS_GROUP 2
#define SBT_MC_SHADOW_MISS_INDEX 2
// Ray Hit Group
#define SBT_MC_HIT_GROUP 3
#define SBT_MC_ANY_HIT_INDEX 3
#define SBT_MC_CLOSEST_HIT_INDEX 4
// Shadow Ray Hit Group
#define SBT_MC_SHADOW_HIT_GROUP 4
#define SBT_MC_SHADOW_ANY_HIT_INDEX 5
// Group count
#define SBT_MC_NUM_SHADER_GROUPS 5
// ----

// Hybrid Pipeline app shader table groups
// Main Ray Generation Group
#define SBT_HP_RAY_GEN_GROUP 0
#define SBT_HP_RAY_GEN_INDEX 0
// Ray Miss Group
#define SBT_HP_MISS_GROUP 1
#define SBT_HP_MISS_INDEX 1
// Shadow Ray Miss Group
#define SBT_HP_SHADOW_MISS_GROUP 2
#define SBT_HP_SHADOW_MISS_INDEX 2
// Ray Hit Group
#define SBT_HP_HIT_GROUP 3
#define SBT_HP_ANY_HIT_INDEX 3
#define SBT_HP_CLOSEST_HIT_INDEX 4
// Shadow Ray Hit Group
#define SBT_HP_SHADOW_HIT_GROUP 4
#define SBT_HP_SHADOW_ANY_HIT_INDEX 5
// Group count
#define SBT_HP_NUM_SHADER_GROUPS 5
// ----

#define SRGB_ALPHA 0.055

#define NOT_REFRACTIVE_IDX -1.0f
#define NOT_REFLECTVE_IDX -1.0f

#define RAY_TYPE_UNDEFINED 0
#define RAY_TYPE_DIFFUSE 1
#define RAY_TYPE_REFRACTION 2
#define RAY_TYPE_REFLECTION 3
#define RAY_TYPE_MISS 4
#endif
