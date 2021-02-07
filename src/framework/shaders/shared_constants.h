/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef COMMON_CONSTANTS_H
#define COMMON_CONSTANTS_H

#define M_PIf 3.14159265358979323846f
#define M_INV_PIf 0.31830988618379067154f
#define M_INV_2PIf 0.15915494309189533577f
#define M_PI_OVER4f 0.78539816339744830961f
#define M_PI_OVER2f 1.57079632679489661923f

#define NOT_REFRACTIVE_IDX 0.0f
#define NOT_REFLECTVE_IDX 0.0f

#define RAY_TYPE_UNDEFINED 0
#define RAY_TYPE_DIFFUSE 1
#define RAY_TYPE_REFRACTION 2
#define RAY_TYPE_REFLECTION 3
#define RAY_TYPE_MISS 4

#define CAMERA_NEAR 0.2f
#define CAMERA_FAR 800.0f

#define RAY_MIN_HIT 0.01f
#define RAY_MAX_HIT 800.0f

#define AS_FLAG_EVERYTHING 0xFF

#endif // COMMON_CONSTANTS_H
