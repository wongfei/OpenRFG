// Original: volition/render_lib/code/rl_3d_entity.h
// Implementation: volition/render_lib/code/rl_3d_entity.cpp
//
// rl_3d_entity, an rl_base_object with a world matrix.
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/vlib/code/math/matrix.h"

class vector;

// size 0x38
class rl_3d_entity : public rl_base_object
{
protected:
    rl_3d_entity(); // 0x004DD0D0 (.h:60)
    virtual ~rl_3d_entity(); // vtbl+0x00 0x004B8640 (.h:64)
public:
    void set_position(const vector& position); // 0x004B8650 (.h:69)
    void set_rotation(const matrix& rotation); // 0x004B8670 (.h:74)
    const vector& get_position() const; // 0x004B86B0 (.h:79)
    const matrix& get_rotation() const; // 0x004B86C0 (.h:84)
    const matrix43& get_world_transform() const; // 0x004B86D0 (.h:90)
    rl_3d_entity& operator=(const rl_3d_entity& __that); // 0x004F5DC0

private:
    matrix43 m_world_matrix; // 0x08
};

// ---- inline members (no .cpp home)

// 0x004DD0D0 (.h:60)
inline rl_3d_entity::rl_3d_entity()
    : m_world_matrix(IDENTITY_MATRIX43)
{
}

// 0x004B8640 (.h:64)
inline rl_3d_entity::~rl_3d_entity()
{
}

// 0x004B8650 (.h:69)
inline void rl_3d_entity::set_position(const vector& position)
{
    m_world_matrix.m_translation = position;
}

// 0x004B8670 (.h:74)
inline void rl_3d_entity::set_rotation(const matrix& rotation)
{
    m_world_matrix.m_rotation = rotation;
}

// 0x004B86B0 (.h:79)
inline const vector& rl_3d_entity::get_position() const
{
    return m_world_matrix.m_translation;
}

// 0x004B86C0 (.h:84)
inline const matrix& rl_3d_entity::get_rotation() const
{
    return m_world_matrix.m_rotation;
}

// 0x004B86D0 (.h:90)
inline const matrix43& rl_3d_entity::get_world_transform() const
{
    return m_world_matrix;
}

// 0x004F5DC0
// Compiler-generated: memberwise.
inline rl_3d_entity& rl_3d_entity::operator=(const rl_3d_entity& __that) = default;
