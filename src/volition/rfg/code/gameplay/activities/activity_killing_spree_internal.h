// Original: volition/rfg/code/gameplay/activities/activity_killing_spree_internal.h
// Implementation: volition/rfg/code/gameplay/activities/activity_killing_spree.cpp
//
// The killing spree activity (a free activity): a timed kill tally with its HUD; activity_ks_data is one tallied kill.
// Placement of activity_ks_data inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/vlib/code/time/timestamp.h"

class human;
class object;

// size 0x8
struct activity_ks_data
{
    activity_ks_data(); // 0x00754490

    timestamp expiration_time; // 0x00
    int kill_count; // 0x04
};

// size 0x450
class activity_killing_spree_data : public activity_base_free
{
public:
    activity_killing_spree_data(); // 0x0077E6E0 (.h:20)
    virtual void level_init(); // vtbl+0x24 0x0076E8A0 (activity_killing_spree.cpp:72)
    virtual void activity_end(); // vtbl+0x3C 0x007ADA20 (activity_killing_spree.cpp:174)
    virtual void activity_kill(); // vtbl+0x40 0x00798990 (activity_killing_spree.cpp:198)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x00758D10 (activity_killing_spree.cpp:144)
    virtual bool render_hud(int x_pos, int y_pos); // vtbl+0x70 0x00758D00 (activity_killing_spree.cpp:116)
    virtual const char* get_title() const; // vtbl+0x80 0x00758C90 (activity_killing_spree.cpp:86)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x00758CA0 (activity_killing_spree.cpp:99)
    virtual bool get_ui_display_info(char* value_text, int& time_til_end, int buffer_size) const; // vtbl+0xCC 0x00758CB0 (activity_killing_spree.cpp:105)
    virtual void register_death(const object* target_obj, const object* attacker_obj); // vtbl+0xBC 0x007ADAD0 (activity_killing_spree.cpp:220)
    void reset(); // 0x00758C70 (activity_killing_spree.cpp:79)
    void start_killing_spree(); // 0x007AD9C0 (activity_killing_spree.cpp:155)
    void record_kill(const human* hp); // 0x00758D60 (activity_killing_spree.cpp:287)
    void add_kill_to_tally(const human* hp); // 0x00758D40 (activity_killing_spree.cpp:210)
    virtual ~activity_killing_spree_data(); // vtbl+0x00 0x00769110

private:
    activity_ks_data activity_data; // 0x448
};

extern activity_killing_spree_data Killing_spree_data; // 0x016532B0
