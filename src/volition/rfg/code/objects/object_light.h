// Original: volition/rfg/code/objects/object_light.h
// Implementation: volition/rfg/code/objects/object_light.cpp
//
// obj_light, a light placed in a zone (OT_LIGHT): its rl_light in the main scene, type, color, attenuation and the
// time-of-day/flicker behaviour.
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"

class rl_compound_mesh_instance;
class rl_light;
class rl_renderer;

enum obj_light_type
{
    LIGHT_TYPE_OMNI = 0,
    LIGHT_TYPE_SPOT_RECT = 1,
    LIGHT_TYPE_SPOT_CIRCLE = 2,
    LIGHT_TYPE_DIRECTIONAL = 3,
    LIGHT_TYPE_AMBIENT = 4,
};

// size 0xE4
class obj_light : public object
{
public:
    obj_light(); // 0x00A19A10 (:59)
    static const object_type get_type_static(); // 0x00A19930 (.h:40)
    static const object_sub_type get_subtype_static(); // 0x00A19940 (.h:40)
    static const char* get_classname_static(); // 0x00A19950 (.h:40)
    static int get_bpo_index_static(); // 0x00A19960 (.h:40)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A19A30 (.h:40)
    static unsigned int get_classname_hash(); // 0x00A19A00 (:58)
    static int pool_remaining(); // 0x00A3CBF0 (:58)
    virtual void pool_free(); // vtbl+0x80 0x00A917D0 (:58)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA9030 (:71)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A66C00 (:223)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A917E0 (:297)
public:
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A19A50 (:304)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A19B20 (:312)
    virtual void update_render(rl_renderer*); // vtbl+0x44 0x00A19B60 (:318)
    virtual ~obj_light(); // vtbl+0x00 0x00A19A40

    obj_light_type light_type; // 0x9C
    int flags; // 0xA0
    vector fvec; // 0xA4
    vector clr_orig; // 0xB0
    float hotspot_size; // 0xBC
    float hotspot_falloff_size; // 0xC0
    float atten_start; // 0xC4
    float atten_end; // 0xC8
    vector clr; // 0xCC
    rl_light* m_light_p; // 0xD8
    rl_compound_mesh_instance* m_clip_inst_p; // 0xDC
    unsigned int m_clip_mesh_name_crc; // 0xE0
};

void types_obj_light(signed char& type, signed char& subtype); // 0x00A199D0 (:58)
int bpo_index_obj_light(); // 0x00A199E0 (:58)
const char* classname_obj_light(); // 0x00A199F0 (:58)
object* alloc_obj_light(); // 0x00A66BF0 (:58)
void object_light_level_init_pre(); // 0x00A19BD0 (:335)
void object_light_system_init(); // 0x00A19BE0 (:339)
void object_light_system_shutdown(); // 0x00A19BF0 (:343)
