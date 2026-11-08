// Original: game/build/win32/steam/master/unity_sources/rfg_multi_unity.cpp (the multi unity build)
//
// rfg.exe compiles the whole multi/ tree as this one translation unit (88 .cpp files in path order, rfg.pdb compiland
// rfg_multi_unity.obj), so the file statics are shared by the whole tree. Partial: the files ported so far, in that
// order (multiplayer.md MP2: the platform layer, the player uids, the connection addresses; MP3: multi_core, the
// sessions, joining and leaving; MP4: the match flow, the deathmatch row and the multi_game data, tables and
// settings; MP5: compression, delta compression, the remote objects, the mover registrations, the sim states and
// events, and the destruction sync; MP8: the weapons, the projectiles, the items and the client-created projectiles;
// MP10: the backpacks, the backpack racks and the markers; stub_impl.md S6: the experience, multi_game_xp.cpp, the badges, the playlists,
// the player icons (multi_game_render.cpp), the server migration and its destruction sync, the friends, voice chat, the parties, and the
// bagman, capture the flag, demolition, king of the hill, siege, team bagman and team deathmatch modes, the flag objects and
// the flag capture zones).
// multi_core_compression.cpp and multi_game_destruction.cpp are also compiled alone
// in rfg_multi_alone (with OPENRFG_MULTI_DESTRUCTION_ALONE) for the programs without this TU.

// Port-only: winsock2.h before anything pulls in windows.h (whose winsock.h it would collide with); multi.cpp,
// multi_connection.cpp and multi_core_io.cpp include it.
#include <winsock2.h>

#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.cpp"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_platform_generic.cpp"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.cpp"
#include "volition/rfg/code/multi/multi_core/generic/multi_voice_generic.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_connection.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_bandwidth.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_clock.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_compression.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_delta_compression.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_game_bandwidth.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_io.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_matchmaking.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_objects.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_platform.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_qos.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_find_servers.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_join.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_leave.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_player_uid.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_session.cpp"
#include "volition/rfg/code/multi/multi_core/global/multi_voice.cpp"

// Port-only: the file statics of the multi files not ported yet that the ported multi_game files use (rfg.exe defines
// them in their files, between here and the users). See the file.
#include "game/build/win32/steam/master/unity_sources/rfg_multi_unity_pending_early.inl"

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_bagman.cpp"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_capture_the_flag.cpp"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.cpp"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_deathmatch.cpp"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.cpp"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.cpp"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.cpp"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_bagman.cpp"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_deathmatch.cpp"

#include "volition/rfg/code/multi/multi_game/multi_game.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_backpack_rack.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_callbacks.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_character.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_classes.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_client_create.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_client_predict.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_data.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction_migration.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_flag_capture_zone.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_friend.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_io.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_internal.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_flag.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_items.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_movers.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_player.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_projectile.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_turret.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_vehicle_automobile.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_object_vehicle_walker.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_overrides.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_party.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_player.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_preferences.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_render.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_respawn.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_event.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_timer.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.cpp"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.cpp"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.cpp"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.cpp"
