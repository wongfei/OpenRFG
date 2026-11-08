// Original: volition/render_lib/code/rl_skin_instance.h
// Implementation: volition/render_lib/code/rl_skin_instance.cpp
//
// rl_skin_instance, a skinned mesh instance (characters) with its bone matrices.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_mesh_instance.h"

class matrix43;
class mempool;
class rl_camera;
class rl_class_factory_info;
class rl_decal;
class rl_dynamic_morph_instance;
class rl_morph_container;
class rl_renderer;

// size 0x130
class rl_skin_instance : public rl_mesh_instance
{
protected:
    rl_skin_instance(); // 0x0054E070 (:45)
    virtual ~rl_skin_instance(); // vtbl+0x00 0x0054E0C0 (:61)
private:
    static const pool_list<rl_skin_instance*>& get_object_pool(); // 0x004C5B60 (.h:27)
public:
    static void set_objects_max(int objects_max); // 0x007C1F10 (.h:27)
    static int get_objects_max(); // 0x004C5B70 (.h:27)
    static int get_objects_used();
    static int get_objects_free(); // 0x006E87F0 (.h:27)
    static void factory_init(mempool& factory_mempool); // 0x00518CD0 (:42)
    static rl_skin_instance* create_placement(rl_skin_instance*);
    static rl_skin_instance* create(); // 0x00550D30 (:42)
    static bool can_create(); // 0x004FCCE0 (:42)
    virtual void release(); // vtbl+0x04 0x00530A40 (:42)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int flags); // vtbl+0x10 0x00530AC0 (:73)
    void prerender(rl_renderer& renderer); // 0x004FCCF0 (:67)
    virtual void render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags); // vtbl+0x14 0x00530B40 (:94)
    virtual void set_shader_types(rl_renderer& renderer); // vtbl+0x54 0x004FCD10 (:127)
    void set_bones_matricies(const matrix43* bone_matrices, int num_bone_matrices); // 0x004D1380 (:144)
    void release_morph_data(); // 0x004D1400 (:169)
    bool activate_morph(rl_morph_container* container_p, int target, float weight); // 0x00550DE0 (:177)
    virtual void render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p); // vtbl+0x40 0x00530BC0 (:244)
    virtual void on_visible_for_frame(rl_renderer& renderer); // vtbl+0x0C 0x00546590 (:318)
    static bool can_allocate_bones(int num_bones); // 0x004D1420 (:332)
    bool allocate_bones(int num_bones); // 0x004D14B0 (:378)
private:
    void release_bones(); // 0x004D1450 (:351)

public:
    static const unsigned int m___class_id; // 0x012BEE48
    static rl_class_factory_info ms_class_factory_info; // 0x015162A8
protected:
    static pool_list<rl_skin_instance*> ms_object_pool; // 0x015162D8
private:
    static const int num_bones_matrices_max; // 0x012BEE4C
    int m_num_allocated_bones; // 0x120
    matrix43* m_bone_matrices_p; // 0x124
    int m_num_bones_matrices; // 0x128
    rl_dynamic_morph_instance* m_morph_instance_p; // 0x12C
};

void rl_skin_bone_manager_init(); // 0x004D1550 (:413)
void rl_skin_bone_manager_shutdown(); // 0x004D15F0 (:454)
void rl_skin_bone_manager_set_pool_sizes(int num_large_slots, int num_small_slots); // 0x004D1600 (:471)
void rl_skin_bone_manager_set_thread_ownership(); // 0x004D1620 (:479)
