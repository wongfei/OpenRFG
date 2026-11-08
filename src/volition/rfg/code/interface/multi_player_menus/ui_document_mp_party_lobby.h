// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.cpp
//
// The party lobby (Steam parties). Placement of mp_party_states inferred.
#pragma once


struct sender;

enum mp_party_lobby_types
{
    MPPLT_CUSTOM_GAME = 0,
    MPPLT_MATCHMAKING = 1,
    MPPLT_UNKNOWN = 2,
    NUM_MP_PARTY_LOBBY_TYPES = 3,
    NUM_MP_SELECTABLE_PARTY_LOBBY_TYPES = 2,
};

enum mp_party_states
{
    MPS_NONE = 0,
    MPS_HOST_CREATING_LOBBY = 1,
    MPS_HOST_LOBBY_READY = 2,
    MPS_HOST_WAITING_ON_PARTY = 3,
    MPS_HOST_WAITING_ON_CLIENTS_TO_JOIN = 4,
    MPS_HOST_STARTING_GAME = 5,
    MPS_CLIENT_WAITING_FOR_LOBBY = 6,
    MPS_CLIENT_WAITING_FOR_SERVER = 7,
    MPS_CLIENT_MOVING_TO_MATCHMAKING = 8,
    MPS_ERROR = 9,
    NUM_MP_PARTY_STATES = 10,
};

extern mp_party_lobby_types Party_lobby_type; // 0x01663610
extern const char* Party_lobby_invite_type_names[5]; // 0x01663668

void ui_document_mp_party_lobby_init(); // 0x00915280 (:1994)
void ui_document_mp_party_lobby_go_matchmake(unsigned char* packet_data, const sender& from); // 0x008AE020 (:2010)
void ui_document_mp_party_lobby_type_request(unsigned char* packet_data, const sender& from); // 0x008AE100 (:2050)
void ui_document_mp_party_lobby_type(unsigned char* packet_data, const sender& from); // 0x0088CE10 (:2055)
void ui_document_mp_party_lobby_return_to_lobby(unsigned char* packet_data, const sender& from); // 0x008AE110 (:2061)
mp_party_lobby_types party_lobby_get_next_type(mp_party_lobby_types current_type); // 0x0088CE40 (:2072)
mp_party_lobby_types party_lobby_get_prev_type(mp_party_lobby_types current_type); // 0x0088CE60 (:2077)
void party_lobby_send_type_update(); // 0x0088CE80 (:2082)
void party_lobby_reset_party_state(); // 0x0088CF50 (:2091)
mp_party_states party_lobby_get_party_state(); // 0x0088CF60 (:2097)
mp_party_states party_lobby_get_pending_party_state(); // 0x0088CF70 (:2102)
