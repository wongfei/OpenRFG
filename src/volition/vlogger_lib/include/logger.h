// Original: volition/vlogger_lib/include/logger.h
// Implementation: volition/vlogger_lib/logger_stubs.cpp
//
// The Volition event logger interface (telemetry sessions and events); the game links the stubs (logger_stubs.cpp),
// which do nothing.
#pragma once

enum log_modes
{
    LOG_BUFFER = 0,
    LOG_FILE = 1,
    LOG_DEBUG = 2,
    NUM_LOG_MODES = 3,
};

int log_system_init(char* mem_buffer, int& size, unsigned int continuous_event_update_time, unsigned short events_version); // 0x00401000 (logger_stubs.cpp:4)
void log_system_shutdown(); // 0x00401010 (logger_stubs.cpp:13)
int log_get_session_request(char* message_buffer, unsigned int& buffer_size); // 0x00401020 (logger_stubs.cpp:17)
int log_start_session(char* log_priorities, unsigned int& log_priorities_size, char* packet_buffer, unsigned short total_event_type_count, unsigned char player_id); // 0x00401030 (logger_stubs.cpp:24)
void log_end_session(); // 0x00401040 (logger_stubs.cpp:34)
int log_set_mode(log_modes mode); // 0x00401050 (logger_stubs.cpp:38)
bool log_record_event(unsigned char type_id); // 0x00401060 (logger_stubs.cpp:44)
void log_add_event(unsigned char typeID, int argc, void** argv, unsigned char* sizes, bool* variable_length); // 0x00401070 (logger_stubs.cpp:50)
unsigned int log_start_continuous_event(unsigned char typeID, int argc, void** argv, unsigned char* sizes, bool* variable_length); // 0x00401080 (logger_stubs.cpp:59)
int log_stop_continuous_event(unsigned int eventHandle); // 0x00401090 (logger_stubs.cpp:69)
void log_update_continuous_events(unsigned int numUnits); // 0x004010A0 (logger_stubs.cpp:75)
int log_send_events(char* send_buffer, unsigned short buffer_size); // 0x004010B0 (logger_stubs.cpp:80)
int log_get_config_profile(char* send_buffer, unsigned short buffer_size); // 0x004010C0 (logger_stubs.cpp:87)
