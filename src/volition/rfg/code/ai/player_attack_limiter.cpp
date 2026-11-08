// Original: volition/rfg/code/ai/player_attack_limiter.cpp
//
// Limits how many npcs attack the player at once (stub_impl.md S9). Linked by OpenRFG only.

#include "volition/rfg/code/ai/player_attack_limiter.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"

#include <string.h>

// size 0x10 (rfg.pdb)
struct player_attack_limiter_record
{
    // size 0x1, bit positions inferred
    struct player_attack_limiter_record_flags
    {
        bool processed_this_frame : 1;
        bool previously_on_list : 1;
    };

    unsigned int npc_handle; // 0x00
    float score; // 0x04
    timestamp valid_timer; // 0x08
    player_attack_limiter_record_flags flags; // 0x0C
};

static const int MAX_PLAYER_ATTACK_LIMITER_RECORDS = 32; // the table's size; check_frame's stack copy has 0x200 bytes
static const int RECORD_VALID_MS = 5000; // a request keeps its record alive this long
static const float WAS_ON_LIST_BONUS = 6.0f; // metres, so the list does not flip between npcs at similar distances
static const float RECENT_CONTACT_BONUS = 10.0f; // metres, for an npc with recent contact with the player
static const int RECENT_CONTACT_MS = 10000;

int Max_npcs_attacking_player_by_alert_level[4]; // 0x01D67730
static tweak_table_item Tweak_table_item_max_npcs_attacking_player_green("max_npcs_attacking_player_green", &Max_npcs_attacking_player_by_alert_level[0]); // (0x01066F40)
static tweak_table_item Tweak_table_item_max_npcs_attacking_player_yellow("max_npcs_attacking_player_yellow", &Max_npcs_attacking_player_by_alert_level[1]); // (0x01066F60)
static tweak_table_item Tweak_table_item_max_npcs_attacking_player_orange("max_npcs_attacking_player_orange", &Max_npcs_attacking_player_by_alert_level[2]); // (0x01066F80)
static tweak_table_item Tweak_table_item_max_npcs_attacking_player_red("max_npcs_attacking_player_red", &Max_npcs_attacking_player_by_alert_level[3]); // (0x01066FA0)

int Num_player_attack_limiter_records; // 0x01D6771C
static player_attack_limiter_record Player_attack_limiter_records[MAX_PLAYER_ATTACK_LIMITER_RECORDS];
static int Last_request_frame; // 0x01D6520C
static gfm_alert_levels Last_alert_level; // 0x01D65210
static int Last_npc_limit; // 0x01D6572C

// 0x005F0500 (:126)
static void swap_records(int i, int j)
{
    const player_attack_limiter_record temp = Player_attack_limiter_records[i];
    Player_attack_limiter_records[i] = Player_attack_limiter_records[j];
    Player_attack_limiter_records[j] = temp;
}

// 0x005F0550 (:135)
static void check_frame()
{
    if (Framecount == Last_request_frame)
    {
        return;
    }
    if (Last_npc_limit < Num_player_attack_limiter_records)
    {
        Num_player_attack_limiter_records = Last_npc_limit;
    }

    // Keep the records still valid, in order, for this frame's requests to rescore.
    player_attack_limiter_record kept[MAX_PLAYER_ATTACK_LIMITER_RECORDS];
    int num_kept = 0;
    for (int i = 0; i < Num_player_attack_limiter_records; ++i)
    {
        player_attack_limiter_record& record = Player_attack_limiter_records[i];
        if (!record.valid_timer.elapsed())
        {
            record.flags.previously_on_list = true;
            record.flags.processed_this_frame = false;
            kept[num_kept] = record;
            ++num_kept;
        }
    }
    Num_player_attack_limiter_records = num_kept;
    if (num_kept > 0)
    {
        memcpy(Player_attack_limiter_records, kept, num_kept * sizeof(player_attack_limiter_record));
    }
    Last_alert_level = gsm_get_alert_level();
    Last_npc_limit = Max_npcs_attacking_player_by_alert_level[Last_alert_level];
    Last_request_frame = Framecount;
}

// 0x005F0650 (:181)
int find_record(unsigned int npc_handle)
{
    for (int i = 0; i < Num_player_attack_limiter_records; ++i)
    {
        if (npc_handle == Player_attack_limiter_records[i].npc_handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x005F0680 (:198)
float calculate_score(const npc* np, bool was_on_list)
{
    const ai_system* ai = np->get_ai();
    player* pp = player_get_local_player();
    float score = vec_dist(pp->pos, np->pos);
    if (was_on_list)
    {
        score -= WAS_ON_LIST_BONUS;
    }
    const ai_belief* last_contact = ai->perception.get_belief(AI_BELIEF_LAST_CONTACT, pp->get_handle());
    if (last_contact != nullptr && last_contact->update_time.time_since() < RECENT_CONTACT_MS)
    {
        score -= RECENT_CONTACT_BONUS;
    }
    return score;
}

// 0x005F0730 (:235)
void player_attack_limiter_level_init_pre()
{
    Last_request_frame = -1;
    Last_alert_level = ALERT_LEVEL_GREEN;
    Last_npc_limit = Max_npcs_attacking_player_by_alert_level[0];
    Num_player_attack_limiter_records = 0;
}

// 0x006402F0 (:252)
bool player_attack_limiter_request(const npc* np)
{
    check_frame();
    const unsigned int handle = np->get_handle();
    int index = find_record(handle);
    player_attack_limiter_record* record;
    if (index >= 0)
    {
        record = &Player_attack_limiter_records[index];
        if (record->flags.processed_this_frame)
        {
            return index < Last_npc_limit;
        }
    }
    else
    {
        if (Num_player_attack_limiter_records == MAX_PLAYER_ATTACK_LIMITER_RECORDS)
        {
            return false;
        }
        index = Num_player_attack_limiter_records;
        Player_attack_limiter_records[index].flags.previously_on_list = false;
        record = &Player_attack_limiter_records[index];
        ++Num_player_attack_limiter_records;
        record->npc_handle = handle;
    }
    record->score = calculate_score(np, record->flags.previously_on_list);
    record->flags.processed_this_frame = true;
    record->valid_timer.set(RECORD_VALID_MS);

    // Find the first record on the list scoring worse than this one (lower scores win).
    const int limit = Last_npc_limit;
    const int num_on_list = Num_player_attack_limiter_records < limit ? Num_player_attack_limiter_records : limit;
    int place = 0;
    while (place < num_on_list && !(place != index && record->score < Player_attack_limiter_records[place].score))
    {
        ++place;
    }

    if (place < limit)
    {
        int current = index;
        if (limit <= current)
        {
            current = limit - 1;
            swap_records(index, current);
        }
        for (; current > place; --current)
        {
            swap_records(current, current - 1);
        }
        for (; current + 1 < place; ++current)
        {
            swap_records(current, current + 1);
        }
        return true;
    }

    if (index < limit)
    {
        // It was on the list but now scores worst: the best npc off the list takes the last place, if any is better.
        const player_attack_limiter_record saved = *record;
        const int last = limit - 1;
        if (index < last)
        {
            memmove(&Player_attack_limiter_records[index], &Player_attack_limiter_records[index + 1], (last - index) * sizeof(player_attack_limiter_record));
        }
        int best = last;
        float best_score = saved.score;
        for (int i = limit; i < Num_player_attack_limiter_records; ++i)
        {
            if (Player_attack_limiter_records[i].score < best_score)
            {
                best = i;
                best_score = Player_attack_limiter_records[i].score;
            }
        }
        if (best == last)
        {
            Player_attack_limiter_records[last] = saved;
            return true;
        }
        Player_attack_limiter_records[last] = Player_attack_limiter_records[best];
        Player_attack_limiter_records[best] = saved;
    }
    return false;
}

// 0x005F0760 (:386)
bool player_attack_limiter_check(const npc* np)
{
    // Original bug kept: the loop runs to Last_npc_limit, so it also reads records past
    // Num_player_attack_limiter_records, which can hold a handle from before they were dropped.
    for (int i = 0; i < Last_npc_limit; ++i)
    {
        if (Player_attack_limiter_records[i].npc_handle == np->get_handle())
        {
            return true;
        }
    }
    return false;
}
