/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_DENOISER_H
#define MANUEME_DENOISER_H

#include "../monte_carlo_ray_tracing/monte_carlo_ray_tracing.h"

class DenoiserApp : public MonteCarloRTApp {
public:
    DenoiserApp();
    ~DenoiserApp();
};

#endif // MANUEME_DENOISER_H
