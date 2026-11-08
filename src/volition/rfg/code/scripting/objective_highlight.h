// Original: volition/rfg/code/scripting/objective_highlight.h
// Implementation: volition/rfg/code/scripting/objective_highlight.cpp
//
// Objective highlights: objects scripts or activities mark (arrows, cylinders, frames, glow outlines; by purpose: destroy,
// EDF, neutral, activity), ground and activity markers, kept in Highlight_objects_table and drawn with the effect and
// outline systems. Placement of the enums and highlight_data inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/hash_table.h"

class object;

enum highlight_purpose
{
    HIGHLIGHT_PURPOSE_DESTROY = 0,
    HIGHLIGHT_PURPOSE_NEUTRAL = 1,
    HIGHLIGHT_PURPOSE_EDF = 2,
    HIGHLIGHT_PURPOSE_ACTIVITY = 3,
    NUM_HIGHLIGHT_PURPOSES = 4,
};

enum highlight_style
{
    HIGHLIGHT_STYLE_DEFAULT = -1,
    HIGHLIGHT_STYLE_NONE = 0,
    HIGHLIGHT_STYLE_GLOW = 1,
    HIGHLIGHT_STYLE_FRAME = 2,
    HIGHLIGHT_STYLE_ARROW = 3,
    HIGHLIGHT_STYLE_ARROW_FOR_PASSENGER = 4,
    HIGHLIGHT_STYLE_CYLINDER = 5,
    NUM_HIGHLIGHT_STYLES = 6,
};

// size 0xC
struct highlight_data
{
    unsigned int effect_obj_handle; // 0x00
    highlight_purpose color_idx; // 0x04
    highlight_style style; // 0x08
};

extern vector Highlight_colors[4]; // 0x016B2660
extern data_hash_table<unsigned int,highlight_data,&uint_hash,64,unsigned char> Highlight_objects_table; // 0x016B2690

unsigned int objective_highlight_activity_marker_add(const object* host_objp, const vector& effect_offset); // 0x00B2E120 (:1130)
unsigned int objective_highlight_activity_marker_add(unsigned int host, const vector& effect_offset); // 0x00B2E1C0 (:1151)
unsigned int objective_highlight_ground_marker_add(unsigned int host, float diameter, const vector& offset, bool use_action_node, bool stick_to_ground); // 0x00B2E1E0 (:1167)
void unused_dcf_objective_highlight_ground_marker_kill(); // 0x00B3E540 (:1269)
void objective_highlight_ground_marker_remove(unsigned int host); // 0x00B2E360 (:1304)
void objective_highlight_do_frame(); // 0x00B3E550 (:1310)
void objective_highlight_do_frame_handshake(); // 0x00B33D30 (:1462)
void objective_highlight_level_init(); // 0x00B2E370 (:1584)
void objective_highlight_system_init(); // 0x00B239B0 (:1593)
void objective_highlight_add(unsigned int handle, highlight_purpose color_hint, highlight_style style); // 0x00B3A790 (:1605)
void objective_highlight_remove(unsigned int handle); // 0x00B3A840 (:1639)
void objective_highlight_remove_all(); // 0x00B3A8B0 (:1660)
