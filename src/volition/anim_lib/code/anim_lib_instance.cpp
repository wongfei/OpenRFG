// Original: volition/anim_lib/code/anim_lib_instance.cpp
//
// anim_instance: bone and tag lookup by name and the model space position of a tag (a bone's model matrix, or a tag's
// transform on its parent bone's). Part of anim_lib_unity.cpp (player_minimal.md P2).
// The external interface is declared in anim_lib.h.

#include "volition/anim_lib/code/anim_lib.h"

#include "volition/vlib/code/util/string_comp.h"

#include <math.h>

// 0x00C8AE50. The bind pose position of a bone (its relative translations up the hierarchy).
void anim_instance::get_virtual_bone_pos(int bone_index, vector& pos)
{
    pos = rig->bones[bone_index].rel_bone_translation;
    for (int parent = rig->bones[bone_index].parent_index; parent != -1; parent = rig->bones[parent].parent_index) {
        pos.x = rig->bones[parent].rel_bone_translation.x + pos.x;
        pos.y = rig->bones[parent].rel_bone_translation.y + pos.y;
        pos.z = rig->bones[parent].rel_bone_translation.z + pos.z;
    }
}

// 0x00C8AF10. Indices below num_bones are bones, the next num_tags are tags; anything else is the identity.
matrix43 anim_instance::calculate_final_transformation_matrix(int tag_bone_index)
{
    if (tag_bone_index > -1 && cache_instanced_model != nullptr) {
        if (tag_bone_index < rig->num_bones) {
            return cache_instanced_model[tag_bone_index];
        }
        const int tag = tag_bone_index - rig->num_bones;
        if (tag > -1 && tag < rig->num_tags) {
            return rig->tags[tag].transformation * cache_instanced_model[rig->tags[tag].parent_index];
        }
    }
    return IDENTITY_MATRIX43;
}

// 0x00C8AFD0
matrix43 anim_instance::get_bone_transformation(int bone_index)
{
    return calculate_final_transformation_matrix(bone_index);
}

// 0x00C95DC0
int anim_instance::find_bone(const char* name)
{
    for (int i = 0; i < rig->num_bones; i++) {
        if (v_stricmp(name, rig->bones[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

// 0x00C95E10. Tags are numbered after the bones.
int anim_instance::find_tag(const char* name)
{
    const int num_bones = rig->num_bones;
    const int num_tags = rig->num_tags;
    for (int i = 0; i < num_tags; i++) {
        if (v_stricmp(name, rig->tags[i].name) == 0) {
            return i + num_bones;
        }
    }
    return -1;
}

// 0x00C95E80. The num_tags-th tag of the name (counting from 1).
int anim_instance::find_nth_tag(const char* name, int num_tags)
{
    const int count = rig->num_tags;
    const int num_bones = rig->num_bones;
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (v_stricmp(name, rig->tags[i].name) == 0 && ++found == num_tags) {
            return i + num_bones;
        }
    }
    return -1;
}

// 0x00C95F00. A tag of the name for one of the active variant ids: a match on a non-zero vid wins, vid 0 matches only
// as a fallback when several vids are active, and a tag without vid (-1) is the last resort.
int anim_instance::find_tag_with_active_vids(const char* name, int num_active_vids, const int* active_vids)
{
    int any_vid = -1;
    int vid_zero = -1;
    const int num_bones = rig->num_bones;
    const int num_tags = rig->num_tags;
    for (int i = 0; i < num_tags; i++) {
        if (v_stricmp(name, rig->tags[i].name) != 0) {
            continue;
        }
        const int vid = rig->tags[i].vid;
        if (vid == -1) {
            any_vid = i + num_bones;
            continue;
        }
        for (int v = 0; v < num_active_vids; v++) {
            if (active_vids[v] == vid) {
                if (num_active_vids < 2 || vid != 0) {
                    return i + num_bones;
                }
                vid_zero = i + num_bones;
                break;
            }
        }
    }
    if (num_tags > 0 && vid_zero != -1) {
        return vid_zero;
    }
    return any_vid;
}

// 0x00C8B260. The tag's model space position and orientation; the rows are renormalized when the first one is not
// of unit length (scaled bones).
void anim_instance::get_tag_local_coords(int tag_index, vector& pos, matrix* orient)
{
    matrix43 m = calculate_final_transformation_matrix(tag_index);
    pos = m.m_translation;
    if (orient != nullptr) {
        if (0.001f < fabsf(m.m_rotation.rvec.mag_and_normalize_safe() - 1.0f)) {
            m.m_rotation.uvec.normalize_safe();
            m.m_rotation.fvec.normalize_safe();
        }
        *orient = m.m_rotation;
    }
}

// 0x00C8B300. The tag in world space for an entity at base_pos / base_orient.
void anim_instance::get_tag_world_coords(int tag_index, const matrix& base_orient, const vector& base_pos, vector& pos, matrix* orient)
{
    get_tag_local_coords(tag_index, pos, orient);
    if (orient != nullptr) {
        *orient = *orient * base_orient;
    }
    pos.unrotate(base_orient);
    pos.x = base_pos.x + pos.x;
    pos.y = base_pos.y + pos.y;
    pos.z = base_pos.z + pos.z;
}

// 0x00C8AFF0. Overrides a bone's model matrix (and its skinning matrix).
void anim_instance::set_bone_transformation(int entity_bone_index, const vector& pos, const matrix& orient)
{
    if (cache_instanced_model == nullptr || cache_vertex_to_model == nullptr) {
        return;
    }
    matrix43& bone = cache_instanced_model[entity_bone_index];
    bone.m_translation = pos;
    bone.m_rotation = orient;
    const int rig_bone = entity_bone_info->entity_to_rig_bone_mapping[entity_bone_index];
    cache_vertex_to_model[entity_bone_index].m_translation = rig->bones[rig_bone].inv_translation * bone;
    cache_vertex_to_model[entity_bone_index].m_rotation = bone.m_rotation;
}
