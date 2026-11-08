// Original: volition/rfg/code/destruction/rfg_destruction.cpp
//
// Frame hooks of the destruction system (decompiled; header comment in rfg_destruction.h). Compiled in
// rfg_destruction_unity.cpp.
//
// Partial: rfg_message_process_and_render does not draw (the message fades out over its last second, yellow when
// important, grey otherwise, at 32,64 in the header font).

#include "volition/rfg/code/destruction/rfg_destruction.h"

#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_load_balance.h"
#include "volition/vlib/code/time/timestamp.h"

#include <string.h>

// ---- file-static globals
static timestamp Rfg_message_timestamp; // 0x016504B0
static timestamp RFG_load_delete_stamp; // 0x016504B4, also rfg_load_balance.cpp (later in the unity)
static bool Rfg_message_important; // 0x01E38320
static char Rfg_message[200]; // 0x01E38340
static int RFG_load_rm_index; // 0x01EF82A0, rfg_load_balance.cpp
static int RFG_load_gm_index; // 0x01EF82A4, rfg_load_balance.cpp
static int RFG_load_od_index; // 0x01EF82A8, rfg_load_balance.cpp

// 0x00732490 (:383)
void rfg_level_init_pre()
{
    rfg_dyn_level_init_pre();
    rfgl_level_init_pre(); // inlined
    rfg_message_init_pre(); // inlined
    rfg_effect_candidate_level_init_pre();
}

// 0x0071B8B0 (:403)
void rfg_level_shutdown()
{
}

// 0x0073CDE0 (:416)
void rfg_do_frame_pre()
{
    rfg_dyn_do_frame_pre(); // inlined
}

// 0x00737CC0 (:435)
void rfg_do_frame_post()
{
    rfg_dyn_do_frame_post(); // inlined: destruction trails and shape cutters
    rfg_effect_candidate_post_frame();
    rfgl_do_frame();
}

// 0x007114D0 (:1921)
void rfg_message_init_pre()
{
    Rfg_message[0] = '\0';
}

// 0x007114E0 (:1926)
// Shown for 2 s. Kept: copied without a length check.
void rfg_message_add(char* m, bool important)
{
    strcpy(Rfg_message, m);
    Rfg_message_important = important;
    Rfg_message_timestamp.set(2000);
}

// 0x00711520 (:1933)
void rfg_message_process_and_render()
{
    if (Rfg_message[0] == '\0')
    {
        return;
    }
    if (Rfg_message_timestamp.time_until() < 1)
    {
        Rfg_message[0] = '\0';
        return;
    }
    // Partial: drawing (see the file comment)
}
