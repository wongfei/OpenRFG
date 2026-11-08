// Original: volition/render_lib/code/rl_layered_material.h
// Implementation: volition/render_lib/code/rl_layered_material.cpp
//
// rl_layered_material, a material blended from several layers.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_defines.h"

class mempool;
class mempool_base;
class rl_class_factory_info;
struct rl_material_state_store;
class rl_renderer;

// size 0x24
class rl_layered_material : public rl_material_base
{
protected:
    rl_layered_material(); // 0x004E8AE0 (:54)
    virtual ~rl_layered_material(); // vtbl+0x00 0x0054BB90 (:59)
private:
    static const pool_list<rl_layered_material*>& get_object_pool(); // 0x004C22F0 (.h:38)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004C2300 (.h:38)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00510670 (:51)
    static rl_layered_material* create_placement(rl_layered_material* _data_p); // 0x00510770 (:51)
    static rl_layered_material* create(); // 0x00526BA0 (:51)
    static bool can_create(); // 0x004E8AD0 (:51)
    virtual void release(); // vtbl+0x04 0x005107A0 (:51)
    void init(unsigned int name_checksum, rl_material_layer* layers, int num_layers); // 0x00510820 (:88)
    void init(const char* name, rl_material_layer* layers, int num_layers); // 0x00526C10 (:74)
    virtual bool can_combine_drawcalls(); // vtbl+0x28 0x004E8B00 (.h:45)
    virtual bool compare(const rl_material_base& other_mat, bool exhaustive) const; // vtbl+0x08 0x004C2360 (:158)
    virtual unsigned int get_name_checksum() const; // vtbl+0x0C 0x004E8B20 (.h:52)
    virtual void set_as_render_material(rl_renderer& renderer, rl_material_state_store* pStateStore, int requested_pass_index) const; // vtbl+0x10 0x00526C40 (:195)
    virtual void cleanup_render_material(rl_renderer& renderer, rl_material_state_store* pStateStore, int requested_pass_index) const; // vtbl+0x14 0x00526FE0 (:306)
    virtual bool get_is_transparent(); // vtbl+0x18 0x004E8B30 (.h:58)
    virtual void set_is_transparent(bool is_transparent); // vtbl+0x1C 0x004E8B40 (.h:59)
    virtual bool get_has_flags(int pass_index, unsigned int flags, bool depthPass); // vtbl+0x20 0x00510A00 (:353)
    virtual int get_num_passes(bool depthPass); // vtbl+0x24 0x004C23D0 (:388)
    virtual rl_material_type get_type() const; // vtbl+0x2C 0x004E8B60 (.h:68)
    virtual rl_material_base* clone(mempool_base* pool) const; // vtbl+0x30 0x005271D0 (:375)
protected:
    virtual unsigned int hash_internal() const; // vtbl+0x34 0x005105A0 (:28)
    bool layer_needed_in_depth_only(int layer_index) const; // 0x004C2310 (:145)

public:
    static const unsigned int m___class_id; // 0x012BEE20
    static rl_class_factory_info ms_class_factory_info; // 0x01514C7C
protected:
    static pool_list<rl_layered_material*> ms_object_pool; // 0x01514CAC
    static const unsigned int MAX_LAYERS; // 0x012BEE24
    unsigned int m_name_checksum; // 0x10
    short m_total_base_passes; // 0x14
    short m_total_depth_passes; // 0x16
    short m_base_surface_index; // 0x18
    short m_num_layers; // 0x1A
    unsigned int m_flags; // 0x1C
    rl_material_layer* m_layers; // 0x20
};
