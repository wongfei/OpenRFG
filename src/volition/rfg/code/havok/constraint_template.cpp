// Original: volition/rfg/code/havok/constraint_template.cpp
//
// The packed body indices of constraint templates and the template's reset (multiplayer.md MP8: projectiles attach
// through placed constraints). Compiled in rfg_havok_unity.cpp.
// The external interface is declared in constraint_template.h.

#include "volition/rfg/code/havok/constraint_template.h"

// 0x0080D6C0 (:12): the object index in the low word, the parent index in the high word.
void body_index::set(int obj_index, int parent_index)
{
    index = parent_index << 16 | obj_index;
}

// 0x0080D6D0 (:20)
int body_index::get_obj_index() const
{
    return (int)(short)index;
}

// 0x0080D6E0 (:27)
int body_index::get_parent_index() const
{
    return (int)*((const short*)&index + 1);
}

// 0x00742160 (constraint_template.h:151): no type, no bodies, identity transforms, unbreakable (threshold -1). The
// per-type data is left as it is.
void constraint_template::reset()
{
    type = 0xFFFFFFFF;
    _name_unused = 0;
    _body_a_name_unused = 0;
    _body_b_name_unused = 0;
    body_a.set(-1, -1);
    body_b.set(-1, -1);
    body_a_transform = IDENTITY_MATRIX43;
    body_b_transform = IDENTITY_MATRIX43;
    threshold = -1.0f;
}

// 0x00742240 (constraint_template.h:170)
constraint_template::constraint_template()
{
    reset();
}
