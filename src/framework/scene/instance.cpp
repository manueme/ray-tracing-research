/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "instance.h"

Instance::Instance() { }

Instance::Instance(uint32_t t_blasIdx, uint32_t t_meshIdx)
    : m_meshIdx(t_meshIdx)
    , m_blasIdx(t_blasIdx)
{
}

uint32_t Instance::getMeshIdx() const { return m_meshIdx; }

uint32_t Instance::getBlasIdx() const { return m_blasIdx; }
