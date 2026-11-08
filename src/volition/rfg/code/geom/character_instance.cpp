// Original: volition/rfg/code/geom/character_instance.cpp
//
// Character instances: a free list of 296 instances; character_instance_create takes one with a v_animate,
// character_instance_resources_loaded creates its anim_lib instance and skin renderable once the character is
// streamed in and links it into the used list, whose bone infos character_instance_animlib_matrices_do_frame clears
// every frame. Part of rfg_geom_unity.cpp (player_minimal.md P2).
// update_render (P7) sets the skin renderable's placement, bones, morphs and LOD; it runs only in the render update
// task (headless programs link stand-ins for its render_lib calls). The morph tables (Morph_array, Mesh_morph_hash,
// Character_morph_active) are defined here; mesh_morph.cpp (later in the unity TU) fills the first two.
// Not ported yet: Cloth_simulation_data_sizes, the props, serialization and the unloaded list.
// The external interface is declared in character_instance.h.

#include "volition/rfg/code/geom/character_instance.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_morph_target.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_mesh_shared.h"
#include "volition/rfg/code/geom/mesh_morph.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/math/matrix.h"

// The character LOD distances (m): LOD i up to the apparent distance of entry i.
const int character_instance::c_lod_distance_max = 4; // 0x012C70A4
lod_info character_instance::ms_char_lod_distances[4] = {{10.0f}, {30.0f}, {50.0f}, {100.0f}}; // 0x0165F3AC

// ---- file-static globals
static character_instance* Character_inst_free_list; // 0x025694C8
static character_instance* Character_inst_used_list; // 0x025BD388
static character_instance Character_instances[296]; // 0x025BD3C0
static mesh_morph_entry Morph_array[256]; // 0x025E2940
static bool Character_morph_active; // 0x025EF8B3
static character_instance* Character_inst_unloaded_list; // 0x025EF8C0
static stream2_data_hash<mesh_morph_entry*,256,unsigned short> Mesh_morph_hash; // 0x025EF9A8

// The circular lists of character instances (the inlined list macros of the original).
static void list_append(character_instance*& list, character_instance* ci)
{
    if (list == nullptr) {
        ci->next = ci;
        ci->prev = ci;
        list = ci;
        return;
    }
    ci->prev = list->prev;
    ci->next = list;
    list->prev->next = ci;
    list->prev = ci;
}

static void list_remove(character_instance*& list, character_instance* ci)
{
    if (list == ci) {
        list = ci->next;
        if (list == ci) {
            list = nullptr;
            ci->prev = nullptr;
            ci->next = nullptr;
            return;
        }
    }
    if (ci->prev != nullptr) {
        ci->prev->next = ci->next;
    }
    if (ci->next != nullptr) {
        ci->next->prev = ci->prev;
    }
    ci->prev = nullptr;
    ci->next = nullptr;
}

// 0x007F21C0
animation_group character_instance::get_animation_group()
{
    return vp->default_group_index;
}

// 0x007F21D0
animation_state character_instance::get_animation_state()
{
    if (vp->desired_state != nullptr) {
        return (animation_state)vp->desired_state->index;
    }
    return ANIMATION_STATE_NONE;
}

// 0x007F21F0. The box of the bone positions (model space); a standing human's box without matrices.
void character_instance::get_exact_local_bbox(vector& bmin, vector& bmax)
{
    const int num_bones = animlib_get_num_bones(aim_handle);
    matrix43* matrices;
    if (!animlib_get_instanced_model_matrices(aim_handle, &matrices)) {
        bmin.x = -0.5f;
        bmin.y = 0.0f;
        bmin.z = -0.5f;
        bmax.x = 0.5f;
        bmax.y = 1.8f;
        bmax.z = 0.5f;
        return;
    }
    vector pos;
    matrices[0].get_translation(pos);
    bmin = pos;
    bmax = pos;
    for (int i = 1; i < num_bones; i++) {
        matrices[i].get_translation(pos);
        if (0.0f <= bmin.x - pos.x) {
            bmin.x = pos.x;
        }
        if (bmax.x - pos.x < 0.0f) {
            bmax.x = pos.x;
        }
        if (0.0f <= bmin.y - pos.y) {
            bmin.y = pos.y;
        }
        if (!(0.0f <= bmax.y - pos.y)) {
            bmax.y = pos.y;
        }
        if (0.0f <= bmin.z - pos.z) {
            bmin.z = pos.z;
        }
        if (!(0.0f <= bmax.z - pos.z)) {
            bmax.z = pos.z;
        }
    }
}

// 0x007F23A0. string_pool_size is not used.
void character_instance_system_init(int string_pool_size)
{
    Character_inst_free_list = nullptr;
    Character_inst_used_list = nullptr;
    Character_inst_unloaded_list = nullptr;
    for (character_instance& ci : Character_instances) {
        list_append(Character_inst_free_list, &ci);
    }
}

// 0x007F24C0. Before the frame's animation: the per-frame anim_lib state and every instance's bone infos.
void character_instance_animlib_matrices_do_frame()
{
    animlib_do_frame_pre_transform(true);
    character_instance* ci = Character_inst_used_list;
    if (ci == nullptr) {
        return;
    }
    do {
        animlib_clear_bone_info(ci->aim_handle);
        ci = ci->next;
    } while (ci != Character_inst_used_list);
}

// 0x007FB710. (The instance goes back to the free list without leaving the used list, as the original; the callers
// unload its resources first.)
void character_instance_delete(character_instance* ci)
{
    if (ci == nullptr) {
        return;
    }
    vanim_delete(ci->vp);
    list_append(Character_inst_free_list, ci);
}

// 0x007FB750
void character_instance::set_action_dest_position(vector& current_pos, matrix& current_orient, vector world_pos, int action_alias_index, float time, bool ignore_y)
{
    vanim_set_action_dest_position(vp, current_pos, current_orient, world_pos, action_alias_index, time, ignore_y);
}

// 0x007FB7A0
void character_instance::set_action_dest_orient(matrix& current_orient, const matrix& dest_orient, int action_alias_index, float time)
{
    vanim_set_action_dest_orient(vp, current_orient, dest_orient, action_alias_index, time);
}

// 0x007FB7D0
bool character_instance::is_in_state(int state_alias_index)
{
    const v_anim* state = vp->desired_state;
    return state != nullptr && (state->index == state_alias_index || state->alias == state_alias_index || state->instance == (unsigned int)state_alias_index);
}

// 0x007FB800
bool character_instance::is_playing_action(int action_alias_index)
{
    return vanim_is_playing_action(vp, action_alias_index);
}

// 0x007FB840
unsigned int character_instance::get_action_anim_instance_handle(int action_alias_index)
{
    return vanim_get_anim_instance_handle(vp, action_alias_index);
}

// 0x007FB900. Without a priority flag (0x20 / 0x40) the action plays at priority 0x10.
unsigned int character_instance::play_action(animation_action anim_index, vanim_aliases anim_alias, float speed_multiplier, unsigned int flags)
{
    if ((flags & 0x60) == 0) {
        flags |= 0x10;
    }
    return vanim_play_action(vp, index_alias(anim_index, anim_alias), flags, speed_multiplier, 1.0f);
}

// 0x007FB950. Ramp out frames are 30 fps frames.
void character_instance::stop_action(int action_alias_index, int ramp_out_frames, bool pause)
{
    if (pause) {
        vanim_pause_and_ramp_out_action(vp, action_alias_index, ramp_out_frames);
        return;
    }
    vanim_ramp_out_action(vp, action_alias_index, ramp_out_frames);
}

// 0x007FB9C0. The skin renderable's local bounding sphere.
void character_instance::get_bounding_sphere(vector& center, float& radius)
{
    const resource_dependent_data* data = rdd.get_data();
    if (data == nullptr) {
        center.zero();
        radius = 0.0f;
        return;
    }
    const rl_sphere sphere = data->renderable_instance_p->get_local_bounding_sphere();
    center = sphere.m_position;
    radius = sphere.m_radius;
}

// 0x007FBA30
void character_instance_resources_unloaded(character_instance* char_inst)
{
    character_instance::resource_dependent_data* data = char_inst->rdd.get_data();
    if (data == nullptr) {
        return;
    }
    rl_skin_instance* skin = data->renderable_instance_p;
    data->base_character = nullptr;
    skin->release();
    data->renderable_instance_p = nullptr;
    animlib_delete_anim_instance(char_inst->aim_handle);
    char_inst->aim_handle = -1;
    char_inst->vp->aim_handle = -1;
    list_remove(Character_inst_used_list, char_inst);
    char_inst->rdd.set_loaded(false);
}

// 0x007FBAC0 (:719)
// The render alpha and the character scale as properties of the object's save block.
void character_instance_serialize(character_instance* char_inst, obj_prop_block& pblock, bool load)
{
    if (load)
    {
        pblock.get(PROP_CHAR_INST_RENDER_ALPHA, char_inst->render_alpha);
        pblock.get(PROP_CHAR_INST_CHARACTER_SCALE, char_inst->character_scale);
        return;
    }
    pblock.set(PROP_CHAR_INST_RENDER_ALPHA, char_inst->render_alpha);
    pblock.set(PROP_CHAR_INST_CHARACTER_SCALE, char_inst->character_scale);
}

// 0x00801FB0
character_instance* character_instance_create(float character_scale, const char* anim_set_name)
{
    character_instance* ci = Character_inst_free_list;
    if (ci == nullptr) {
        return nullptr;
    }
    ci->vp = vanim_create(anim_set_name);
    if (ci->vp == nullptr) {
        return nullptr;
    }
    list_remove(Character_inst_free_list, ci);
    ci->buf_verts_shadow_offset = 0;
    ci->buf_frame_verts = nullptr;
    ci->rdd.set_loaded(false);
    ci->ci_flags = 0;
    ci->render_alpha = 0xFF;
    ci->cache_lod = 0;
    ci->aim_handle = -1;
    ci->character_scale = character_scale;
    return ci;
}

// 0x00802040
void character_instance::set_state(index_alias state_index, index_alias action_index, float blend_time, bool start_at_zero)
{
    vanim_change_state(vp, state_index, action_index, blend_time, start_at_zero);
}

// 0x00802080 (:157)
void character_instance::set_action_starting_orient(vector& current_pos, matrix& current_orient, vector& dest_pos, matrix& dest_orient, int action_alias_index, float time)
{
    vanim_set_action_starting_orient(vp, current_pos, current_orient, dest_pos, dest_orient, action_alias_index, time);
}

// 0x008020C0. The skin renderable's local box.
void character_instance::get_bbox(vector& min, vector& max)
{
    const resource_dependent_data* data = rdd.get_data();
    if (data == nullptr) {
        min.zero();
        max.zero();
        return;
    }
    const rl_aabb box = data->renderable_instance_p->get_local_aabb();
    min = box.min;
    max = box.max;
}

// 0x008021D0. The anim_lib instance on the character's rig and its skin renderable (lod 0 submesh) with the bones;
// fails (unloaded again) when either cannot be allocated.
bool character_instance_resources_loaded(character_instance* char_inst, character* base_character, unsigned char rl_category)
{
    char_inst->rdd.set_loaded(true);
    character_instance::resource_dependent_data* data = char_inst->rdd.get_data();
    char_inst->aim_handle = animlib_new_anim_instance_manager(base_character->rig, base_character->bones_used_info.unique_id);
    if (char_inst->aim_handle == -1) {
        char_inst->rdd.set_loaded(false);
        return false;
    }
    const int num_bones = animlib_get_num_bones(char_inst->aim_handle);
    if (!rl_skin_instance::can_allocate_bones(num_bones)) {
        animlib_delete_anim_instance(char_inst->aim_handle);
        char_inst->aim_handle = -1;
        char_inst->rdd.set_loaded(false);
        return false;
    }
    data->renderable_instance_p = rl_skin_instance::create();
    if (data->renderable_instance_p == nullptr) {
        animlib_delete_anim_instance(char_inst->aim_handle);
        char_inst->aim_handle = -1;
        char_inst->rdd.set_loaded(false);
        return false;
    }
    data->base_character = base_character;
    const character_mesh* cmesh = base_character->m_data;
    data->renderable_instance_p->init(cmesh->m_shared_header.m_mesh.to_pointer(), cmesh->m_shared_header.m_material_map.to_pointer(), (unsigned int)cmesh->m_lod_submesh_id.to_pointer()[0]);
    data->renderable_instance_p->allocate_bones(num_bones);
    data->renderable_instance_p->calculate_local_aabb();
    vector bmin;
    vector bmax;
    char_inst->get_bbox(bmin, bmax);
    const float dx = bmax.x - bmin.x;
    const float dy = bmax.y - bmin.y;
    const float dz = bmax.z - bmin.z;
    float dimension = dx;
    if (dx - dy < 0.0f) {
        dimension = dy;
    }
    if (dimension - dz < 0.0f) {
        dimension = dz;
    }
    char_inst->bbox_max_dimension = dimension;
    char_inst->vp->aim_handle = char_inst->aim_handle;
    list_append(Character_inst_used_list, char_inst);
    return true;
}

// 0x0080A820. A new group: the given state, or the current state in the new group.
void character_instance::set_animation_group(animation_group group_index, index_alias state_index, float blend_time, bool start_at_zero)
{
    vp->default_group_index = group_index;
    if (state_index.index == -1 && vp->desired_state != nullptr) {
        state_index.index = vp->desired_state->index;
        state_index.alias = ANIM_ALIAS_NONE;
    }
    vanim_change_state(vp, state_index, index_alias(-1, (vanim_aliases)-1), blend_time, start_at_zero);
}

// 0x0080B9A0 (:249). The skin renderable's local box, position and orientation (the inlined rl_scene_entity setters),
// then the scene update; unless transform_only, the bone matrices of the anim_lib instance, the morph weights of its
// morph animations (at most 20; with Character_morph_active even without any) and the sub mesh of the LOD level
// (capped at the mesh's last LOD).
// Notes (decompiled behaviour, kept as is): the resource data is not checked (the callers check m_loaded first).
void character_instance::update_render(const vector& lpos, const matrix& lorient, const vector& bmin, const vector& bmax, int lod_level, bool transform_only)
{
    resource_dependent_data* data = rdd.get_data();
    data->renderable_instance_p->set_local_aabb(rl_aabb(bmin, bmax));
    data->renderable_instance_p->set_position(lpos);
    data->renderable_instance_p->set_rotation(lorient);
    game_render_scene_update(data->renderable_instance_p);
    if (transform_only) {
        return;
    }
    data->renderable_instance_p->release_morph_data();
    const matrix43* bone_matrices = animlib_get_render_matrices(aim_handle);
    if (bone_matrices == nullptr) {
        return;
    }
    data->renderable_instance_p->set_bones_matricies(bone_matrices, animlib_get_num_bones(aim_handle));
    anim_morph_info* morph_info = nullptr;
    int num_morphs = animlib_get_morph_info(aim_handle, &morph_info);
    // Magic number: at most 20 morph animations.
    if (num_morphs > 0x14) {
        num_morphs = 0x14;
    }
    if (num_morphs != 0 || Character_morph_active) {
        mesh_morph_entry* entry = nullptr;
        Mesh_morph_hash.get(data->base_character->name, data->base_character->srid, entry);
        if (entry != nullptr && entry->index != -1) {
            rl_morph_container* container = nullptr;
            // Magic number: the 256 entries of Morph_array.
            if ((unsigned int)entry->index < 0x100) {
                container = Morph_array[entry->index].data;
            }
            for (int i = 0; i < num_morphs; i++) {
                const int target = container->find_morph_target(morph_info[i].name_check);
                if (target != -1) {
                    data->renderable_instance_p->activate_morph(container, target, morph_info[i].blend_amount);
                }
            }
        }
    }
    const character_mesh* cmesh = data->base_character->m_data;
    const int last_lod = (int)cmesh->m_num_lods - 1;
    if (last_lod <= lod_level) {
        lod_level = last_lod;
    }
    data->renderable_instance_p->set_submesh_index((unsigned int)cmesh->m_lod_submesh_id.to_pointer()[lod_level]);
}
