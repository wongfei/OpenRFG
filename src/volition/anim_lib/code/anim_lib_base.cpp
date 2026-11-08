// Original: volition/anim_lib/code/anim_lib_base.cpp
//
// The rig table: Anim_rig holds every loaded .rig_pc (name, bones, tags, the bone name checksums); anim_rig_new takes a
// free slot and resolves the file's offsets in place (set_rig_data). Part of anim_lib_unity.cpp, which also places the
// TU-private types shared with the other anim_lib files here (player_minimal.md P2).
// The external interface is declared in anim_lib_internal.h.

#include "volition/anim_lib/code/anim_lib_internal.h"
#include "volition/anim_lib/code/anim_lib.h"

#include "volition/vlib/code/memory/vmemory.h"

#include "keen/base/trace.hpp"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include <string.h>

// ---- TU-private types (rfg.pdb; anim_lib.h names them by pointer only; added in player_minimal.md P0)
// A page of animation key data owned by an instance manager (doubly linked, aged for reuse).
// size 0x14
struct animlib_page_pool_manager
{
    animlib_page_pool_manager* next; // 0x00
    animlib_page_pool_manager* prev; // 0x04
    anim_instance_manager* aim; // 0x08
    void* buf; // 0x0C
    int age; // 0x10, frames until the page is released (allocate_* set it to 2, animlib_do_frame_pre_transform counts it down)
};

// The bone mapping between an entity class (mesh) and its rig. Mappings are -1 for unmapped bones.
// size 0xD0
struct anim_entity_class_bone_info
{
    int unique_id; // 0x00
    int num_bones; // 0x04
    int num_common_bones; // 0x08
    signed char entity_to_rig_bone_mapping[64]; // 0x0C
    signed char rig_to_entity_bone_mapping[64]; // 0x4C
    unsigned char use_special_blend_mode[64]; // 0x8C, blend only the heading (get_local_bone_matrices)
    unsigned int padding; // 0xCC
};

// The original's circular doubly linked lists of page pool managers (inlined list macros, decompiled).
static void animlib_list_remove(animlib_page_pool_manager*& list, animlib_page_pool_manager* p)
{
    if (list == p && (list = p->next) == p) {
        list = nullptr;
    } else {
        if (p->prev != nullptr) {
            p->prev->next = p->next;
        }
        if (p->next != nullptr) {
            p->next->prev = p->prev;
        }
    }
    p->prev = nullptr;
    p->next = nullptr;
}

static void animlib_list_append(animlib_page_pool_manager*& list, animlib_page_pool_manager* p)
{
    if (list == nullptr) {
        p->next = p;
        p->prev = p;
        list = p;
    } else {
        p->prev = list->prev;
        p->next = list;
        list->prev->next = p;
        list->prev = p;
    }
}

// ---- globals
anim_rig Anim_rig[290]; // 0x038BD6F8
static animlib_page_pool_manager* Small_page_pool_manager_list; // 0x038C2888
static anim_entity_class_bone_info Animlib_entity_class_bone_info[148]; // 0x038C2890

// ---- internal functions
// Not reached (LTCG: list_size ECX, list EDX, val ESI; not mapped).
static void add_int_to_list(int val, int max_list_size, int& list_size, int* list); // 0x00C8AD00 (:100)

// 0x00C8ACF0
void anim_rig::clear()
{
    memset(this, 0, sizeof(anim_rig));
}

// 0x00C95920
int anim_rig::find_bone_by_substring(const char* str) const
{
    for (int i = 0; i < num_bones; i++) {
        if (str_stri(bones[i].name, str) != nullptr) {
            return i;
        }
    }
    return -1;
}

// 0x00C95970
int anim_rig::find_bone_by_name(const char* bone_name) const
{
    for (int i = 0; i < num_bones; i++) {
        if (v_stricmp(bones[i].name, bone_name) == 0) {
            return i;
        }
    }
    return -1;
}

// 0x00C959C0. Tags are numbered after the bones.
int anim_rig::find_tag_by_name(const char* tag_name) const
{
    for (int i = 0; i < num_tags; i++) {
        if (v_stricmp(tag_name, tags[i].name) == 0) {
            return num_bones + i;
        }
    }
    return -1;
}

// 0x00C8AD10. Breadth-first list of the bones under root_bone_index that the entity uses.
void anim_rig::get_bone_tree_list(int root_bone_index, const signed char* entity_rig_bones, int max_tree_bones, int& num_tree_bones, int* bone_tree_list) const
{
    num_tree_bones = 0;
    if (max_tree_bones != 0) {
        bone_tree_list[0] = root_bone_index;
        num_tree_bones++;
    }
    for (int i = 0; i < num_tree_bones; i++) {
        const int parent = bone_tree_list[i];
        for (int b = parent + 1; b < num_bones; b++) {
            if (bones[b].parent_index == parent && entity_rig_bones[b] != -1 && num_tree_bones != max_tree_bones) {
                bone_tree_list[num_tree_bones] = b;
                num_tree_bones++;
            }
        }
    }
}

// 0x00C9B210. A copy of the rig in pool with its own tables and names. Magic numbers: the rig is 0x48 bytes, a bone 0x24
// and a tag 0x3C. Allocation failures are not checked, as the original.
anim_rig* anim_rig::duplicate_rig(mempool* pool) const
{
    anim_rig* copy = (anim_rig*)pool->alloc(sizeof(anim_rig), 0);
    memcpy(copy, this, sizeof(anim_rig));
    copy->bone_name_chksums = (unsigned int*)pool->alloc(copy->num_bones * sizeof(unsigned int), 0);
    memcpy(copy->bone_name_chksums, bone_name_chksums, copy->num_bones * sizeof(unsigned int));
    copy->bones = (anim_bone*)pool->alloc(copy->num_bones * sizeof(anim_bone), 0);
    memcpy(copy->bones, bones, copy->num_bones * sizeof(anim_bone));
    copy->tags = (anim_tag*)pool->alloc(copy->num_tags * sizeof(anim_tag), 0);
    memcpy(copy->tags, tags, copy->num_tags * sizeof(anim_tag));
    for (int i = 0; i < copy->num_bones; i++) {
        const char* name_src = bones[i].name.to_pointer();
        char* name_dst = (char*)pool->alloc((unsigned int)strlen(name_src) + 1, 1);
        copy->bones[i].name.from_pointer(name_dst);
        strcpy(name_dst, name_src);
    }
    for (int i = 0; i < copy->num_tags; i++) {
        const char* name_src = tags[i].name.to_pointer();
        char* name_dst = (char*)pool->alloc((unsigned int)strlen(name_src) + 1, 1);
        copy->tags[i].name.from_pointer(name_dst);
        strcpy(name_dst, name_src);
    }
    return copy;
}

// 0x00C95A10 (inlined into anim_rig_new). The file is an anim_rig_serialized_data followed by the bone name checksums,
// the bones, the tags and the names; the three table offsets are relative to the end of the header and of the tables
// before them, and each name offset becomes self-relative (et_ptr_offset).
void anim_rig::set_rig_data(unsigned char* data, unsigned int l_data_size)
{
    // A copy of the header (the name offsets below are patched in place).
    alignas(4) unsigned char raw[sizeof(anim_rig_serialized_data)];
    memcpy(raw, data, sizeof(raw));
    const anim_rig_serialized_data& header = *(const anim_rig_serialized_data*)raw;
    // An offset of -1 or 0 relative to the table start means "none".
    const int chksum_offset = header.bone_name_chksums.get_offset();
    if (chksum_offset == -0x41 || chksum_offset == -0x40) {
        bone_name_chksums = nullptr;
    } else {
        bone_name_chksums = (unsigned int*)(data + chksum_offset + 0x40);
    }
    int table = header.num_bones * 4 + 0x40;
    unsigned char* p = data + header.bones.get_offset() + table;
    bones = (p == (unsigned char*)-1 || p == nullptr) ? nullptr : (anim_bone*)p;
    table += header.num_bones * (int)sizeof(anim_bone);
    p = data + header.tags.get_offset() + table;
    tags = (p == (unsigned char*)-1 || p == nullptr) ? nullptr : (anim_tag*)p;
    table += header.num_tags * (int)sizeof(anim_tag);
    for (int i = 0; i < header.num_bones; i++) {
        int* field = (int*)&bones[i].name;
        if (*field != -1) {
            *field = (int)(data + table + *field) - (int)field;
        }
    }
    for (int i = 0; i < header.num_tags; i++) {
        int* field = (int*)&tags[i].name;
        if (*field != -1) {
            *field = (int)(data + table + *field) - (int)field;
        }
    }
    memcpy(name, header.name, sizeof(name));
    num_common_bones = header.num_common_bones;
    flags = header.flags;
    num_bones = header.num_bones;
    num_virtual_bones = header.num_virtual_bones;
    original_buffer = data;
    num_tags = header.num_tags;
    data_size = l_data_size;
}

// 0x00C8ADA0
void anim_rig_system_init()
{
    memset(Anim_rig, 0, sizeof(Anim_rig));
}

// 0x00C8ADC0
void anim_rig_system_shutdown()
{
    for (anim_rig& rig : Anim_rig) {
        if (rig.flags & 1) {
            rig.clear();
        }
    }
}

// 0x00C95B60. The rig keeps the caller's buffer; flags bit 0 is "in use", bit 1 "permanent".
anim_rig* anim_rig_new(const char* filename, unsigned char* data_buffer, unsigned int buffer_size, bool permanent)
{
    if (strlen(filename) >= 0x20) {
        char message[512];
        _snprintf_safe(message, sizeof(message), "Rig name too long: %s", filename);
        return nullptr;
    }
    for (anim_rig& rig : Anim_rig) {
        if ((rig.flags & 1) == 0) {
            rig.clear();
            rig.set_rig_data(data_buffer, buffer_size);
            strcpy(rig.name, filename);
            rig.flags |= 1;
            if (permanent) {
                rig.flags |= 2;
            }
            return &rig;
        }
    }
    keen::debug::nativePrintArgs("unable to allocate anim_rig");
    return nullptr;
}

// 0x00C8ADF0
int anim_rig_find_index(const char* filename)
{
    for (int i = 0; i < 290; i++) {
        if ((Anim_rig[i].flags & 1) != 0 && v_stricmp(Anim_rig[i].name, filename) == 0) {
            return i;
        }
    }
    return -1;
}

// 0x00C8AE30
bool anim_rig_test_existence(const char* filename)
{
    return anim_rig_find_index(filename) > -1;
}
