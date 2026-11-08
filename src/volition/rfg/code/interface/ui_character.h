// Original: volition/rfg/code/interface/ui_character.h
// Implementation: volition/rfg/code/interface/ui_character.cpp
//
// The 3D character preview of the menus (multiplayer character select, main menu): the model, its static meshes and
// the frame buffer it renders into.
#pragma once


class character;
class character_instance;
class cloth_simulation;
class human_info;
struct item_info;
class rl_mesh_instance;
struct static_mesh;
class vector;

// size 0x60
struct model_info
{
    void clear(); // 0x00893180 (:109)
    model_info(); // 0x008931B0 (:121)

    char model_name[64]; // 0x00
    unsigned int srid; // 0x40
    character_instance* char_inst; // 0x44
    cloth_simulation* cloth_sim; // 0x48
    const human_info* h_info; // 0x4C
    const item_info* i_info; // 0x50
    int attach_bone; // 0x54
    int parent_attach_bone; // 0x58
    bool requested; // 0x5C
    bool loaded; // 0x5D
};

// size 0x5C
struct static_mesh_info
{
    void clear(); // 0x008931E0 (:137)
    static_mesh_info(); // 0x00893210 (:148)

    char model_name[64]; // 0x00
    unsigned int srid; // 0x40
    static_mesh* smesh; // 0x44
    rl_mesh_instance* smesh_instance; // 0x48
    const item_info* i_info; // 0x4C
    int attach_bone; // 0x50
    int parent_attach_bone; // 0x54
    bool requested; // 0x58
    bool loaded; // 0x59
};

rl_mesh_instance* ui_create_static_mesh(static_mesh* base_mesh); // 0x008C48E0 (:343)
character_instance* ui_create_character_instance(character* base_character, const char* anim_set_name, unsigned char skin_category); // 0x00893450 (:360)
void ui_character_release_effect(); // 0x008937A0 (:919)
void ui_character_create_effect(); // 0x008937C0 (:959)
void ui_character_get_effect_bbox(vector& bmin, vector& bmax); // 0x008939A0 (:992)
void ui_character_update_effect(float d_time); // 0x00893B80 (:1024)
void ui_character_update_effect_render(); // 0x00893EE0 (:1075)
void ui_character_play_effect(); // 0x00893F10 (:1087)
void ui_character_init(int dhandle); // 0x008D71F0 (:1194)
void ui_character_shutdown(); // 0x008B0CD0 (:1211)
void ui_character_process(float dtime); // 0x00893F90 (:1222)
void ui_character_process_internal(); // 0x008C4B10 (:1248)
void ui_character_handshake(); // 0x008C4ED0 (:1340)
bool ui_character_is_loaded(); // 0x00894030 (:1356)
void ui_character_set_visible(bool visible); // 0x00894050 (:1370)
void ui_character_set_frame_visible(bool visible); // 0x008D7220 (:1381)
void ui_character_trigger(bool select, int menu_option); // 0x008D7310 (:1402)
