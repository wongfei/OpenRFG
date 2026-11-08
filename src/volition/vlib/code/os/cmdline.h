// Original: volition/vlib/code/os/cmdline.h
// Implementation: volition/vlib/code/os/cmdline.cpp
//
// Command line parsing (cmdline_param, a registered command line option).
#pragma once

// size 0x18
class cmdline_param
{
public:
    cmdline_param(const char* name, const char* help, bool has_argument); // 0x0058ABE0 (:296)
    ~cmdline_param(); // 0x005AD1C0 (:317)
    void delete_args(); // 0x0058AC50 (:328)
    bool found() const; // 0x0058AC60 (:337)
    int get_int() const; // 0x0058AC70 (:344)
    float get_float() const; // 0x0058AC80 (:352)
    const char* get_str() const; // 0x0058ACA0 (:360)

    cmdline_param* next; // 0x00
    cmdline_param* prev; // 0x04
    const char* name; // 0x08
    const char* help; // 0x0C
    char* args; // 0x10
    bool name_found; // 0x14
    bool has_argument; // 0x15
};

void cmdline_init(const char* cmdline, bool validate_parms, bool skip_first); // 0x005AD040 (:251)
void cmdline_init(const int cmdline_count, const char** cmdline, bool validate_parms, bool skip_first); // 0x005AD0D0 (:272)
const char* cmdline_get_solo_arg(int which_one); // 0x0058ACB0 (:376)
