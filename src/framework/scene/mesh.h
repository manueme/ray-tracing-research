/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_MESH_H
#define MANUEME_MESH_H

#include <cstdint>

class Mesh {
public:
    Mesh();
    Mesh(uint32_t t_idx, uint32_t t_indexOffset, uint32_t t_indexBase,
        uint32_t t_indexCount, uint32_t t_vertexOffset, uint32_t t_vertexBase,
        uint32_t t_vertexCount, uint32_t t_materialIdx);

    uint32_t getIdx() const;
    uint32_t getIndexBase() const;
    uint32_t getIndexCount() const;
    uint32_t getVertexOffset() const;
    uint32_t getVertexBase() const;
    uint32_t getVertexCount() const;
    uint32_t getMaterialIdx() const;
    uint32_t getIndexOffset() const;

private:
    uint32_t m_idx;
    uint32_t m_indexOffset;
    uint32_t m_indexBase;
    uint32_t m_indexCount;
    uint32_t m_vertexOffset;
    uint32_t m_vertexBase;
    uint32_t m_vertexCount;
    uint32_t m_materialIdx;
};

#endif // MANUEME_MESH_H
