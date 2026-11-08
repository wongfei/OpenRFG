// Original: volition/render_lib/code/rl_material.h
// Implementation: volition/render_lib/code/rl_material.cpp
//
// rl_material_base and rl_material, a shader plus its constant and texture data (rl_material_data) with an optional
// technique override.
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_material_defines.h"
#include "volition/render_lib/code/rl_shader_defines.h"

class mempool_base;
class rl_class_dynamic_info;
struct rl_material_state_store;
class rl_renderer;

namespace keen
{
    struct Vector3;
}

// size 0x10
class rl_material_base : public rl_base_object
{
public:
    rl_material_base(); // 0x004C2A50 (:27)
    virtual ~rl_material_base(); // vtbl+0x00 0x004C2A70 (:35)
    virtual bool compare(const rl_material_base&, bool) const = 0; // vtbl+0x08
    unsigned int hash() const; // 0x004C2B30 (:93)
    unsigned int hash(); // 0x004C2B10 (:75)
    void invalidate_hash_cache(); // 0x004C2C20 (:525)
    virtual unsigned int get_name_checksum() const = 0; // vtbl+0x0C
    virtual void set_as_render_material(rl_renderer&, rl_material_state_store*, int) const = 0; // vtbl+0x10
    virtual void cleanup_render_material(rl_renderer&, rl_material_state_store*, int) const = 0; // vtbl+0x14
    virtual bool get_is_transparent() = 0; // vtbl+0x18
    virtual void set_is_transparent(bool) = 0; // vtbl+0x1C
    bool get_is_base_transparent(); // 0x004B7990 (.h:70)
    void set_is_base_transparent(bool transparent); // 0x004B79A0 (.h:71)
    virtual bool get_has_flags(int, unsigned int, bool) = 0; // vtbl+0x20
    virtual int get_num_passes(bool) = 0; // vtbl+0x24
    virtual bool can_combine_drawcalls() = 0; // vtbl+0x28
    virtual rl_material_type get_type() const = 0; // vtbl+0x2C
    void set_registered(bool is_registered); // 0x004B79C0 (.h:89)
    bool get_registered();
    void set_is_override(bool is_override); // 0x004B79E0 (.h:92)
    bool get_is_override(); // 0x004B7A00 (.h:93)
    virtual rl_material_base* clone(mempool_base*) const = 0; // vtbl+0x30
protected:
    virtual unsigned int hash_internal() const = 0; // vtbl+0x34
    void implicit_unregister(); // 0x0053EFF0 (:531)

private:
    unsigned int hash_cache; // 0x08
    unsigned int base_flags; // 0x0C
};

// size 0x1C
class rl_material : public rl_material_base
{
protected:
    rl_material(); // 0x004C2AA0 (:51)
    virtual ~rl_material(); // vtbl+0x00 0x0054BCE0 (:59)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_material* create(); // 0x004E8C90 (:15)
    static bool can_create(); // 0x004C2A20 (:15)
    virtual void release(); // vtbl+0x04 0x004C2A30 (:15)
    virtual unsigned int get_name_checksum() const; // vtbl+0x0C 0x004C2BE0 (:475)
    virtual bool compare(const rl_material_base& other_mat, bool exhaustive) const; // vtbl+0x08 0x00510D50 (:121)
    void set_material_data(rl_material_data* mat_data); // 0x004C2B50 (:104)
    const rl_material_data* get_material_data() const; // 0x004B7A10 (.h:126)
    bool get_constant_data(float* data, const char* name, int numFloats); // 0x004E8D70 (:351)
    bool get_constant_data(float* data, unsigned int which, int numFloats); // 0x004E8D30 (:334)
    bool get_texture_data(int& tex_handle, const char* name); // 0x00511220 (:422)
    bool get_texture_data(int& tex_handle, int tmu_index); // 0x005111C0 (:407)
    void set_constant_data(const char* name, float* data, int numFloats); // 0x004E8E10 (:380)
    void set_constant_data(unsigned int which, float* data, int numFloats); // 0x004E8DD0 (:362)
    void set_texture_data(const char* name, int tex_handle); // 0x00511120 (:393)
    int get_constant_loc(const char* name); // 0x004E8EA0 (:554)
    int get_constant_loc(unsigned int name_crc); // 0x004E8E80 (:547)
    bool validate_material(rl_shader_type shader_type, int flags); // 0x00511270 (:432)
    virtual rl_material_type get_type() const; // vtbl+0x2C 0x004C2AC0 (.h:141)
    virtual void set_as_render_material(rl_renderer& renderer, rl_material_state_store*, int) const; // vtbl+0x10 0x0053EE70 (:274)
    virtual void cleanup_render_material(rl_renderer& renderer, rl_material_state_store*, int) const; // vtbl+0x14 0x004E8D20 (:279)
    virtual bool get_is_transparent(); // vtbl+0x18 0x004C2B70 (:286)
    virtual void set_is_transparent(bool is_transparent); // vtbl+0x1C 0x004C2B90 (:292)
    virtual int get_num_passes(bool); // vtbl+0x24 0x004C2AD0 (.h:150)
    int get_override_technique();
    void set_override_technique(rl_shader_technique_type technique_type, int user_technique_index); // 0x004C2AF0 (:65)
    virtual bool can_combine_drawcalls(); // vtbl+0x28 0x004C2AE0 (.h:155)
    virtual bool get_has_flags(int pass_index, unsigned int flags, bool depthPass); // vtbl+0x20 0x004C2BC0 (:304)
    virtual rl_material_base* clone(mempool_base* pool) const; // vtbl+0x30 0x0053EE90 (:313)
    void draw_debug(const keen::Vector3& pos) const; // 0x004C2C10 (:488)
protected:
    virtual unsigned int hash_internal() const; // vtbl+0x34 0x00510FC0 (:218)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514D54
protected:
    int m_override_technique_index; // 0x10
    rl_shader_technique_type m_override_technique_type; // 0x14
    rl_material_data* m_data; // 0x18
};

void setDebugPlayerPosition(const keen::Vector3& pos); // 0x004C2BF0 (:483)
