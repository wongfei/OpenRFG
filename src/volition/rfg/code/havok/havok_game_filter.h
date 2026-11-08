// Original: volition/rfg/code/havok/havok_game_filter.h
// Implementation: volition/rfg/code/havok/havok_game_filter.cpp
//
// The collision filter of the game world: collision types map to one of 128 layers (get_layer), a 128x128 bit table
// (m_layer_lookup_table) says which layers collide, and system groups (in the filter info) disable collisions inside
// one system (a ragdoll, a vehicle). filter_callback / filter_params let a ray query skip objects.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"

class hkBool;
class hkpCdBody;
class hkpCollidable;
struct hkpCollisionInput;
class hkpShapeContainer;
struct hkpShapeRayCastInput;
struct hkpWorldRayCastInput;
class object;

// size 0x3C
class havok_game_filter : public hkpCollisionFilter
{
public:
    enum constants
    {
        MAX_LAYERS_SHIFT = 7,
        MAX_LAYERS = 128,
        NUM_LAYER_TABLE_ENTRIES = 16384,
        NUM_BYTES_IN_LAYER_TABLE = 2048,
        NUM_UINTS_IN_LAYER_TABLE = 512,
    };

    havok_game_filter(unsigned int num_predefined_system_groups, unsigned int* layer_lookup_table); // 0x0081BDB0 (:155)
    int getNewSystemGroup(); // 0x00A068D0 (.h:75)
    static unsigned int get_system_group(unsigned int filter_info); // 0x00A06910 (.h:91)
    static unsigned int calcFilterInfo(int layer, int systemGroup); // 0x0080D6F0 (.h:101)
    static int get_layer(unsigned int coll_type); // 0x007115D0 (.h:111)
    static unsigned int get_table_index(unsigned int layer_a, unsigned int layer_b); // 0x0080D700 (.h:118)
    virtual hkBool isCollisionEnabled(const hkpWorldRayCastInput& input_a, const hkpCollidable& coll_b) const; // vtbl+0x04 0x0081C010 (:233)
    virtual hkBool isCollisionEnabled(const hkpShapeRayCastInput& input_a, const hkpShapeContainer& cont_b, unsigned int key_b) const; // vtbl+0x00 0x00811460 (:282)
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& body_a, const hkpCdBody& body_b, const hkpShapeContainer& cont_a, const hkpShapeContainer& cont_b, unsigned int key_a, unsigned int key_b) const; // vtbl+0x00 0x00811570 (:375)
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input_a, const hkpCdBody& body_a, const hkpCdBody& body_b, const hkpShapeContainer& cont_b, unsigned int key_b) const; // vtbl+0x04 0x008114D0 (:319)
    virtual hkBool isCollisionEnabled(const hkpCollidable& coll_a, const hkpCollidable& coll_b) const; // vtbl+0x04 0x0081BF40 (:183)
    hkBool isCollisionEnabled(unsigned int info_a, unsigned int info_b) const; // 0x0080D710 (.h:124)
    virtual void enableCollisionsBetween(unsigned int layer_a, unsigned int layer_b); // vtbl+0x14 0x00811620 (:400)
    virtual void disableCollisionsBetween(unsigned int layer_a, unsigned int layer_b); // vtbl+0x18 0x00811670 (:436)
    virtual void disable_collisions_for_layer(unsigned int layer); // vtbl+0x1C 0x008116D0 (:477)
    virtual void copy_layer(unsigned int dest_layer, unsigned int source_layer); // vtbl+0x20 0x00811700 (:495)
    virtual ~havok_game_filter(); // vtbl+0x00 0x0081BE20

    static bool (*filter_callback)(object*, void*); // 0x02B82278
    static void* filter_params; // 0x02B8227C
    static unsigned int m_current_ray_flags; // 0x02B82280
    unsigned int m_num_predefined_system_groups; // 0x30
protected:
    unsigned int* m_layer_lookup_table; // 0x34
    unsigned int m_nextFreeSystemGroup; // 0x38
};

extern havok_game_filter* Havok_filter; // 0x02642F84, the world's collision filter (havok_system_init)

// size 0x1
struct havok_disable_property_asserts
{
    havok_disable_property_asserts(); // 0x00811440 (:59)
    ~havok_disable_property_asserts(); // 0x00811450 (:80)

};
