// Original: volition/lua_lib/code/lua_scripts.cpp
//
// Volition's script layer over Lua 5.0 (lua_lib_unity in rfg.exe, with lua_system_actions.cpp and lua_thread.cpp,
// which share this file's statics): the allocator callbacks, the up to four root states, script loading (system_lib.lua
// at state creation, static and dynamic scripts from cfile or memory), the global accessors, the nil-assignment and
// global-variable locks, the per-frame garbage collection and the paused root states. The debug print functions are
// empty in the release build, so the dump helpers only walk the stack.

#include "volition/lua_lib/code/lua_scripts.h"

#include "volition/libmodules/lua/lua_1_21/lauxlib.h"
#include "volition/libmodules/lua/lua_1_21/lmem.h"
#include "volition/libmodules/lua/lua_1_21/lua.h"
#include "volition/libmodules/lua/lua_1_21/lualib.h"
#include "volition/lua_lib/code/lua_system_actions.h"
#include "volition/lua_lib/code/lua_thread.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <stdlib.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0x8
struct lua_active_state
{
    lua_State* lua_state; // 0x00
    void (*error_cb)(lua_thread*); // 0x04
};
static_assert(sizeof(lua_active_state) == 0x8, "lua_active_state");

// ---- file-static globals (shared with lua_thread.cpp and lua_system_actions.cpp through lua_lib_unity)
static unsigned int Paused_lua_state_count[4]; // 0x038B7170
static lua_active_state Lua_states[4]; // 0x038B7180
static lua_State* Paused_lua_states[4]; // 0x038B91E0
static unsigned int Thread_stack_depth; // 0x038B91F0
static lua_State* Default_lua_state; // 0x038B91F4
static unsigned int Num_lua_states; // 0x038B91F8
static int (*Get_total_mem)(); // 0x038B91FC
static int (*Get_free_mem)(); // 0x038B9200
static int Num_script_threads; // 0x038B9204
static unsigned short Next_thread_handle; // 0x038B9208
static void* (*Realloc_cb)(void*, unsigned int); // 0x038B9214
static void (*Free_cb)(void*); // 0x038B9218

static const int Max_lua_states = 4;

// 0x00C313D0 (:130)
// Indentation for the dumps; empty in the release build.
static void indent_helper(int count)
{
    (void)count;
}

// 0x00C2FF10 (:138)
// filename with ".lua" appended unless it already ends in it (case-insensitive). Original bug kept: a name shorter than
// four characters is compared from before its start.
static bool fix_up_script_filename(char* filename_out, unsigned int out_size, const char* filename)
{
    if (filename_out == NULL || filename == NULL)
    {
        return false;
    }
    const unsigned int length = (unsigned int)strlen(filename);
    if (v_stricmp(filename + (length - 4), ".lua") == 0)
    {
        if (length < out_size)
        {
            strncpy_safe(filename_out, filename, out_size);
            return true;
        }
    }
    else if (length + 4 < out_size)
    {
        _snprintf_safe(filename_out, out_size, "%s%s", filename, ".lua");
        return true;
    }
    return false;
}

// 0x00C2FF90 (:155)
// Reads the script through cfile into temp_mempool and runs it (chunk name: the name as given). Original bug kept: when
// the pool cannot hold the file, the file stays open and the pool's mark is not restored.
static bool load_script_internal(const char* filename, lua_State* lua_state, mempool_base* temp_mempool)
{
    if (filename == NULL || lua_state == NULL || temp_mempool == NULL)
    {
        return false;
    }
    char fixed_filename[64];
    if (!fix_up_script_filename(fixed_filename, sizeof(fixed_filename), filename))
    {
        return false;
    }
    cfile* p_cfile = cf_open(fixed_filename, "rb", PLATFORM_PC, false);
    if (p_cfile == NULL)
    {
        return false;
    }
    unsigned int size = cf_size(p_cfile);
    const unsigned int mark = temp_mempool->mark();
    char* buffer = (char*)temp_mempool->alloc(size, 0);
    if (buffer == NULL)
    {
        return false;
    }
    temp_mempool->lock();
    size = cf_read(buffer, size, p_cfile, false);
    cf_close(p_cfile);
    lua_setgcthreshold(lua_state, 0);
    const int status = lua_dobuffer(lua_state, buffer, size, filename);
    temp_mempool->unlock();
    temp_mempool->restore_to_mark(mark);
    return status == 0;
}

// 0x00C300B0 (:203)
static bool load_script_internal(const char* filename, unsigned char* data_buffer, unsigned int data_size, lua_State* lua_state)
{
    return lua_dobuffer(lua_state, (const char*)data_buffer, data_size, filename) == 0;
}

// 0x00C32690 (:241)
// Console command: prints _DynamicGlobals (the prints are empty in the release build).
void unused_dcf_dump_lua_globals()
{
    if (Console_command != 0 && Default_lua_state != NULL)
    {
        lua_debug_printf("_DynamicGlobals - ");
        lua_pushstring(Default_lua_state, "_DynamicGlobals");
        lua_gettable(Default_lua_state, LUA_GLOBALSINDEX);
        lua_print_var(Default_lua_state, lua_gettop(Default_lua_state), 0);
        lua_debug_printf("\n");
    }
}

// 0x00C300D0 (:265)
void (*lua_get_error_cb(lua_State* root_state))(lua_thread*)
{
    for (unsigned int i = 0; i < Num_lua_states; i++)
    {
        if (Lua_states[i].lua_state == root_state)
        {
            return Lua_states[i].error_cb;
        }
    }
    return NULL;
}

// 0x00C313E0 (:277)
// Prints a value (tables recursively, up to depth 100, skipping _G; four pairs per line).
void lua_print_var(lua_State* L, int idx, int depth)
{
    if (depth > 100)
    {
        return;
    }
    const int type = lua_type(L, idx);
    switch (type)
    {
    case LUA_TBOOLEAN:
        lua_debug_printf("%s", lua_toboolean(L, idx) ? "true" : "false");
        break;
    case LUA_TNUMBER:
        lua_debug_printf("%.02f", lua_tonumber(L, idx));
        break;
    case LUA_TSTRING:
        lua_debug_printf("\"%s\"", lua_tostring(L, idx));
        break;
    case LUA_TTABLE:
    {
        int count = 0;
        lua_debug_printf("{ ");
        lua_pushnil(L);
        while (lua_next(L, idx) != 0)
        {
            if (lua_type(L, lua_gettop(L) - 1) == LUA_TSTRING && strcmp(lua_tostring(L, lua_gettop(L) - 1), "_G") == 0)
            {
                lua_settop(L, -2);
            }
            else
            {
                if (lua_type(L, lua_gettop(L)) == LUA_TTABLE)
                {
                    lua_debug_printf("\n");
                    lua_debug_printf("    ");
                }
                lua_print_var(L, lua_gettop(L) - 1, 0);
                lua_debug_printf(" - ");
                lua_print_var(L, lua_gettop(L), depth + 1);
                lua_debug_printf(", ");
                lua_settop(L, -2);
                count++;
                if (count % 4 == 0)
                {
                    lua_debug_printf("\n");
                    lua_debug_printf("    ");
                }
            }
        }
        lua_debug_printf("}\n");
        lua_debug_printf("    ");
        break;
    }
    default:
        lua_debug_printf("%s", lua_typename(L, type));
        break;
    }
}

// 0x00C315F0 (:339)
// Prints the call stack with its locals, then the threads on the stack.
void lua_dump_state(lua_State* L, int depth)
{
    lua_Debug ar;
    lua_debug_printf("\n--------------- Stack Dump %d ----------------\n", depth);
    int level = 0;
    while (lua_getstack(L, level, &ar) == 1)
    {
        if (lua_getinfo(L, "Sln", &ar) == 1)
        {
            const char* name = ar.name;
            if (name == NULL)
            {
                lua_thread* t = lua_get_thread(L);
                name = t != NULL ? t->func_name : "?";
            }
            lua_debug_printf("%s: line %d - file %s\n", name, ar.currentline, ar.source);
        }
        int n = 1;
        const char* local_name = lua_getlocal(L, &ar, n);
        while (local_name != NULL)
        {
            n++;
            lua_debug_printf("  local %s = ", local_name);
            lua_print_var(L, 1, depth); // original: index 1, not the local just pushed
            lua_settop(L, -2);
            lua_debug_printf("\n");
            local_name = lua_getlocal(L, &ar, n);
        }
        level++;
    }
    for (int i = lua_gettop(L); i != 0; i--)
    {
        if (lua_type(L, i) == LUA_TTHREAD)
        {
            lua_dump_state(lua_tothread(L, i), depth + 1);
        }
    }
    lua_debug_printf("--------------- Stack Dump Finished %d ---------------\n", depth);
}

// 0x00C31740 (:401)
// Without all four callbacks Lua uses the CRT heap and the memory statistics report -1.
void lua_system_init(void* (*realloc_cb)(void*, unsigned int), void (*free_cb)(void*), int (*get_total_mem)(), int (*get_free_mem)())
{
    if (realloc_cb == NULL || free_cb == NULL || get_total_mem == NULL || get_free_mem == NULL)
    {
        Get_total_mem = NULL;
        Get_free_mem = NULL;
        free_cb = free;
        realloc_cb = realloc;
    }
    else
    {
        Get_total_mem = get_total_mem;
        Get_free_mem = get_free_mem;
    }
    Realloc_cb = realloc_cb;
    Free_cb = free_cb;
    lua_set_mem_calls(realloc_cb, free_cb);
    Num_lua_states = 0;
    for (int i = 0; i < Max_lua_states; i++)
    {
        Paused_lua_states[i] = NULL;
    }
    // lua_thread_system_init, inlined
    Thread_stack_depth = 0;
    Num_script_threads = 0;
    Next_thread_handle = 0;
}

// 0x00C30100 (:429)
// A full collection now (a threshold of 0 collects at once and resets it).
void lua_do_garbage_collection(lua_State* lua_state)
{
    if (lua_state != NULL)
    {
        lua_setgcthreshold(lua_state, 0);
    }
}

// 0x00C30120 (:444)
bool lua_get_global_boolean(char* name, lua_State* lua_state)
{
    if (lua_state != NULL)
    {
        lua_pushstring(lua_state, name);
        lua_rawget(lua_state, LUA_GLOBALSINDEX);
        if (lua_type(lua_state, -1) != LUA_TNIL)
        {
            const int value = lua_toboolean(lua_state, -1);
            lua_settop(lua_state, -2);
            return value != 0;
        }
        lua_settop(lua_state, -2);
    }
    return false;
}

// 0x00C317E0 (:470)
float lua_get_global_number(char* name, lua_State* lua_state)
{
    return (float)lua_get_global_number_large(name, lua_state);
}

// 0x00C30180 (:475)
double lua_get_global_number_large(char* name, lua_State* lua_state)
{
    if (lua_state != NULL)
    {
        lua_pushstring(lua_state, name);
        lua_rawget(lua_state, LUA_GLOBALSINDEX);
        if (lua_type(lua_state, -1) != LUA_TNIL)
        {
            const double value = lua_tonumber(lua_state, -1);
            lua_settop(lua_state, -2);
            return value;
        }
        lua_settop(lua_state, -2);
    }
    return 0.0;
}

// 0x00C301F0 (:501)
// Only an existing global is set.
void lua_set_global_boolean(char* name, bool value, lua_State* lua_state)
{
    if (lua_state != NULL)
    {
        lua_pushstring(lua_state, name);
        lua_rawget(lua_state, LUA_GLOBALSINDEX);
        const int type = lua_type(lua_state, -1);
        lua_settop(lua_state, -2);
        if (type != LUA_TNIL)
        {
            lua_pushstring(lua_state, name);
            lua_pushboolean(lua_state, value);
            lua_rawset(lua_state, LUA_GLOBALSINDEX);
        }
    }
}

// 0x00C30260 (:523)
void lua_set_global_number(char* name, float value, lua_State* lua_state)
{
    if (lua_state != NULL)
    {
        lua_pushstring(lua_state, name);
        lua_rawget(lua_state, LUA_GLOBALSINDEX);
        const int type = lua_type(lua_state, -1);
        lua_settop(lua_state, -2);
        if (type != LUA_TNIL)
        {
            lua_pushstring(lua_state, name);
            lua_pushnumber(lua_state, value);
            lua_rawset(lua_state, LUA_GLOBALSINDEX);
        }
    }
}

// 0x00C302D0 (:545)
void lua_set_global_string(char* name, const char* string, lua_State* lua_state)
{
    if (lua_state != NULL)
    {
        lua_pushstring(lua_state, name);
        lua_rawget(lua_state, LUA_GLOBALSINDEX);
        const int type = lua_type(lua_state, -1);
        lua_settop(lua_state, -2);
        if (type != LUA_TNIL)
        {
            lua_pushstring(lua_state, name);
            lua_pushstring(lua_state, string);
            lua_rawset(lua_state, LUA_GLOBALSINDEX);
        }
    }
}

// 0x00C31800 (:567)
// A dynamic script's globals are tracked by system_lib.lua (_PrepareForDynamicGlobals .. _DynamicGlobalsLoadComplete)
// so that lua_unload_dynamic_script can remove them. The chunk name is the name with ".lua".
bool lua_load_dynamic_script(const char* filename, lua_State* root_state, mempool_base* temp_mempool)
{
    if (root_state == NULL || filename == NULL || temp_mempool == NULL)
    {
        return false;
    }
    char fixed_filename[64];
    if (!fix_up_script_filename(fixed_filename, sizeof(fixed_filename), filename))
    {
        return false;
    }
    lua_pushstring(root_state, "_PrepareForDynamicGlobals");
    lua_rawget(root_state, LUA_GLOBALSINDEX);
    lua_pushstring(root_state, filename);
    lua_call(root_state, 1, 0);
    if (!load_script_internal(fixed_filename, root_state, temp_mempool))
    {
        return false;
    }
    lua_pushstring(root_state, "_DynamicGlobalsLoadComplete");
    lua_rawget(root_state, LUA_GLOBALSINDEX);
    lua_call(root_state, 0, 0);
    return true;
}

// 0x00C318D0 (:594)
bool lua_load_dynamic_script(const char* filename, unsigned char* data_buffer, unsigned int data_size, lua_State* root_state, mempool_base* _temp_pool)
{
    if (root_state == NULL || filename == NULL || _temp_pool == NULL)
    {
        return false;
    }
    char fixed_filename[64];
    if (!fix_up_script_filename(fixed_filename, sizeof(fixed_filename), filename))
    {
        return false;
    }
    lua_pushstring(root_state, "_PrepareForDynamicGlobals");
    lua_rawget(root_state, LUA_GLOBALSINDEX);
    lua_pushstring(root_state, filename);
    lua_call(root_state, 1, 0);
    if (lua_dobuffer(root_state, (const char*)data_buffer, data_size, fixed_filename) != 0)
    {
        return false;
    }
    lua_pushstring(root_state, "_DynamicGlobalsLoadComplete");
    lua_rawget(root_state, LUA_GLOBALSINDEX);
    lua_call(root_state, 0, 0);
    return true;
}

// 0x00C30330 (:621)
void lua_unload_dynamic_script(const char* filename, lua_State* root_state)
{
    if (filename != NULL && root_state != NULL)
    {
        lua_pushstring(root_state, "_UnloadDynamicGlobals");
        lua_rawget(root_state, LUA_GLOBALSINDEX);
        lua_pushstring(root_state, filename);
        lua_call(root_state, 1, 0);
        lua_setgcthreshold(root_state, 0);
    }
}

// 0x00C30380 (:637)
// The globals table gets a metatable whose __newindex (system_lib.lua's _CatchNilAssignment) rejects nil assignments,
// and whose __index looks the dynamic globals up.
void lua_disable_nil_assignments(lua_State* lua_state)
{
    if (lua_state != NULL)
    {
        lua_dostring(lua_state, "_UGGlobals = getfenv()");
        lua_dostring(lua_state, "setmetatable(_UGGlobals, {__index = _GetDynamicGlobal, __newindex = _CatchNilAssignment})");
        lua_dostring(lua_state, "setfenv(1, _UGGlobals)");
    }
}

// 0x00C303B0 (:647)
// As above with _CatchUndefinedGlobalWrite (no new globals), after a full collection.
void lua_lock_global_variables(lua_State* lua_state)
{
    if (lua_state != NULL)
    {
        lua_setgcthreshold(lua_state, 0);
        lua_dostring(lua_state, "_UGGlobals = getfenv()");
        lua_dostring(lua_state, "setmetatable(_UGGlobals, {__index = _GetDynamicGlobal, __newindex = _CatchUndefinedGlobalWrite})");
        lua_dostring(lua_state, "setfenv(1, _UGGlobals)");
    }
}

// 0x00C303F0 (:662)
bool lua_load_script(const char* filename, lua_State* lua_state, mempool_base* temp_mempool)
{
    return load_script_internal(filename, lua_state, temp_mempool);
}

// 0x00C30410 (:677)
bool lua_load_script(const char* filename, unsigned char* data_buffer, unsigned int data_size, lua_State* lua_state, mempool_base* temp_mempool)
{
    (void)temp_mempool;
    return load_script_internal(filename, data_buffer, data_size, lua_state);
}

// 0x00C319B0 (:694)
// Collects every state when the "free" figure is above 90% of the total. vint_rfg_system_init passes
// dl_get_allocated_bytes as get_free_mem, so in the game this is: more than 90% of the dlmalloc heap in use. The total
// goes through float, the product is a double.
void lua_do_frame()
{
    const int free_mem = lua_free_mem();
    const int total_mem = lua_total_mem();
    const int limit = (int)((float)total_mem * 0.9);
    if (limit > 0 && free_mem > limit)
    {
        for (unsigned int i = 0; i < Num_lua_states; i++)
        {
            lua_setgcthreshold(Lua_states[i].lua_state, 0);
        }
    }
}

// 0x00C32700 (:727)
// A root state with the base and string libraries (no math, table or debug library), the system natives and
// system_lib.lua.
lua_State* lua_create_state(bool is_default, mempool_base* temp_pool, void (*error_cb)(lua_thread*))
{
    (void)is_default;
    if (Num_lua_states >= (unsigned int)Max_lua_states || temp_pool == NULL)
    {
        return NULL;
    }
    lua_State* L = lua_open();
    if (L == NULL)
    {
        return NULL;
    }
    if (Num_lua_states == 0)
    {
        Default_lua_state = L;
    }
    Lua_states[Num_lua_states].lua_state = L;
    Lua_states[Num_lua_states].error_cb = error_cb;
    Num_lua_states++;
    luaopen_base(L);
    luaopen_string(L);
    lua_system_actions_register_functions(L);
    load_script_internal("system_lib.lua", L, temp_pool);
    return L;
}

// 0x00C31A20 (:760)
void lua_destroy_state(lua_State* lua_state)
{
    if (lua_state == NULL)
    {
        return;
    }
    for (int i = 0; i < Max_lua_states; i++)
    {
        if (Paused_lua_states[i] == lua_state)
        {
            Paused_lua_states[i] = NULL;
        }
    }
    lua_clear_threads(lua_state);
    for (unsigned int i = 0; i < Num_lua_states; i++)
    {
        if (Lua_states[i].lua_state == lua_state)
        {
            // Original: moves Num_lua_states - i entries (one past the last live one).
            memmove(&Lua_states[i], &Lua_states[i + 1], (Num_lua_states - i) * sizeof(lua_active_state));
            Num_lua_states--;
            break;
        }
    }
    lua_close(lua_state);
}

// 0x00C30440 (:786)
// Include scripts are not supported in the game: a critical failure.
void lua_add_include_script(const char* fname)
{
    (void)fname;
    VLIB_CRITICAL_FAILURE();
}

// 0x00C30460 (:818)
void* lua_realloc(void* addy, unsigned int bytes)
{
    if (Realloc_cb == NULL)
    {
        return NULL;
    }
    return Realloc_cb(addy, bytes);
}

// 0x00C30470 (:836)
void lua_free(void* addy)
{
    if (Free_cb != NULL)
    {
        Free_cb(addy);
    }
}

// 0x00C30480 (:852)
int lua_total_mem()
{
    if (Get_total_mem != NULL)
    {
        return Get_total_mem();
    }
    return -1;
}

// 0x00C30490 (:861)
int lua_free_mem()
{
    if (Get_free_mem != NULL)
    {
        return Get_free_mem();
    }
    return -1;
}

// 0x00C304A0 (:907)
// The debug prints are empty in the release build.
void lua_debug_printf_begin()
{
}

// 0x00C304B0 (:908)
void lua_debug_printf(const char*, ...)
{
}

// 0x00C304C0 (:909)
void lua_debug_printf_end()
{
}

// 0x00C304D0 (:913)
bool lua_root_state_is_paused(lua_State* state)
{
    for (int i = 0; i < Max_lua_states; i++)
    {
        if (Paused_lua_states[i] == state)
        {
            return true;
        }
    }
    return false;
}

// 0x00C30500 (:925)
// Pauses nest: each pause of a paused state adds one, an unpause removes one.
void lua_pause_root_state(lua_State* state, bool pause)
{
    unsigned int free_slot = 0xFFFFFFFF;
    for (unsigned int i = 0; i < (unsigned int)Max_lua_states; i++)
    {
        if (Paused_lua_states[i] == NULL)
        {
            free_slot = i;
        }
        if (Paused_lua_states[i] == state)
        {
            if (pause)
            {
                Paused_lua_state_count[i]++;
            }
            else if (Paused_lua_state_count[i] == 1)
            {
                Paused_lua_states[i] = NULL;
            }
            else
            {
                Paused_lua_state_count[i]--;
            }
            return;
        }
    }
    if (free_slot != 0xFFFFFFFF && pause)
    {
        Paused_lua_states[free_slot] = state;
        Paused_lua_state_count[free_slot] = 1;
    }
}
