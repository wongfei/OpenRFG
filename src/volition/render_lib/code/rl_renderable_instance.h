// Original: volition/render_lib/code/rl_renderable_instance.h
// Implementation: volition/render_lib/code/rl_renderable_instance.cpp
//
// rl_renderable_instance, a scene entity that draws an rl_renderable: tint, opacity, LOD, sort method, decals and
// death callbacks; rl_compressed_transform and rl_deferred_batch_info.
#pragma once

#include "volition/render_lib/code/rl_callback_widget.h"
#include "volition/render_lib/code/rl_scene_entity.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

class matrix43;
class rl_batched_pass;
class rl_camera;
class rl_decal;
class rl_decal_parameters;
class rl_deferred_batch_info;
struct rl_deferred_batch_item;
class rl_material_map;
class rl_renderable;
class rl_renderer;

// size 0x110
class rl_renderable_instance : public rl_scene_entity
{
public:
    enum sort_method
    {
        SORT_METHOD_NONE = 0,
        SORT_METHOD_BACK_TO_FRONT = 1,
        SORT_METHOD_FRONT_TO_BACK = 2,
        SORT_METHOD_FIRST = 3,
        SORT_METHOD_LAST = 4,
    };

    enum pass_flags
    {
        PASS_FLAGS_DISTORTION = 1,
        PASS_FLAGS_GLOW = 2,
        PASS_FLAGS_HALF_RES = 4,
        PASS_FLAGS_QUARTER_RES = 8,
        PASS_FLAGS_OUTLINE_0 = 16,
        PASS_FLAGS_OUTLINE_1 = 32,
        PASS_FLAGS_OUTLINE_2 = 64,
        PASS_FLAGS_OUTLINE_3 = 128,
        PASS_FLAGS_XRAY_EFFECT = 256,
        PASS_FLAGS_ADDITIVE = 512,
        PASS_FLAGS_DISTORTION_ONLY = 1024,
        PASS_FLAGS_FORCE_SIZEOF_ENUM = -1,
    };

    enum flags
    {
        FLAGS_IS_TRANSPARENT = 32,
        FLAGS_CASTS_SHADOWS = 64,
        FLAGS_IS_BATCHABLE = 128,
        FLAGS_CASTS_DROP_SHADOWS = 256,
        FLAGS_CAST_TRANSPARENT_SHADOWS = 512,
        FLAGS_CASTS_TERRAIN_SHADOWS = 1024,
        NUM_FLAGS = 11,
    };

protected:
    rl_renderable_instance(); // 0x005433B0 (:23)
    virtual ~rl_renderable_instance(); // vtbl+0x00 0x00543750 (:35)
public:
    rl_renderable_instance* get_prev() const;
    void set_prev(rl_renderable_instance*);
    rl_renderable_instance* get_next() const;
    void set_next(rl_renderable_instance*);
    virtual void render(rl_renderer&, rl_camera&, unsigned int) = 0; // vtbl+0x10
    virtual void render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags); // vtbl+0x14 0x00543480 (.h:113)
    virtual void render_debug(); // vtbl+0x18 0x005434A0 (.h:114)
    virtual void add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids); // vtbl+0x1C 0x005434B0 (.h:122)
    virtual int get_indices_for_batch(const unsigned char** indices, unsigned int& first_index, unsigned int& block_id, unsigned int sub_id) const; // vtbl+0x20 0x005434C0 (.h:123)
    virtual int get_instance_stream_index(unsigned int sub_id) const; // vtbl+0x24 0x005434D0 (.h:124)
    virtual void get_index_range(int& index_min, int& index_max, unsigned int sub_id) const; // vtbl+0x28 0x005434E0 (.h:125)
    bool get_is_batchable() const; // 0x004BA660 (.h:365)
    void set_is_batchable(bool is_batchable); // 0x004BA670 (.h:370)
    virtual void add_to_deferred(const rl_camera* pCamera, rl_deferred_batch_info* def_ids); // vtbl+0x2C 0x005434F0 (.h:129)
    virtual void on_visible_for_frame(rl_renderer&); // vtbl+0x0C 0x00515810 (:64)
    virtual unsigned int add_decal(const rl_decal_parameters* decal_params, unsigned int reserved_handle); // vtbl+0x30 0x00543860 (:85)
    virtual void transfer_decals(rl_renderable_instance* new_owner, unsigned int decal_handle); // vtbl+0x34 0x004C4980 (:105)
    virtual void remove_decal(rl_decal* decal_p); // vtbl+0x38 0x004C4A70 (:122)
    virtual void remove_all_decals(); // vtbl+0x3C 0x004C4AD0 (:129)
    virtual void render_decals(rl_renderer&, int, rl_decal**); // vtbl+0x40 0x004C4B10 (:137)
    void set_opacity(float opacity); // 0x004DF800 (.h:311)
    float get_opacity(); // 0x004BA610 (.h:321)
    void set_tint(vector& tint); // 0x00753D60 (.h:326)
    const vector& get_tint() const; // 0x004BA580 (.h:146)
    virtual void set_renderable(rl_renderable* renderable_p); // vtbl+0x44 0x00543530 (.h:331)
    virtual rl_renderable* get_renderable() const; // vtbl+0x48 0x00543520 (.h:340)
    void set_instance_params(const vector4& instance_params); // 0x004BA620 (.h:345)
    const vector4& get_instance_params() const; // 0x006E1410 (.h:349)
    void set_lod_level(int lod_level); // 0x004BA640 (.h:354)
    int get_lod_level() const; // 0x004BA650 (.h:359)
    bool get_is_transparent() const; // 0x004BA690 (.h:379)
    void set_is_transparent(bool is_transparent); // 0x004BA6A0 (.h:384)
    bool get_casts_shadows() const;
    void set_casts_shadows(bool casts_shadows); // 0x005D0B90 (.h:398)
    bool get_casts_terrain_shadows() const;
    void set_casts_terrain_shadows(bool casts_shadows); // 0x005D0BB0 (.h:412)
    bool get_casts_drop_shadows() const;
    void set_casts_drop_shadows(bool casts_drop_shadows); // 0x005D0BD0 (.h:426)
    bool get_casts_transparent_shadows() const;
    void set_casts_transparent_shadows(bool casts_shadows); // 0x00A056C0 (.h:440)
    bool get_xray_visible() const;
    void set_xray_visible(bool xray_visible); // 0x00A056E0 (.h:449)
    bool get_distortion_visible() const;
    void set_distortion_visible(bool distortion_visible); // 0x00A05700 (.h:464)
    bool get_distortion_only() const; // 0x004BA6C0 (.h:488)
    void set_distortion_only(bool distortion_only); // 0x006C3980 (.h:479)
    void get_xray_render_params(unsigned char& mat_handle, unsigned char& opacity) const; // 0x004BA6D0 (.h:494)
    void set_xray_render_params(const unsigned char& mat_handle, const unsigned char opacity); // 0x00A05720 (.h:500)
    bool get_is_additive() const; // 0x004BA6F0 (.h:506)
    void set_is_additive(bool is_additive); // 0x00C9E200 (.h:511)
    void set_render_pass_flags(unsigned short);
    sort_method get_sort_method() const; // 0x004BA700 (.h:521)
    void set_sort_method(sort_method _sort_method); // 0x006C39A0 (.h:526)
    unsigned short get_sort_priority() const; // 0x004BA710 (.h:532)
    void set_sort_priority(unsigned short sort_priority); // 0x00C9E220 (.h:537)
    void set_outline_enabled(int outline_pass); // 0x0071AD10 (.h:553)
    void set_outline_disabled(); // 0x00710E00 (.h:563)
    void set_is_visible(bool is_visible); // 0x004DF830 (.h:542)
    virtual void set_material_map(const rl_material_map* mat_p); // vtbl+0x4C 0x00543500 (.h:224)
    virtual const rl_material_map* get_material_map(); // vtbl+0x50 0x00543510 (.h:225)
    void add_on_death_cb(rl_callback_widget* widget_p); // 0x006E13F0 (.h:228)
    void remove_on_death_cb(rl_callback_widget* widget_p); // 0x006E1400 (.h:229)
    int get_batching_complexity() const;
    int get_conditional_index();
    void set_conditional_index(int);
    int build_sorted_decal_list(rl_decal** decal_list_p, int decal_list_size); // 0x004C4BA0 (:163)
protected:
    void copy_parameters(const rl_renderable_instance* param_source); // 0x004F1690 (:74)
    static void sort_decals(rl_decal** decal_list_p, int num_decals); // 0x004C4B80 (:155)
    void set_batching_complexity(int batching_complexity); // 0x004BA590 (.h:245)
    // Not in the binary: the decal list unlink and append the original inlines (rl_renderable_instance.cpp).
    void unlink_decal(rl_decal* decal_p);
    void append_decal(rl_decal* decal_p);
private:
    void visible_update(bool is_visible); // 0x004C4910 (:57)

public:
    rl_renderable_instance* prev; // 0xC0
    rl_renderable_instance* next; // 0xC4
protected:
    vector4 m_instance_parameters; // 0xD0
    vector m_tint; // 0xE0
    rl_renderable* m_renderable_p; // 0xEC
    float m_opacity; // 0xF0
    short m_batching_complexity; // 0xF4
    short m_lod_level; // 0xF6
    rl_decal* m_decal_list; // 0xF8
private:
    rl_callback_widget_list m_on_death; // 0xFC
    sort_method m_sort_method; // 0x100
    int m_conditional_index; // 0x104
    unsigned short m_sort_priority; // 0x108
    unsigned char m_xray_opacity; // 0x10A
    unsigned char m_xray_material_handle; // 0x10B
};

// size 0x18
struct rl_compressed_transform
{
    void set(const matrix43& transform); // 0x004BA500 (.h:68)
    void get(matrix43& transform) const; // 0x004BA530 (.h:73)
    rl_compressed_transform(); // 0x004BCFA0

    vector pos; // 0x00
    float pitch; // 0x0C
    float bank; // 0x10
    float heading; // 0x14
};

// size 0xC
class rl_deferred_batch_info
{
public:
    bool add_def_id(unsigned int def_id, float dist_to_camera_sq, bool opacity_fade); // 0x004BA5A0 (.h:287)
    void init(rl_deferred_batch_item* item_array, unsigned int array_size); // 0x004BA5E0 (.h:298)
    unsigned int size(); // 0x004DF7F0 (.h:299)
    void clear(); // 0x004BA600 (.h:301)

    rl_deferred_batch_item* m_elem; // 0x00
private:
    unsigned int m_max_items; // 0x04
    volatile unsigned int m_num_items; // 0x08
};

// 0x004BA660 (.h:365). Inline here (not in rl_renderable_instance.cpp) because the headless rfg code calls it
// without rfg_render.
inline bool rl_renderable_instance::get_is_batchable() const
{
    return (m_flags & FLAGS_IS_BATCHABLE) != 0;
}
