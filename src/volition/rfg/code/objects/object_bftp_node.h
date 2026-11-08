// Original: volition/rfg/code/objects/object_bftp_node.h
// Implementation: volition/rfg/code/objects/object_bftp_node.cpp
//
// Blast from the past nodes: tag nodes the player touches to collect tags, and the car bomb nodes of the same
// activity (paired by number), with their effects and state flags.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

struct cfile;
class human;

// size 0xB0
class object_bftp_node : public object
{
public:
    // size 0x1
    struct bftp_state_flags
    {
        bool collected : 1; // 0x00:0
        bool enabled_bomb : 1; // 0x00:1
    };

    object_bftp_node(); // 0x00A15BB0 (:227)
    static const object_type get_type_static(); // 0x0084C2E0 (.h:23)
    static const object_sub_type get_subtype_static(); // 0x0084C2F0 (.h:23)
    static const char* get_classname_static(); // 0x00A10050 (.h:23)
    static int get_bpo_index_static(); // 0x00A10060 (.h:23)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A15BD0 (.h:23)
    static unsigned int get_classname_hash(); // 0x00A15BA0 (:226)
    static int pool_remaining(); // 0x00A39590 (:226)
    virtual void pool_free(); // vtbl+0x80 0x00A8DD50 (:226)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA4A40 (:231)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A15BF0 (:287)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A611E0 (:293)
    bool is_collected(); // 0x00A15C00 (:329)
    void collect_item(); // 0x00A15C10 (:335)
    void spawn_bomb_from_tag(); // 0x00A61300 (:364)
    void load(cfile*);
    void save(cfile*);
    const bool is_tag_node() const; // 0x0084C300 (.h:48)
    const int get_pair_number() const; // 0x00A10070 (.h:49)
    const unsigned int get_item_handle() const; // 0x00A10080 (.h:50)
    const bool is_bomb_enabled() const; // 0x00A15C30 (:379)
    void restore_bomb(); // 0x00A61340 (:388)
private:
    void spawn_bomb_item(); // 0x00A612A0 (:344)
public:
    virtual ~object_bftp_node(); // vtbl+0x00 0x00A15BE0

    unsigned int bftp_effect; // 0x9C
private:
    bool tag_node; // 0xA0
    int pair_number; // 0xA4
    unsigned int bftp_item_handle; // 0xA8
    bftp_state_flags bftp_flags; // 0xAC
};

object* alloc_object_bftp_node(); // 0x00A611D0 (:226)
const char* classname_object_bftp_node(); // 0x00A15B90 (:226)
void types_object_bftp_node(signed char& type, signed char& subtype); // 0x00A15B70 (:226)
int bpo_index_object_bftp_node(); // 0x00A15B80 (:226)
bool blast_from_the_past_touch_item(human* hp, unsigned int item_handle); // 0x00A61350 (:406)
int blast_from_the_past_tags_collected(); // 0x00A61510 (:464)
void blast_from_the_past_process_car_bombs(); // 0x00AA4C60 (:480)
void blast_from_the_past_reset_last_bomb(); // 0x00A61570 (:544)
void blast_from_the_past_bomb_destroyed(); // 0x00A15C50 (:567)
