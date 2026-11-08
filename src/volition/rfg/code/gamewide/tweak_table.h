// Original: volition/rfg/code/gamewide/tweak_table.h
// Implementation: volition/rfg/code/gamewide/tweak_table.cpp
//
// Tweak table: named bool/int/float variables that tweak_table.xtbl (or dlcNN_tweak_table.xtbl) and
// online_tweak_table.xtbl override at load time (<Tweak_Table_Entry> with <Name> and <Value>; decompiled). Items
// register themselves in The_tweak_table (a function-local static farray<tweak_table_item*,350> of get_tweak_table).
#pragma once

struct xml_element;

// size 0xC
class tweak_table_item
{
public:
    enum tweak_value_type
    {
        TWEAK_VALUE_TYPE_BOOL = 0,
        TWEAK_VALUE_TYPE_FLOAT = 1,
        TWEAK_VALUE_TYPE_INT = 2,
    };

    tweak_table_item(const char* in_name, float* in_value); // 0x007EF100 (:188)
    tweak_table_item(const char* in_name, int* in_value); // 0x007EF0E0 (:183)
    tweak_table_item(const char* in_name, bool* in_value); // 0x007EF0C0 (:178)
    void update_value(float new_value) const; // 0x007C9220 (:232)
    float get_value_float() const; // 0x007C91D0 (:211)
    const char* get_name() const; // 0x007B9EC0 (.h:52)
    tweak_value_type get_type() const; // 0x007B9ED0 (.h:53)
    void get_string(char* buffer) const; // 0x007C9290 (:254)
private:
    void init(const char* in_name, void* in_value, tweak_value_type in_type); // 0x007EB460 (:194)

    const char* name; // 0x00
    void* value; // 0x04
    tweak_value_type type; // 0x08
};

void unused_dcf_reload_tweak_table(); // 0x007EF090 (:87)
void unused_dcf_tweak_table(); // 0x007EB1E0 (:96)
void tweak_table_init(); // 0x007EF120 (:325)
void tweak_parse_xml(xml_element* root); // 0x007EB5A0 (:330)
void tweak_table_load(int dlc_bundle_id); // 0x007EB5B0 (:336)
void multi_game_online_tweaks_init(); // 0x007EB660 (:389)
