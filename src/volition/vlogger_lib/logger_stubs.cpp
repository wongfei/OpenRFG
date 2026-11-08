// Original: volition/vlogger_lib/logger_stubs.cpp
//
// Volition's own no-op event logger (stub_impl.md S2, 2.3): the game links these stubs, so telemetry sessions and
// events go nowhere. Original behaviour kept: log_record_event says yes, so the game builds every event it then
// drops.

#include "volition/vlogger_lib/include/logger.h"

// 0x00401000 (:4)
int log_system_init(char*, int&, unsigned int, unsigned short)
{
    return 0;
}

// 0x00401010 (:13)
void log_system_shutdown()
{
}

// 0x00401020 (:17)
int log_get_session_request(char*, unsigned int&)
{
    return 0;
}

// 0x00401030 (:24)
int log_start_session(char*, unsigned int&, char*, unsigned short, unsigned char)
{
    return 0;
}

// 0x00401040 (:34)
void log_end_session()
{
}

// 0x00401050 (:38)
int log_set_mode(log_modes)
{
    return 0;
}

// 0x00401060 (:44)
bool log_record_event(unsigned char)
{
    return true;
}

// 0x00401070 (:50)
void log_add_event(unsigned char, int, void**, unsigned char*, bool*)
{
}

// 0x00401080 (:59)
unsigned int log_start_continuous_event(unsigned char, int, void**, unsigned char*, bool*)
{
    return 0;
}

// 0x00401090 (:69)
int log_stop_continuous_event(unsigned int)
{
    return 0;
}

// 0x004010A0 (:75)
void log_update_continuous_events(unsigned int)
{
}

// 0x004010B0 (:80)
int log_send_events(char*, unsigned short)
{
    return 0;
}

// 0x004010C0 (:87)
int log_get_config_profile(char*, unsigned short)
{
    return 0;
}
