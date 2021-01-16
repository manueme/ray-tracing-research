/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "mesh.h"

Mesh::Mesh() = default;

Mesh::Mesh(uint32_t t_idx, uint32_t t_indexOffset, uint32_t t_indexBase, uint32_t t_indexCount,
    uint32_t t_vertexOffset, uint32_t t_vertexBase, uint32_t t_vertexCount, uint32_t t_materialIdx)
    : m_idx(t_idx)
    , m_indexOffset(t_indexOffset)
    , m_indexBase(t_indexBase)
    , m_indexCount(t_indexCount)
    , m_vertexOffset(t_vertexOffset)
    , m_vertexBase(t_vertexBase)
    , m_vertexCount(t_vertexCount)
    , m_materialIdx(t_materialIdx)
{
}

uint32_t Mesh::getIdx() const { return m_idx; }

uint32_t Mesh::getIndexBase() const { return m_indexBase; }

uint32_t Mesh::getIndexCount() const { return m_indexCount; }

uint32_t Mesh::getVertexOffset() const { return m_vertexOffset; }

uint32_t Mesh::getVertexBase() const { return m_vertexBase; }

uint32_t Mesh::getVertexCount() const { return m_vertexCount; }

uint32_t Mesh::getMaterialIdx() const { return m_materialIdx; }

uint32_t Mesh::getIndexOffset() const { return m_indexOffset; }
