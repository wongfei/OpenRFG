// Original: volition/rfg/code/multi/multi_game/multi_game_migration.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_migration.cpp
//
// Server migration: when the server leaves, a client becomes the server (migration state machine, timers).
#pragma once


class multi_session_base;
struct sender;

enum migration_state
{
    MIGRATION_STATE_NONE = 0,
    MIGRATION_SERVER_REQ_DESTRUCTION_STATUS = 1,
    MIGRATION_SERVER_WAIT_DESTRUCTION_STATUS = 2,
    MIGRATION_SERVER_WAIT_FOR_DESTRUCTION_UPDATE = 3,
    MIGRATION_SERVER_APPLY_DESTRUCTION = 4,
    MIGRATION_SERVER_DESTROY_OBJECTS = 5,
    MIGRATION_SERVER_RESET_REMOTE_OBJECT_SYSTEM = 6,
    MIGRATION_SERVER_WAIT_FOR_CREATE_PROCESS = 7,
    MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_UPDATE = 8,
    MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_APPLIED = 9,
    MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_APPLIED_POST = 10,
    MIGRATION_CLIENT_WAIT_FOR_CREATE_PROCESS = 11,
    MIGRATION_STATE_FAILED = 12,
    NUM_MIGRATION_STATES = 13,
};

void multi_game_migration_set_allow_migration_timer(); // 0x00956F60 (:421)
bool multi_game_migration_allow_migration_timer_elapsed(); // 0x00956F70 (:426)
void multi_game_clear_has_migrated(); // 0x00956F80 (:431)
bool multi_game_has_migrated(); // 0x00956F90 (:437)
bool multi_game_has_skipped_game_migration(); // 0x00956FA0 (:442)
void multi_game_migration_level_shutdown(); // 0x00956FB0 (:447)
void multi_game_migration_set_state(migration_state state); // 0x00956FC0 (:452)
void multi_game_migration_prepare_for_migration(multi_session_base*); // 0x00956FD0 (:461)
void multi_game_migration_set_skipped_game_migration(); // 0x00956FE0 (:469)
void multi_game_migration_start_migration(multi_session_base* sp); // 0x009F1140 (:476)
void multi_game_migration_do_frame(); // 0x009F8490 (:853)
bool multi_game_is_migrating(); // 0x00957010 (:862)
void multi_game_migration_complete(unsigned char* packet_data, const sender& from); // 0x00999BA0 (:867)
void multi_game_process_migrate_last_df(unsigned char* packet_data, const sender& from); // 0x00957020 (:895)
void multi_game_process_migrate_req_status(unsigned char* packet_data, const sender& from); // 0x009DF2E0 (:930)
bool multi_game_is_migration_state(migration_state _state); // 0x00957110 (:968)
void multi_game_migration_init(); // 0x00957120 (:973)
