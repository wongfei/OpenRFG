// Original: volition/rfg/code/multi/multi_core/generic/multi_voice_generic.cpp
//
// The PC platform layer of voice chat: everything goes through GameLink (on the internet link, Steam's voice), under
// g_gameLinkMutex. The headset status, the mute state and the talking state come from GameLink's player info; a remote
// talker needs no registration of its own. While push to talk is on, the local microphone is muted unless the button
// is held.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_voice_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/vlib/code/os/os.h"
#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/mutex.hpp"
#include "unit4/gamesession/remotegamesession.hpp"

// ---- file-static globals
static bool Push_to_talk_enabled = true; // 0x0167D833
static bool Multi_voice_generic_inited; // 0x02FEB572
static bool Multi_voice_enabled_local_talker; // 0x02FEB573
// The talkers are multi_voice.cpp's; placed here, their first user.
static int Num_registered_talkers; // 0x02FE6C78
static voice_talker Registered_talkers[30]; // 0x02FEBCA0

// 0x00942A50 (:41)
bool multi_voice_platform_init()
{
    if (!Multi_voice_generic_inited)
    {
        Multi_voice_generic_inited = true;
    }
    return true;
}

// 0x00942A70 (:52)
void multi_voice_platform_shutdown()
{
    if (Multi_voice_generic_inited)
    {
        Multi_voice_generic_inited = false;
    }
}

// 0x00942A90 (:61)
bool multi_voice_platform_engine_valid()
{
    g_gameLinkMutex.lock(0);
    if (Remote_game_session.getGameLinkSession() != nullptr && Multi_voice_generic_inited)
    {
        g_gameLinkMutex.unlock();
        return true;
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x00942AD0 (:72)
void multi_voice_platform_update_my_headset_status()
{
    if (!multi_voice_platform_engine_valid())
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    kaiko::GameLinkVoiceChatPlayerInfo info;
    if (Remote_game_session.getGameLinkSession()->getVoiceChatLocalPlayerInfo(&info))
    {
        Local_player_voice_info.has_headset = info.hasHeadset;
    }
    g_gameLinkMutex.unlock();
}

// 0x00942B50 (:90)
void multi_voice_platform_register_local_talker()
{
    if (!multi_voice_platform_engine_valid())
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    Multi_voice_enabled_local_talker = Remote_game_session.setVoiceChatEnabled(true);
    Local_player_voice_info.registered_for_voice = true;
    g_gameLinkMutex.unlock();
}

// 0x00942BC0 (:105)
void multi_voice_platform_unregister_local_talker()
{
    if (!multi_voice_platform_engine_valid())
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    Remote_game_session.setVoiceChatEnabled(false);
    Multi_voice_enabled_local_talker = false;
    Local_player_voice_info.registered_for_voice = false;
    g_gameLinkMutex.unlock();
}

// 0x00971470 (:122)
bool multi_voice_platform_talker_is_registered(const multi_player_uid& talker_uid)
{
    for (int i = 0; i < Num_registered_talkers; i++)
    {
        if (Registered_talkers[i].player_uid.is_equal(talker_uid))
        {
            return true;
        }
    }
    return false;
}

// 0x00942C30 (:134): empty in rfg.exe
void multi_voice_platform_register_remote_talker(multi_player_uid talker_uid)
{
}

// 0x00942C40 (:140): empty in rfg.exe
void multi_voice_platform_remove_remote_talker(multi_player_uid old_talker)
{
}

// 0x009714C0 (:149): a player without voice info counts as muted
bool multi_voice_platform_player_is_muted(const multi_player_uid& uid)
{
    if (!multi_voice_platform_engine_valid())
    {
        return true;
    }
    g_gameLinkMutex.lock(0);
    kaiko::GameLinkVoiceChatPlayerInfo info = {};
    bool found = false;
    if (uid.is_equal(My_player_uid))
    {
        found = Remote_game_session.getGameLinkSession()->getVoiceChatLocalPlayerInfo(&info);
    }
    else
    {
        const kaiko::GameLinkRemoteStation* station = Remote_game_session.getGameLinkSession()->getRemoteStationById(uid.get_internal_id());
        if (station != nullptr)
        {
            found = Remote_game_session.getGameLinkSession()->getVoiceChatRemotePlayerInfo(&info, *station);
        }
    }
    if (found)
    {
        g_gameLinkMutex.unlock();
        return info.isMuted;
    }
    g_gameLinkMutex.unlock();
    return true;
}

// 0x00942C50 (:186)
void multi_voice_platform_mute_player(const multi_player_uid& uid)
{
    if (!multi_voice_platform_engine_valid())
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    const kaiko::GameLinkRemoteStation* station = Remote_game_session.getGameLinkSession()->getRemoteStationById(uid.get_internal_id());
    if (station != nullptr)
    {
        Remote_game_session.getGameLinkSession()->setVoiceChatRemotePlayerMuted(*station, true);
    }
    g_gameLinkMutex.unlock();
}

// 0x00942CF0 (:205)
void multi_voice_platform_unmute_player(const multi_player_uid& uid)
{
    if (!multi_voice_platform_engine_valid())
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    const kaiko::GameLinkRemoteStation* station = Remote_game_session.getGameLinkSession()->getRemoteStationById(uid.get_internal_id());
    if (station != nullptr)
    {
        Remote_game_session.getGameLinkSession()->setVoiceChatRemotePlayerMuted(*station, false);
    }
    g_gameLinkMutex.unlock();
}

// 0x00942D90 (:224). Original bug kept: the result of getVoiceChatRemotePlayerInfo is not checked, so a failed query
// reads the uninitialized info.
bool multi_voice_platform_remote_player_is_talking(const multi_player_uid& uid)
{
    if (!multi_voice_platform_engine_valid())
    {
        return false;
    }
    g_gameLinkMutex.lock(0);
    const kaiko::GameLinkRemoteStation* station = Remote_game_session.getGameLinkSession()->getRemoteStationById(uid.get_internal_id());
    if (station != nullptr)
    {
        kaiko::GameLinkVoiceChatPlayerInfo info;
        Remote_game_session.getGameLinkSession()->getVoiceChatRemotePlayerInfo(&info, *station);
        g_gameLinkMutex.unlock();
        return info.hasJustSpoken;
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x009715B0 (:248). Original bug kept: the result of getVoiceChatLocalPlayerInfo is not checked (as above).
bool multi_voice_platform_local_player_is_talking(const multi_player_uid& uid)
{
    if (!multi_voice_platform_engine_valid() || !My_player_uid.is_equal(uid))
    {
        return false;
    }
    g_gameLinkMutex.lock(0);
    kaiko::GameLinkVoiceChatPlayerInfo info;
    Remote_game_session.getGameLinkSession()->getVoiceChatLocalPlayerInfo(&info);
    g_gameLinkMutex.unlock();
    return info.hasJustSpoken;
}

// 0x00942E50 (:265)
bool multi_voice_platform_chat_allowed(bool systemlink_or_lan)
{
    g_gameLinkMutex.lock(0);
    if (!systemlink_or_lan && !Remote_game_session.hasVoiceChatPermissions())
    {
        g_gameLinkMutex.unlock();
        return false;
    }
    g_gameLinkMutex.unlock();
    return true;
}

// 0x00971640 (:273): retries enabling voice chat until it holds; push to talk mutes the microphone unless the button
// is down (skipped while the window is minimized, which keeps the last state)
void multi_voice_platform_do_frame()
{
    if (!Multi_voice_enabled_local_talker && multi_voice_platform_engine_valid())
    {
        g_gameLinkMutex.lock(0);
        Multi_voice_enabled_local_talker = Remote_game_session.setVoiceChatEnabled(true);
        g_gameLinkMutex.unlock();
    }
    const bool minimized = os_minimized();
    if (Push_to_talk_enabled && !minimized)
    {
        const bool talking = controls_is_button_down(CBA_PUSHTOTALK, Player_controller_index);
        g_gameLinkMutex.lock(0);
        Remote_game_session.getGameLinkSession()->setVoiceChatLocalPlayerMuted(!talking);
        g_gameLinkMutex.unlock();
    }
}
