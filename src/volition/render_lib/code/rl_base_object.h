// Original: volition/render_lib/code/rl_base_object.h
//
// rl_base_object, the root of render_lib objects: a leaf class id used by the render_lib RTTI (rl_factory_rtti.h).
#pragma once

// size 0x8
class rl_base_object
{
protected:
    rl_base_object(); // 0x004B68B0 (.h:58)
    virtual ~rl_base_object(); // vtbl+0x00 0x004B68C0 (.h:62)
public:
    virtual void release() = 0; // vtbl+0x04
    unsigned int get_leaf_class_id() const; // 0x004B68D0 (.h:66)
protected:
    void set_leaf_class_id(unsigned int leaf_class_id); // 0x004B68E0 (.h:71)
public:
    rl_base_object& operator=(const rl_base_object& __that); // 0x004C7650

    // 0x00 vfptr
private:
    unsigned int m_leaf_class_id; // 0x04
};

// ---- inline members

// 0x004B68B0 (.h:58)
inline rl_base_object::rl_base_object()
{
    // m_leaf_class_id is set by the create functions.
}

// 0x004B68C0 (.h:62)
inline rl_base_object::~rl_base_object()
{
}

// 0x004B68D0 (.h:66)
inline unsigned int rl_base_object::get_leaf_class_id() const
{
    return m_leaf_class_id;
}

// 0x004B68E0 (.h:71)
inline void rl_base_object::set_leaf_class_id(unsigned int leaf_class_id)
{
    m_leaf_class_id = leaf_class_id;
}

// 0x004C7650
// Compiler-generated: memberwise.
inline rl_base_object& rl_base_object::operator=(const rl_base_object& __that) = default;
