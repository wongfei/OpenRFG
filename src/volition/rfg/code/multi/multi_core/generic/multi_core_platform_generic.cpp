// Original: volition/rfg/code/multi/multi_core/generic/multi_core_platform_generic.cpp
//
// The PC platform behind multi_core_platform.h: the keen remote game session (Remote_game_session, a LAN and an
// internet GameLink) guarded by g_gameLinkMutex, its interactions shown as message boxes, packets to and from GameLink
// stations (a connection_address is a station id), the local player uid, Steam profiles, achievements and the
// leaderboards (keen system services). Most of the console-style platform calls do nothing on PC.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/option_menus/option_menu_system.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/frametime.h"
#include "game/build/win32/steam/master/achievements/rfgachievements_adf.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"
#include "keen/gameframework/gameframework.hpp"
#include "library/modules/runtime/gamelink/sources/global/lan/gamelinklan.hpp"
#include "library/modules/runtime/network/sources/global/networkplatform.hpp"
#include "library/modules/runtime/systemservices/sources/steam/systemservices_steam.hpp"
#include "library/modules/thirdparty/sw_api/sw_api.h"
#include "unit4/gamesession/localgamesession.hpp"
#include "unit4/gamesession/remotegamesession.hpp"

#include <stdlib.h>
#include <string.h>

// ---- globals (see multi_core_platform_internal.h)
unit4::RemoteGameSession Remote_game_session; // 0x0167F500
// keen::Mutex g_gameLinkMutex (0x02FEB77C) is defined in multi.cpp (its dynamic initializer is multi.cpp:407).
kaiko::GameLinkConnectionType g_gameLinkConnectionType; // 0x02FEB608
bool Multi_core_platform_last_session_start_succeeded = true; // 0x0167D7C9
float current_voicechat_volume = -1.0f; // 0x0167D7F4, not a volume: the first process applies the option
int Remote_game_session_msgbox_handle = -1; // 0x0167D7E8, no message box
unsigned int Remote_game_session_msgbox_selection = 0xFFFFFFFFu; // 0x0167D7EC, no answer
unit4::SessionInteractionData Remote_game_session_last_interaction; // 0x01691500
platform_op_status Scoreboard_read_progress = POS_FAILED; // 0x0167D7F0
scoreboard_results* Last_scoreboard_results; // 0x02FEB578
unit4::RankingResult Current_scoreboard_ranking_results; // 0x02FE5BDC
unsigned int Max_num_ranking_results; // 0x02FEB574
scoreboard_column_info Last_scoreboard_column_infos[4]; // 0x02FEB398
bool Scoreboard_read_op_active; // 0x02FEB571

// ---- file-static globals
static pending_invite_info Pending_invite; // 0x02FEBBD8

// Magic number: the "no local player" id of the keen game sessions.
static const keen::LocalPlayerIdStructureType* const No_local_player = (const keen::LocalPlayerIdStructureType*)0xFF;

// 0x00941E60 (:106)
unit4::RemoteGameSession* get_remote_game_session()
{
    return &Remote_game_session;
}

// 0x00941E70 (:111)
kaiko::gamelink::GameLink* get_game_link_session()
{
    return Remote_game_session.getGameLinkSession();
}

// 0x00941E80 (:123): both links are created; up to 16 lobby and party members, 25 search results, 512 queued packets
void multi_core_platform_internal_system_init()
{
    unit4::RemoteGameSessionCreationParameters params;
    params.pLocalGameSession = rfg_get_local_game_session();
    params.gameLinkGeneralParameters.maxNumMatchmakingResults = 25;
    params.gameLinkGeneralParameters.maxNumLobbyMembers = 16;
    params.gameLinkGeneralParameters.maxNumPartyMembers = 16;
    params.gameLinkGeneralParameters.pSoundSystem = Rfg_soundSystem;
    params.gameLinkGeneralParameters.pSystemServices = params.pLocalGameSession->getSystemServices();
    kaiko::GameLinkLanCreationParameters lanParams;
    lanParams.maxNumStoredUserDefinedPackets = 512;
    keen::copyString(lanParams.localUserName, sizeof(lanParams.localUserName), keen::GameFramework::getLocalUserName(Rfg_gameframework_system));
    kaiko::GameLinkInternetCreationParameters internetParams;
    kaiko::GameLinkWirelessCreationParameters wirelessParams;
    params.pGameLinkWirelessParameters = &wirelessParams;
    params.pGameLinkLanParameters = &lanParams;
    params.pGameLinkInternetParameters = &internetParams;
    internetParams.vacEnabled = true;
    g_gameLinkMutex.lock(0);
    Remote_game_session.create(rfg_get_cpu_scratch_allocator(), params);
    Remote_game_session.setVoiceChatVolume(get_voicechat_volume());
    g_gameLinkMutex.unlock();
}

// 0x00941F80 (:169)
void multi_core_platform_internal_system_shutdown()
{
    g_gameLinkMutex.lock(0);
    Remote_game_session.destroy(rfg_get_cpu_scratch_allocator());
    g_gameLinkMutex.unlock();
}

// 0x009706F0 (:176): the message box of a session interaction answers with the chosen response (abort: the last one)
bool handleRemoteGameSessionMsgboxResponse(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Remote_game_session_msgbox_handle != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        Remote_game_session_msgbox_selection = selection_index;
        return true;
    }
    if (choice == MBC_ABORT)
    {
        Remote_game_session_msgbox_selection = Remote_game_session_last_interaction.responseOptionCount - 1;
        return true;
    }
    if (choice == MBC_FORCED_EXIT)
    {
        memset(&Remote_game_session_last_interaction, 0, sizeof(Remote_game_session_last_interaction));
        Remote_game_session_msgbox_selection = 0xFFFFFFFFu;
        return true;
    }
    return false;
}

// 0x00970750 (:205): the voice chat volume option, the remote session's update (the GameLink: sockets, lobby,
// keep-alives), and its interaction as a message box; then the profile, and the login callback after a failed start
void multi_core_platform_internal_system_process()
{
    if (current_voicechat_volume != get_voicechat_volume())
    {
        current_voicechat_volume = get_voicechat_volume();
        g_gameLinkMutex.lock(0);
        Remote_game_session.setVoiceChatVolume(current_voicechat_volume);
        g_gameLinkMutex.unlock();
    }
    g_gameLinkMutex.lock(0);
    Remote_game_session.update(Frametime_real);
    g_gameLinkMutex.unlock();
    unit4::SessionInteractionData interaction;
    memset(&interaction, 0, sizeof(interaction));
    g_gameLinkMutex.lock(0);
    const bool required = Remote_game_session.isInteractionRequired(&interaction);
    g_gameLinkMutex.unlock();
    if (required)
    {
        if ((interaction.id != Remote_game_session_last_interaction.id || (Remote_game_session_last_interaction.id == 0 && !ui_is_msgbox_active(-2))) &&
            rfg_get_error_viewer_response() == 0 && rfg_get_currently_handled_error_viewer_event_id() == 0)
        {
            if (Remote_game_session_msgbox_handle != -1)
            {
                ui_hide_msgbox(Remote_game_session_msgbox_handle, false);
                Remote_game_session_msgbox_handle = -1;
                keen::debug::nativePrintArgs("Remote game session interaction changed, hiding original interaction\n");
            }
            Remote_game_session_msgbox_selection = 0xFFFFFFFFu;
            for (unsigned int i = 0; i < interaction.params.parameterCount; i++)
            {
                rfg_process_game_session_interaction_param(interaction.params.parameters[i]);
            }
            // magic number: ids from 1000000 on are error viewer events
            const bool errorViewer = interaction.id > 999999;
            kaiko::GameSessionInteractionLocaTag locaTag;
            kaiko::generateGameSessionInteractionLocaTag(locaTag, errorViewer ? kaiko::GameSessionInteractionId_ErrorViewerRequired : (kaiko::GameSessionInteractionId)interaction.id);
            const wchar_t* pText = localization_get_display_string_req(locaTag.name, nullptr);
            if (pText != nullptr)
            {
                kaiko::GameSessionInteractionFormattedDescriptionText description;
                keen::convertUTF16ToUTF8(description.text, sizeof(description.text), (const uint16*)pText);
                kaiko::fillVariableGameSessionInteractionParams(description, interaction.params);
                keen::debug::nativePrintArgs("Starting remote game session interaction %s\n", description.text);
                int handle;
                if (interaction.responseOptionCount == 0 || errorViewer)
                {
                    if (interaction.id == (unsigned int)kaiko::GameSessionInteractionId_WaitForPermissionCheck)
                    {
                        handle = ui_add_msgbox(MBT_LOADING, "MENU_CONNECTING_TO_MULTIPLAYER", "", nullptr, false, false, nullptr, nullptr, false);
                    }
                    else
                    {
                        handle = ui_add_msgbox(MBT_CONFIG_CONTROL, "REMOTE_GAME_SESSION_ERROR_TITLE", description.text, handleRemoteGameSessionMsgboxResponse, false, false, nullptr, nullptr, false);
                    }
                }
                else
                {
                    handle = ui_add_msgbox(MBT_CONFIG_CONTROL, "REMOTE_GAME_SESSION_ERROR_TITLE", description.text, handleRemoteGameSessionMsgboxResponse, false, true, "MENU_ACCEPT", nullptr, false);
                    for (unsigned int i = 0; i < interaction.responseOptionCount; i++)
                    {
                        kaiko::GameSessionInteractionLocaTag responseTag;
                        kaiko::generateGameSessionResponseLocaTag(responseTag, (kaiko::GameSessionResponseId)interaction.responseOptions[i]);
                        const wchar_t* pResponse = localization_get_display_string_req(responseTag.name, nullptr);
                        if (pResponse == nullptr)
                        {
                            pResponse = L" ";
                        }
                        kaiko::GameSessionInteractionFormattedDescriptionText responseText;
                        keen::convertUTF16ToUTF8(responseText.text, sizeof(responseText.text), (const uint16*)pResponse);
                        kaiko::fillVariableGameSessionInteractionParams(responseText, interaction.params);
                        ui_msgbox_add_caption_item(handle, responseText.text, false, false);
                    }
                }
                if (handle != -1)
                {
                    Remote_game_session_msgbox_handle = handle;
                    Remote_game_session_last_interaction = interaction;
                }
            }
        }
        g_gameLinkMutex.lock(0);
        if (Remote_game_session_msgbox_selection != 0xFFFFFFFFu)
        {
            Remote_game_session.setInteractionResponse(Remote_game_session_last_interaction.responseOptions[Remote_game_session_msgbox_selection]);
            if (Remote_game_session_msgbox_handle != -1)
            {
                ui_hide_msgbox(Remote_game_session_msgbox_handle, false);
                Remote_game_session_msgbox_handle = -1;
                keen::debug::nativePrintArgs("Remote game session interaction ended with %u\n", Remote_game_session_msgbox_selection);
            }
            memset(&Remote_game_session_last_interaction, 0, sizeof(Remote_game_session_last_interaction));
        }
        g_gameLinkMutex.unlock();
    }
    else
    {
        if (Remote_game_session_msgbox_handle != -1)
        {
            keen::debug::nativePrintArgs("Interaction no longer required.\n");
            ui_hide_msgbox(Remote_game_session_msgbox_handle, false);
            Remote_game_session_msgbox_handle = -1;
        }
        Remote_game_session_last_interaction = interaction;
    }
    player_profile_process();
    if (!Multi_core_platform_last_session_start_succeeded)
    {
        Multi_core_platform_last_session_start_succeeded = true;
        if (Login_changed_cb != nullptr)
        {
            Login_changed_cb(false, true);
        }
    }
}

// 0x00941FB0 (:412)
void multi_core_platform_internal_system_handshake()
{
}

// 0x00941FC0 (:417)
void multi_core_platform_internal_system_render()
{
}

// 0x00941FD0 (:422): the platform UI is up while the game window is not in front
bool multi_core_platform_internal_ui_is_active()
{
    return !rfg_is_gameframework_fully_in_foreground();
}

// 0x00941FE0 (:427)
bool multi_core_platform_internal_is_signed_into_profile()
{
    return true;
}

// 0x00941FF0 (:435)
bool multi_core_platform_internal_is_connected_to_network()
{
    return Remote_game_session.doesSystemHaveNetworkAccess();
}

// 0x00942000 (:440)
void multi_core_platform_internal_set_game_type_context(platform_game_type_context)
{
}

// 0x00942010 (:445): the presence enum indexes the rich presence strings
void multi_core_platform_internal_set_presence(platform_game_presence presence)
{
    rfg_get_local_game_session()->setPresence(presence);
}

// 0x00942030 (:450)
void multi_core_platform_internal_set_active_controller(int)
{
}

// 0x00970B40 (:456): the main user's profile name, shortened to 32 characters
bool multi_core_platform_internal_get_player_name(wchar_t* player_name, int max_player_name_size)
{
    keen::UserAccount profile;
    if (!rfg_get_local_game_session()->getProfile(profile, Remote_game_session.getMainUser()))
    {
        return false;
    }
    char shortName[257];
    rfg_shorten_display_string(profile.name.providerData, shortName, sizeof(shortName), 32, true);
    keen::convertUTF8ToUTF16((uint16*)player_name, max_player_name_size, shortName);
    return true;
}

// 0x00942040 (:482)
int multi_core_platform_internal_get_gamer_picture_buffer_size()
{
    return 0;
}

// 0x00942050 (:488)
bool multi_core_platform_internal_get_gamer_picture(const multi_player_uid&, void*, void (*)(const multi_player_uid&, bool))
{
    return false;
}

// 0x00942060 (:497)
bool multi_core_platform_internal_format_gamer_picture(unsigned char*)
{
    return false;
}

// Achievements 38, 39, 41 and 50 are never sent (magic numbers: the ids of keen::AchievementId the PC build skips).
static bool achievement_is_skipped(int achievement_no)
{
    return achievement_no == 38 || achievement_no == 39 || achievement_no == 41 || achievement_no == 50;
}

// 0x00942070 (:504)
void multi_core_platform_internal_update_achievement_progress(int achievement_no, unsigned int progress)
{
    if (!achievement_is_skipped(achievement_no) && SW_HasAchievements())
    {
        rfg_get_local_game_session()->setAchievementProgress((const keen::LocalPlayerIdStructureType*)0, achievement_no, progress);
    }
}

// 0x009420B0 (:518): the progress is the achievement's unlock value
void multi_core_platform_internal_award_achievement(int achievement_no)
{
    if (!achievement_is_skipped(achievement_no) && SW_HasAchievements())
    {
        const unsigned int status = keen::getAchievementUnlockValueMap()[achievement_no];
        rfg_get_local_game_session()->setAchievementProgress((const keen::LocalPlayerIdStructureType*)0, achievement_no, status);
    }
}

// 0x009420F0 (:534)
void multi_core_platform_internal_show_login_ui(bool)
{
}

// 0x00942100 (:543)
void multi_core_platform_internal_set_session_is_started(unsigned int)
{
}

// 0x00942110 (:548)
void multi_core_platform_internal_set_user_playlist_version(unsigned int)
{
}

// 0x00942120 (:553)
void multi_core_platform_internal_set_user_dlc_mask(unsigned int)
{
}

// 0x00942130 (:558)
void multi_core_platform_internal_set_user_playlist(unsigned int)
{
}

// 0x00942140 (:563)
void multi_core_platform_internal_set_game_mode_context(platform_game_mode_context)
{
}

// 0x00942150 (:568)
platform_nat_type multi_core_platform_internal_get_nat_type()
{
    return PNT_OPEN;
}

// 0x00942160 (:575)
void multi_core_platform_internal_multi_init(mempool_base&)
{
}

// 0x00942170 (:580)
void multi_core_platform_internal_multi_shutdown()
{
}

// 0x00942180 (:584)
bool multi_core_platform_internal_signin_change_is_pending()
{
    return false;
}

// 0x00970BE0 (:590): a ready local player that did not skip the sign-in
bool multi_core_platform_internal_is_signed_into_profile(int controller_index)
{
    unit4::LocalGameSession* pSession = rfg_get_local_game_session();
    const keen::LocalPlayerIdStructureType* pPlayerId = (const keen::LocalPlayerIdStructureType*)(uintptr_t)(controller_index & 0xFF);
    return pSession->isLocalPlayerReady(pPlayerId) && !pSession->hasLocalPlayerSkippedSignIn(pPlayerId);
}

// 0x00942190 (:597)
void multi_core_platform_internal_show_gamercard(const multi_player_uid& player_uid)
{
    g_gameLinkMutex.lock(0);
    Remote_game_session.showRemoteUserUi(player_uid.get_internal_id());
    g_gameLinkMutex.unlock();
}

// 0x009421C0 (:603)
bool multi_core_platform_internal_allow_gamer_card_request()
{
    return true;
}

// 0x009421D0 (:609)
void multi_core_platform_internal_show_player_review(const multi_player_uid&)
{
}

// 0x009421E0 (:615): there must be a session (not checked)
void multi_core_platform_internal_show_friends_ui()
{
    g_gameLinkMutex.lock(0);
    Remote_game_session.getGameLinkSession()->openInvitationDialog();
    g_gameLinkMutex.unlock();
}

// 0x009A7EF0 (:621): starts the GameLink session; on success the local player uid follows the link (LAN: the host
// address), on failure a pending invite is dropped. Locks the (recursive) mutex twice on failure (original).
bool multi_core_platform_internal_session_start(const multi_core_session_info& session_info)
{
    g_gameLinkMutex.lock(0);
    Multi_core_platform_last_session_start_succeeded = Remote_game_session.beginGameLinkSession(session_info.game_link_params);
    Pending_invite.had_invite_when_entering_multiplayer = Pending_invite.pending;
    if (Multi_core_platform_last_session_start_succeeded)
    {
        g_gameLinkConnectionType = session_info.game_link_params.connectionType;
        multi_core_platform_set_my_player_uid(false);
    }
    else
    {
        g_gameLinkMutex.lock(0);
        kaiko::gamelink::setInvitationData(nullptr);
        Pending_invite.pending = false;
        Pending_invite.had_invite_when_entering_multiplayer = false;
        g_gameLinkMutex.unlock();
    }
    const bool succeeded = Multi_core_platform_last_session_start_succeeded;
    g_gameLinkMutex.unlock();
    return succeeded;
}

// 0x00942210 (:644)
void multi_core_platform_internal_session_end()
{
    g_gameLinkMutex.lock(0);
    Remote_game_session.endGameLinkSession();
    g_gameLinkMutex.unlock();
}

// 0x00942230 (:650): the session's critical error, or the first local player exited
bool multi_core_platform_internal_session_has_critical_error()
{
    if (Remote_game_session.isInCriticalErrorState())
    {
        return true;
    }
    return rfg_get_local_game_session()->didLocalPlayerExit((const keen::LocalPlayerIdStructureType*)0);
}

// 0x00942260 (:656): not used on PC
void multi_core_platform_internal_register_massage_send_data_callback(bool (*)(const unsigned char*, int, unsigned char*, int, int&, unsigned char*, int&))
{
}

// 0x00942270 (:664): an unreliable packet to the station of the target; a failed send without a link error counts as
// sent (the reliable layer above resends)
bool multi_core_platform_internal_send_packet(const connection_address& target, unsigned char* send_data, unsigned int data_length)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* pLink = Remote_game_session.getGameLinkSession();
    if (!target.connection_id.isValid)
    {
        keen::debug::nativePrintArgs("multi_core_platform_internal_send_packet() invalid connection_id\n");
    }
    else
    {
        const kaiko::GameLinkRemoteStation* pStation = pLink->getRemoteStationById(target.connection_id);
        if (pStation != nullptr && (pLink->sendP2PPacketUnreliable(pStation, send_data, data_length) || pLink->getError(nullptr) == kaiko::GameLinkError_None))
        {
            g_gameLinkMutex.unlock();
            return true;
        }
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x0098E060 (:699): the next queued packet and its sender's station id; a packet from a non-member is dropped (0)
unsigned int multi_core_platform_internal_receive_packet(connection_address& origin, unsigned char* data_buffer, unsigned int capacity)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* pLink = Remote_game_session.getGameLinkSession();
    origin.init();
    const kaiko::GameLinkRemoteStation* pSender = nullptr;
    const unsigned int size = pLink->getNextP2PPacket(data_buffer, capacity, &pSender);
    if (size != 0)
    {
        if (pSender == nullptr)
        {
            g_gameLinkMutex.unlock();
            return 0;
        }
        origin.set(pSender->id);
    }
    g_gameLinkMutex.unlock();
    return size;
}

// 0x00942300 (:727)
void multi_core_platform_internal_set_game_property(platform_game_property, unsigned int)
{
}

// 0x00942310 (:732)
platform_op_status multi_core_platform_internal_get_user_playlist_state()
{
    return POS_SUCCESS;
}

// 0x00942320 (:738)
platform_op_status multi_core_platform_internal_get_user_dlc_mask_state()
{
    return POS_SUCCESS;
}

// 0x00942330 (:744)
platform_op_status multi_core_platform_internal_get_user_playlist_version_state()
{
    return POS_SUCCESS;
}

// 0x00942340 (:750)
platform_op_status multi_core_platform_internal_get_session_is_started_state()
{
    return POS_SUCCESS;
}

// 0x00942350 (:756)
void multi_core_platform_internal_retrieve_remote_file(unsigned char**, unsigned int*)
{
}

// 0x00942360 (:761)
platform_op_status multi_core_platform_internal_get_retrieve_remote_file_state()
{
    return POS_FAILED;
}

// 0x00942370 (:767)
void multi_core_platform_internal_remote_file_process()
{
}

// 0x009A7F70 (:772): the player's level as the skill (left as it is for a marauder or an unknown player)
void multi_core_platform_internal_get_player_skill(skill_info* skill, const multi_player_uid& player_uid)
{
    const multi_game_data* pData = multi_game_data_get_by_player_uid(ST_GAME, player_uid);
    if (pData != nullptr && pData->team != HUMAN_TEAM_MARAUDER)
    {
        skill->skill_ranking = (double)pData->player_level;
        skill->skill_uncertainty = 0.0;
    }
}

// 0x00942380 (:795): the mean, added to what aggregate_skill holds
void multi_core_platform_internal_get_aggregate_skill(skill_info& aggregate_skill, skill_info* skills, unsigned int num_skills)
{
    for (unsigned int i = 0; i < num_skills; i++)
    {
        aggregate_skill.skill_ranking = aggregate_skill.skill_ranking + skills[i].skill_ranking;
        aggregate_skill.skill_uncertainty = skills[i].skill_uncertainty + aggregate_skill.skill_uncertainty;
    }
    aggregate_skill.skill_ranking = aggregate_skill.skill_ranking / (double)num_skills;
    aggregate_skill.skill_uncertainty = aggregate_skill.skill_uncertainty / (double)num_skills;
}

// 0x00942480 (:809)
bool multi_core_platform_internal_get_large_skill_variance(const multi_player_uid*, unsigned int)
{
    return false;
}

// 0x00942490 (:815): one user on controller 0
void multi_core_platform_internal_get_logged_in_users(unsigned int* controller_indices, int& num_users, const int)
{
    controller_indices[0] = 0;
    num_users = 1;
}

// 0x009424B0 (:824)
int multi_core_platform_internal_get_dlc_avail()
{
    return 0;
}

// 0x0098E100 (:829): from the signed-in user's account, or (fromLocalUserAccountId false) from the link: on LAN the
// host's network address (the game exits without one), on the internet the main user's remote id (exits without one)
void multi_core_platform_set_my_player_uid(bool fromLocalUserAccountId)
{
    unit4::RemoteUserId id;
    if (fromLocalUserAccountId)
    {
        unit4::LocalGameSession* pSession = rfg_get_local_game_session();
        const keen::LocalPlayerIdStructureType* pLocalPlayerId = pSession->getFirstSignedInUser();
        keen::UserAccount profile;
        if (pLocalPlayerId != No_local_player && pSession->getProfile(profile, pLocalPlayerId))
        {
            unit4::systemservices::createRemoteUserIdFromUserAccountId(&id, profile.id);
            My_player_uid.set(id, true);
            return;
        }
        My_player_uid.invalidate();
        return;
    }
    if (g_gameLinkConnectionType == kaiko::GameLinkConnectionType_Default)
    {
        memset(id.providerData, 0, sizeof(id.providerData));
        id.guestIndex = 0;
        id.isValid = true;
        keen::NetworkAddress* pAddress = (keen::NetworkAddress*)id.providerData;
        // deviation: the own address of the localhost deviation (original: getHostNetworkAddress(pAddress, ""))
        if (kaiko::gamelink::openrfgGetLanOwnHostAddress(pAddress))
        {
            // OpenRFG deviation (multiplayer.md 2.3): with a local LAN port the uid carries it, so that instances on
            // one machine get different uids (original: the port stays 0)
            pAddress->port = kaiko::gamelink::g_openrfgLanLocalPort;
            My_player_uid.set(id, true);
            return;
        }
        exit(1);
    }
    if (rfg_get_local_game_session()->getRemoteUserId(id, Remote_game_session.getMainUser()))
    {
        My_player_uid.set(id, true);
        return;
    }
    exit(1);
}

// 0x0098E280 (:881): the ranking results buffer from the pool; the local uid from the signed-in account (cleared
// without one). Always answers true.
bool multi_core_platform_internal_scoreboard_init(mempool* pool, int max_entries)
{
    Scoreboard_read_progress = POS_FAILED;
    Last_scoreboard_results = nullptr;
    Current_scoreboard_ranking_results.pEntries = (unit4::RankingListEntry*)pool->alloc(max_entries * sizeof(unit4::RankingListEntry), 0);
    Current_scoreboard_ranking_results.totalRankingEntries = 0;
    Current_scoreboard_ranking_results.filledEntriesCount = 0;
    Max_num_ranking_results = max_entries;
    unit4::LocalGameSession* pSession = rfg_get_local_game_session();
    const keen::LocalPlayerIdStructureType* pLocalPlayerId = pSession->getFirstSignedInUser();
    keen::UserAccount profile;
    if (pLocalPlayerId != No_local_player && pSession->getProfile(profile, pLocalPlayerId))
    {
        unit4::RemoteUserId id;
        unit4::systemservices::createRemoteUserIdFromUserAccountId(&id, profile.id);
        My_player_uid.set(id, true);
        return true;
    }
    // OpenRFG deviation (UI.md UI5): no user accounts here (addLocalPlayer is skipped), so the link's uid that
    // multi_init's session start just set is kept. rfg.exe always has a signed-in Steam user at this point (the
    // account id replaces the uid); clearing it (original: My_player_uid.invalidate()) leaves the host's own connection
    // unmatched and the lobby's best host selection never ends.
    return true;
}

// 0x009424C0 (:897)
void multi_core_platform_internal_scoreboard_shutdown()
{
    Scoreboard_read_progress = POS_FAILED;
    Last_scoreboard_results = nullptr;
    Current_scoreboard_ranking_results.pEntries = nullptr;
    Current_scoreboard_ranking_results.totalRankingEntries = 0;
    Current_scoreboard_ranking_results.filledEntriesCount = 0;
    Max_num_ranking_results = 0;
}

// 0x00970C30 (:909)
void multi_core_platform_internal_scoreboard_retrieve_data(scoreboard_results* results, const scoreboard_request* request)
{
    unit4::LocalGameSession* pSession = rfg_get_local_game_session();
    Last_scoreboard_results = results;
    memcpy(Last_scoreboard_column_infos, request->info->column_info, sizeof(Last_scoreboard_column_infos));
    unit4::RankingListQueryType queryType = unit4::RankingListQueryType_AllCenteredOnUser;
    switch (request->request_type)
    {
    case SRT_DEFAULT:
    case SRT_PLAYERS:
        queryType = unit4::RankingListQueryType_AllWithOffset;
        break;
    case SRT_FRIENDS:
        queryType = unit4::RankingListQueryType_FriendsWithOffset;
        break;
    default:
        break;
    }
    const unsigned int maxNumEntries = (unsigned int)request->entries_to_retrieve < Max_num_ranking_results ? request->entries_to_retrieve : Max_num_ranking_results;
    const int offset = request->rank_offset - 1;
    if (unit4::systemservices::startGetRankingList(pSession->getSystemServices(), (uint16)request->info->scoreboard_index, maxNumEntries, queryType, &request->player_uids->get_internal_id(), offset < 1 ? 0 : offset))
    {
        Scoreboard_read_op_active = true;
        Scoreboard_read_progress = POS_IN_PROGRESS;
        return;
    }
    Scoreboard_read_progress = POS_FAILED;
}

// 0x00970D00 (:977): copies the finished ranking list into the results (uid, shortened name, rank, the score and up
// to three details as the columns)
platform_op_status multi_core_platform_internal_scoreboard_retrieve_state()
{
    if (!Scoreboard_read_op_active)
    {
        return Scoreboard_read_progress;
    }
    unit4::LocalGameSession* pSession = rfg_get_local_game_session();
    unsigned int capacity = Max_num_ranking_results;
    if (Last_scoreboard_results != nullptr && (unsigned int)Last_scoreboard_results->max_entries <= Max_num_ranking_results)
    {
        capacity = Last_scoreboard_results->max_entries;
    }
    if (!unit4::systemservices::getRankingListResult(pSession->getSystemServices(), &Current_scoreboard_ranking_results, capacity))
    {
        return Scoreboard_read_progress;
    }
    if (unit4::systemservices::getRankingError(pSession->getSystemServices()) != unit4::RankingError_None || Last_scoreboard_results == nullptr)
    {
        Scoreboard_read_progress = POS_FAILED;
        return Scoreboard_read_progress;
    }
    if (Scoreboard_read_progress != POS_IN_PROGRESS)
    {
        return Scoreboard_read_progress;
    }
    for (unsigned int i = 0;; i++)
    {
        const unsigned int count = Current_scoreboard_ranking_results.filledEntriesCount < (unsigned int)Last_scoreboard_results->max_entries ? Current_scoreboard_ranking_results.filledEntriesCount : Last_scoreboard_results->max_entries;
        if (i >= count)
        {
            break;
        }
        const unit4::RankingListEntry& ranking = Current_scoreboard_ranking_results.pEntries[i];
        scoreboard_entry& entry = Last_scoreboard_results->entries[i];
        entry.player_uid.set(ranking.userInfo.id, true);
        char shortName[257];
        rfg_shorten_display_string(ranking.userInfo.displayName, shortName, sizeof(shortName), 32, true);
        keen::convertUTF8ToUTF16((uint16*)entry.player_name, sizeof(entry.player_name), shortName);
        entry.rank = ranking.rank;
        for (unsigned int c = 0; c < 4; c++)
        {
            entry.columns[c].column_type = Last_scoreboard_column_infos[c].column_type;
        }
        entry.columns[0].column_value.i_val = ranking.score;
        const unsigned int numDetails = ranking.numScoreDetails < 3 ? ranking.numScoreDetails : 3;
        for (unsigned int d = 0; d < numDetails; d++)
        {
            entry.columns[d + 1].column_value.i_val = ranking.scoreDetails[d];
        }
    }
    Last_scoreboard_results->num_entries = Current_scoreboard_ranking_results.filledEntriesCount;
    Last_scoreboard_results->total_entries_on_scoreboard = Current_scoreboard_ranking_results.totalRankingEntries;
    Scoreboard_read_progress = POS_SUCCESS;
    return POS_SUCCESS;
}

// 0x009424F0 (:1028)
void multi_core_platform_internal_scoreboard_cancel_retrieve()
{
    Scoreboard_read_progress = POS_FAILED;
    Last_scoreboard_results = nullptr;
}

// 0x00942510 (:1035): the first column is the score, the next ones its details. Original behaviour: always three
// details are sent (the ones beyond num_scores - 1 uninitialized), and more than three overflow the copy.
bool multi_core_platform_internal_scoreboard_begin_upload_score(const multi_player_uid& player_uid, unsigned short table_id, scoreboard_column* scores, unsigned int num_scores)
{
    unit4::LocalGameSession* pSession = rfg_get_local_game_session();
    unsigned int details[3];
    for (unsigned int i = 1; i < num_scores; i++)
    {
        details[i - 1] = scores[i].column_value.i_val;
    }
    return unit4::systemservices::startSendRankingEntry(pSession->getSystemServices(), player_uid.get_internal_id(), table_id, scores[0].column_value.i_val, details, 3);
}
