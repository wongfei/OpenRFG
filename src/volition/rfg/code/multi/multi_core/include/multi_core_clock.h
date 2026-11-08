// Original: volition/rfg/code/multi/multi_core/include/multi_core_clock.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_clock.cpp
//
// The network clock: timestamps on the server's time base (synchronized from the clients' round trips), used for state
// updates and the timers of multiplayer objects.
#pragma once


class multi_connection;
struct sender;

// size 0x4
class multi_timestamp
{
public:
    multi_timestamp(); // 0x00970650 (.h:48)
    void invalidate(); // 0x00941CF0 (.h:49)
    void set(int delta_milliseconds); // 0x00943FA0 (:199)
    void add_time(int milliseconds); // 0x00943FE0 (:217)
    void deduct_time(int milliseconds); // 0x00943FF0 (:222)
    bool valid() const; // 0x009440C0 (:297)
    bool elapsed() const; // 0x00944000 (:229)
    int time_until() const; // 0x00944030 (:261)
    int time_since() const; // 0x00944070 (:289)

private:
    int value; // 0x00
};

// size 0x8
class multi_timestamp_percent : public multi_timestamp
{
public:
    void set(int milliseconds); // 0x009440D0 (:306)
    float percent() const; // 0x00944110 (:315)
    int time_since_set() const; // 0x00944160 (:328)
    multi_timestamp_percent(); // 0x00A33DC0

private:
    int set_milliseconds; // 0x04
};

void multi_core_clock_system_init(); // 0x00943F30 (:88)
void multi_core_clock_process(); // 0x00943F50 (:96)
unsigned int multi_core_clock_get_time(); // 0x00943F80 (:111)
void multi_core_clock_set_time(unsigned int current_time); // 0x00943F90 (:117)
void multi_core_process_clock_packet(unsigned char* packet_data, const sender& from); // 0x00971E10 (:127)
void multi_core_clock_poll_for_packet(multi_connection* cp, unsigned int max_size); // 0x00971EA0 (:168)

// 0x00970650 (.h:48)
inline multi_timestamp::multi_timestamp()
    : value(-1)
{
}

// 0x00A33DC0: the base constructor only.
inline multi_timestamp_percent::multi_timestamp_percent()
{
}
