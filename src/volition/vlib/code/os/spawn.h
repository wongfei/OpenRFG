// Original: volition/vlib/code/os/spawn.h
// Implementation: volition/vlib/code/os/spawn.cpp
//
// Spawning processes with redirected pipes (process_handle_internal, redirected_pipe_io). Placement of
// redirected_pipe_io inferred.
#pragma once

// size 0x8
struct redirected_pipe_io
{
    void* read_end; // 0x00
    void* write_end; // 0x04
};

// size 0x24
struct process_handle_internal
{
    void clear(); // 0x0058BC80 (.h:32)

    void* process_h; // 0x00
    void* thread_h; // 0x04
    void* input_pipe_read; // 0x08
    void* input_pipe_write; // 0x0C
    void* output_pipe_read; // 0x10
    void* output_pipe_write; // 0x14
    void* error_pipe_read; // 0x18
    void* error_pipe_write; // 0x1C
    unsigned int pipe_flags; // 0x20
};

process_handle_internal* process_create(const char* cmd, const char* args, void* input, void* output, void* error, unsigned long flags); // 0x005BDF60 (:134)
process_handle_internal* process_spawn_detached(const char* cmd, const char* args); // 0x005BE100 (:211)
process_handle_internal* process_spawn(const char* cmd, const char* args, void* redirected_input, void* redirected_output, void* redirected_error); // 0x005BE120 (:226)
process_handle_internal* process_spawn_redirected(const char* cmd, const char* args, void** redirected_input, void** redirected_output, void** redirected_error); // 0x005BE170 (:258)
int process_wait(process_handle_internal* process); // 0x0058BE50 (:314)
bool process_poll(process_handle_internal* process, int& error_code); // 0x0058BEB0 (:340)
bool process_send_input(const char* buf, void* redirected_input_handle, bool send_eof); // 0x0058BF00 (:371)
int process_get_output_byte_count(void* pipe_h); // 0x0058BF70 (:408)
bool process_get_output(char* dest, unsigned int len, void* redirected_output_handle, unsigned int* remaining); // 0x0058BFA0 (:429)
bool process_spawn_simple(const char*, const char*, ...); // 0x005C89C0 (:469)
bool process_spawn_simple_v(const char* cmd, const char** argv, int argc); // 0x005C8AA0 (:508)
bool process_spawn_piped(const char** appv, unsigned int appc); // 0x005C8B70 (:531)
bool process_terminate(process_handle_internal* process, unsigned int exit_code); // 0x005C8E20 (:581)
void process_close(process_handle_internal* process); // 0x005C48A0 (:599)
void process_pipe_close(void** handle); // 0x0058C020 (:627)
