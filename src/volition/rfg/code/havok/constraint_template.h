// Original: volition/rfg/code/havok/constraint_template.h
// Implementation: volition/rfg/code/havok/constraint_template.cpp
//
// Constraint templates: the serialized description of a Havok constraint between two bodies of an object (placed
// constraints, ragdolls, vehicle parts). type selects the member of the per-type data union (point, hinge, ragdoll,
// prismatic, motor; constraint_type); body_index packs the object and parent indices of a body. Placement of
// constraint_type and the *_constraint_data structs inferred.
#pragma once

#include "volition/vlib/code/math/matrix.h"

enum constraint_type
{
    HAVOK_CONSTRAINT_NONE = -1,
    HAVOK_CONSTRAINT_POINT = 0,
    HAVOK_CONSTRAINT_HINGE = 1,
    HAVOK_CONSTRAINT_PRISMATIC = 2,
    HAVOK_CONSTRAINT_RAGDOLL = 3,
    HAVOK_CONSTRAINT_MOTOR = 4,
    HAVOK_CONSTRAINT_FAKE = 5,
    NUM_HAVOK_CONSTRAINT_TYPES = 6,
};

// size 0x4
struct body_index
{
    void set(int obj_index, int parent_index); // 0x0080D6C0 (:12)
    int get_obj_index() const; // 0x0080D6D0 (:20)
    int get_parent_index() const; // 0x0080D6E0 (:27)

private:
    int index; // 0x00
};

// size 0x20
struct point_constraint_data
{
    int ctype; // 0x00
    float x_limit_min; // 0x04
    float x_limit_max; // 0x08
    float y_limit_min; // 0x0C
    float y_limit_max; // 0x10
    float z_limit_min; // 0x14
    float z_limit_max; // 0x18
    float stiff_spring_length; // 0x1C
};

// size 0x10
struct hinge_constraint_data
{
    int limited; // 0x00
    float limit_min_angle; // 0x04
    float limit_max_angle; // 0x08
    float limit_friction; // 0x0C
};

// size 0x18
struct ragdoll_constraint_data
{
    float twist_min; // 0x00
    float twist_max; // 0x04
    float cone_min; // 0x08
    float cone_max; // 0x0C
    float plane_min; // 0x10
    float plane_max; // 0x14
};

// size 0x10
struct prismatic_constraint_data
{
    int limited; // 0x00
    float limit_min; // 0x04
    float limit_max; // 0x08
    float limit_friction; // 0x0C
};

// size 0x18
struct motor_constraint_data
{
    float angular_speed; // 0x00
    float gain; // 0x04
    int axis; // 0x08
    float axis_in_body_space_x; // 0x0C
    float axis_in_body_space_y; // 0x10
    float axis_in_body_space_z; // 0x14
};

// size 0x9C
struct constraint_template
{
private:
    void reset(); // 0x00742160 (.h:151)
public:
    constraint_template(constraint_type);
    constraint_template(); // 0x00742240 (.h:170)

    unsigned int type; // 0x00
    unsigned int _name_unused; // 0x04
    unsigned int _body_a_name_unused; // 0x08
    body_index body_a; // 0x0C
    matrix43 body_a_transform; // 0x10
    unsigned int _body_b_name_unused; // 0x40
    body_index body_b; // 0x44
    matrix43 body_b_transform; // 0x48
    float threshold; // 0x78
    union
    {
        point_constraint_data point_data; // 0x7C
        hinge_constraint_data hinge_data; // 0x7C
        ragdoll_constraint_data ragdoll_data; // 0x7C
        prismatic_constraint_data prismatic_data; // 0x7C
        motor_constraint_data motor_data; // 0x7C
    };
};
