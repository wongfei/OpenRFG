// Original: volition/rfg/code/code_effects/bullet_sparks.h
// Implementation: volition/rfg/code/code_effects/bullet_sparks.cpp
//
// Bullet sparks: the system init called by effects_system_init and the spark effect of a bullet impact.
#pragma once

#include "volition/rfg/code/destruction/rfg_materials.h"

class object;
class vector;
struct weapon_info;

void bullet_spark_create(const vector& pos, const vector& direction, const weapon_info* w_info, unsigned char material_type, material_shape_type shape_type, const object* hit_obj, unsigned int hit_shape_key, float speed); // 0x006E6C90 (:43)
void bullet_spark_system_init(); // 0x006E17E0 (:134)
