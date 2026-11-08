// Original: volition/vlib/code/os/cmdline.cpp
//
// Command line parameters: every named cmdline_param links itself into Parm_list when it is constructed (static
// initializers, before main), and unlinks when it is destroyed.
//
// Partial (final_push.md step 5): the constructor, the destructor and the list head. rfg.exe never parses a command
// line through this file (cmdline_init, the parsing and validation, and the readers found/get_int/get_float/get_str
// have no callers: the keen command line is used instead), so the parameters rfg.cpp registers are never read.

#include "volition/vlib/code/os/cmdline.h"

#include <stddef.h>

// 0x01CE86EB: set once the list head points at itself. The constructor checks it because the parameters' static
// initializers can run before Parm_list's own.
static bool Parm_list_inited;
// 0x0163E670: the list head, a parameter without a name (rfg.exe's dynamic initializer at 0x010601F0 keeps only the
// self-link of the inlined constructor; the other fields are zero data).
static cmdline_param Parm_list(NULL, NULL, false);

// 0x0058ABE0 (:296)
cmdline_param::cmdline_param(const char* name, const char* help, bool has_argument)
{
    this->help = help;
    this->has_argument = has_argument;
    this->name = name;
    args = NULL;
    name_found = false;
    if (!Parm_list_inited)
    {
        Parm_list.next = &Parm_list;
        Parm_list.prev = &Parm_list;
        Parm_list_inited = true;
    }
    if (name != NULL)
    {
        prev = Parm_list.prev;
        next = &Parm_list;
        Parm_list.prev->next = this;
        Parm_list.prev = this;
    }
}

// 0x005AD1C0 (:317)
// Notes (decompiled behaviour, kept as is): args is only cleared, not freed (delete_args frees it).
cmdline_param::~cmdline_param()
{
    if (args != NULL)
    {
        args = NULL;
    }
    if (name != NULL)
    {
        prev->next = next;
        next->prev = prev;
        next = NULL;
        prev = NULL;
    }
}
