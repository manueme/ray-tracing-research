/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_INSTANCE_H
#define MANUEME_INSTANCE_H

#include "shader_instance.h"
#include <cstdint>

class Instance {
public:
    Instance();
    Instance(uint32_t t_blasIdx, uint32_t t_meshIdx);

    uint32_t getMeshIdx() const;
    uint32_t getBlasIdx() const;

private:
    uint32_t m_blasIdx;
    uint32_t m_meshIdx;
};

#endif // MANUEME_INSTANCE_H
