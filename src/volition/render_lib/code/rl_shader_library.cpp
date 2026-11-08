// Original: volition/render_lib/code/rl_shader_library.cpp
//
// rl_shader_library: the registry of the loaded shaders. Each shader file is registered under its base name (the
// name without the type suffix, e.g. "_s" for standard); the entry keeps one shader per type and configuration. A
// shader handle is the entry index.

#include "volition/render_lib/code/rl_shader_library.h"

#include "volition/render_lib/code/rl_shader.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"
#include "keen/base/trace.hpp"

#include <string.h>
#include <windows.h>

// size 0xC
struct shader_type_lookup
{
    const char* ext; // 0x00
    rl_shader_type type; // 0x04
    int flags; // 0x08
};

// The shader file name suffixes: the shader type, and the configuration (bit 0 "b", bit 1 "m").
static shader_type_lookup Shader_type_lookups[10] = {
    {"_s", shader_type_standard, 0},
    {"_bs", shader_type_standard, 1},
    {"_ms", shader_type_standard, 2},
    {"_bms", shader_type_standard, 3},
    {"_c", shader_type_character, 0},
    {"_mc", shader_type_character, 2},
    {"_bc", shader_type_character, 1},
    {"_bmc", shader_type_character, 3},
    {"_t", shader_type_terrain, 0},
    {"_ts", shader_type_terrain, 2},
}; // 0x014FA490
static int Num_shader_type_lookups = 10; // 0x014FA48C

// The maximum number of shader files loaded by load_shaders (cf_find_ext).
static const unsigned int Max_shader_files = 0x200;

// 0x004F62D0 (:48)
void unused_dcf_reload_shaders()
{
    rl_shader_library* library_p = rl_shader_library::get_instance();
    cf_refresh_file_size_ext(library_p->get_shader_file_extension());
    library_p->shutdown();
    library_p->initialize();
}

// 0x005462A0 (:63)
// Loads and registers every shader file with the extension.
// Notes (decompiled behaviour, kept as is): the depth-only shader is resolved from "rl_debug_highlight" (the same name
// as the highlight shader).
bool rl_shader_library::load_shaders(const char* ext)
{
    const char* files[Max_shader_files];
    char shader_name[256];
    const unsigned int num_files = cf_find_ext(files, Max_shader_files, ext);
    for (int i = 0; i < (int)num_files; i++)
    {
        cf_drop_path(shader_name, sizeof(shader_name), files[i]);
        rl_shader* shader_p = rl_shader::create();
        if (shader_p == nullptr)
        {
            return false;
        }
        if (shader_p->load_shader(shader_name))
        {
            register_shader(shader_name, shader_p, false);
        }
        else
        {
            shader_p->release();
            VLIB_CRITICAL_FAILURE();
        }
    }
    m_depth_only_shader = lookup("rl_debug_highlight");
    return true;
}

// 0x004C8280 (:110)
// The shader of the entry's own type and configuration (the one its name was registered with first).
rl_shader* rl_shader_library::get_reference_shader(unsigned int handle)
{
    if ((int)handle < 0 || num_library_entries <= (int)handle)
    {
        return nullptr;
    }
    const rl_shaderlib_entry& entry = library_entries[handle];
    if (entry.lookup_type < 0 || entry.lookup_type >= num_rl_shader_types || entry.lookup_configuration < 0 || entry.lookup_configuration >= 4)
    {
        return nullptr;
    }
    return entry.types[entry.lookup_type].configurations[entry.lookup_configuration];
}

// 0x005183C0 (:125)
rl_shader_library::rl_shader_library()
    : library_entry_hash(false)
{
    m_highlight_shader_to_use = 0xFFFFFFFF;
    m_depth_only_shader = 0xFFFFFFFF;
}

// 0x004F6310 (:131)
rl_shader_library::~rl_shader_library()
{
}

// 0x00546470 (:137)
void rl_shader_library::initialize()
{
    library_entry_hash.clear();
    num_library_entries = 0;
    for (int i = 0; i < 256; i++)
    {
        library_entries[i].init();
    }
    if (load_shaders(get_shader_file_extension()))
    {
        m_highlight_shader_to_use = lookup("rl_debug_highlight");
    }
}

// 0x004C82D0 (:156)
void rl_shader_library::shutdown()
{
    for (int i = 0; i < 256; i++)
    {
        for (int type = 0; type < 4; type++)
        {
            for (int configuration = 0; configuration < 4; configuration++)
            {
                rl_shader*& shader_p = library_entries[i].types[type].configurations[configuration];
                if (shader_p != nullptr)
                {
                    shader_p->release();
                    shader_p = nullptr;
                }
            }
        }
    }
}

// 0x00518480 (:179)
// Adds the shader under its base name, type and configuration; an occupied slot is replaced only with
// overrideExisting.
// Notes (decompiled behaviour, kept as is): the error messages are formatted without their arguments and not shown.
bool rl_shader_library::register_shader(const char* shader_name, rl_shader* shader, bool overrideExisting)
{
    char name[256];
    char message[256];
    rl_shader_type type = shader_type_generic;
    int configuration = 0;
    get_shader_name_and_type(shader_name, name, sizeof(name), type, configuration);
    const unsigned int key = crc_stri(name, 0);
    if (type == shader_type_invalid)
    {
        _snprintf_safe(message, sizeof(message), "Shader %s does not have a valid type.  Cannot register with renderlib.");
        return false;
    }
    rl_shaderlib_entry* entry_p = nullptr;
    library_entry_hash.get(key, entry_p);
    if (entry_p == nullptr)
    {
        // Magic number: MAX_SHADER_LIBARARY_ENTRIES.
        if (num_library_entries == 256)
        {
            _snprintf_safe(message, sizeof(message), "Shader library contains %d top-level entries which is the max.  MAX_SHADER_LIBARARY_ENTRIES must be increased.");
            return false;
        }
        entry_p = &library_entries[num_library_entries];
        num_library_entries++;
        entry_p->init();
        entry_p->lookup_configuration = configuration;
        entry_p->lookup_type = type;
        library_entry_hash.insert_key_data(key, entry_p);
    }
    if (overrideExisting || entry_p->types[type].configurations[configuration] == nullptr)
    {
        entry_p->types[type].configurations[configuration] = shader;
        return true;
    }
    return false;
}

// 0x00518630 (:257)
unsigned int rl_shader_library::lookup(unsigned int name_hash)
{
    rl_shaderlib_entry* entry_p = nullptr;
    if (library_entry_hash.get(name_hash, entry_p) && entry_p != nullptr)
    {
        return (unsigned int)(entry_p - library_entries);
    }
    return 0xFFFFFFFF;
}

// 0x00518680 (:273)
unsigned int rl_shader_library::lookup(const char* name)
{
    rl_shaderlib_entry* entry_p = nullptr;
    if (library_entry_hash.get(crc_stri(name, 0), entry_p) && entry_p != nullptr)
    {
        return (unsigned int)(entry_p - library_entries);
    }
    return 0xFFFFFFFF;
}

// 0x004C8320 (:286)
rl_shader* rl_shader_library::get_rl_shader(unsigned int handle, rl_shader_type type, int flags)
{
    if ((unsigned int)type < num_rl_shader_types && (unsigned int)flags < 4 && handle < (unsigned int)num_library_entries)
    {
        return library_entries[handle].types[type].configurations[flags];
    }
    return nullptr;
}

// 0x004C8360 (:295)
bool rl_shader_library::does_shader_exist(unsigned int handle, rl_shader_type type, int flags)
{
    if ((unsigned int)type < num_rl_shader_types && (unsigned int)flags < 4 && handle < (unsigned int)num_library_entries)
    {
        return library_entries[handle].types[type].configurations[flags] != nullptr;
    }
    return false;
}

// 0x004C83A0 (:322)
int rl_shader_library::get_shader_tmu_index(unsigned int handle, int tex_name)
{
    rl_shader* shader_p = get_reference_shader(handle);
    if (shader_p == nullptr)
    {
        return -1;
    }
    return shader_p->get_tmu_index(tex_name);
}

// 0x004C8430 (:338)
int rl_shader_library::get_shader_const_index(unsigned int handle, int const_name)
{
    rl_shader* shader_p = get_reference_shader(handle);
    if (shader_p == nullptr)
    {
        return -1;
    }
    return shader_p->get_const_index(const_name);
}

// 0x004C84B0 (:354)
int rl_shader_library::get_shader_bool_index(unsigned int handle, int bool_name)
{
    rl_shader* shader_p = get_reference_shader(handle);
    if (shader_p == nullptr)
    {
        return -1;
    }
    return shader_p->get_bool_const_index(bool_name);
}

// 0x004C8530 (:369)
int rl_shader_library::get_shader_num_consts(unsigned int handle)
{
    rl_shader* shader_p = get_reference_shader(handle);
    if (shader_p == nullptr)
    {
        return -1;
    }
    return shader_p->get_num_consts();
}

// 0x004C8590 (:385)
int rl_shader_library::get_shader_user_technique_index(unsigned int handle, unsigned int technique_name_crc)
{
    rl_shader* shader_p = get_reference_shader(handle);
    if (shader_p == nullptr)
    {
        return -1;
    }
    return shader_p->get_user_technique_index(technique_name_crc);
}

// 0x004C8620 (:403)
// Splits a shader file name into the base name and the type/configuration of its suffix (generic/0 without one).
// Notes (decompiled behaviour, kept as is): max_size is not used to bound the copy; a suffix longer than the name is
// compared before the start of the name.
void rl_shader_library::get_shader_name_and_type(const char* shader_name, char* name_out, int, rl_shader_type& type_out, int& flags_out)
{
    char name[512];
    cf_drop_path(name, sizeof(name), shader_name);
    cf_drop_ext(name, sizeof(name), name);
    size_t name_length = strlen(name);
    type_out = shader_type_generic;
    flags_out = 0;
    for (int i = 0; i < Num_shader_type_lookups; i++)
    {
        const size_t ext_length = strlen(Shader_type_lookups[i].ext);
        if (v_stricmp(name + (name_length - ext_length), Shader_type_lookups[i].ext) == 0)
        {
            type_out = Shader_type_lookups[i].type;
            flags_out = Shader_type_lookups[i].flags;
            name_length -= ext_length;
            break;
        }
    }
    strncpy(name_out, name, name_length);
    name_out[name_length] = '\0';
}

// 0x005186E0 (:440)
// Not counted in a class dynamic info.
rl_shader_library* rl_shader_library::create()
{
    rl_shader_library* library_p = new rl_shader_library();
    if (library_p == nullptr)
    {
        // OpenRFG deviation: the message box is replaced by a log line.
        // MessageBoxA(nullptr, "Failed to initialize shaders.", "Error", 0);
        keen::debug::nativePrintArgs("[error] Failed to initialize shaders.\n");
        ExitProcess(0);
    }
    return library_p;
}

// 0x004C8740 (:454)
bool rl_shader_library::can_create()
{
    return true;
}

// 0x004C8750 (:459)
void rl_shader_library::release()
{
    if (this != nullptr)
    {
        delete this;
    }
}

// 0x00518420 (.h:137)
const char* const rl_shader_library::get_shader_file_extension() const
{
    return ".fxo_kg";
}

// 0x004B6D60 (.h:149)
unsigned int rl_shader_library::get_depth_only_shader() const
{
    return m_depth_only_shader;
}

// 0x004B6D10 (.h:70)
void rl_shaderlib_container::init()
{
    configurations[0] = nullptr;
    configurations[1] = nullptr;
    configurations[2] = nullptr;
    configurations[3] = nullptr;
}

// 0x004B6D20 (.h:92)
void rl_shaderlib_entry::init()
{
    for (int i = 0; i < 4; i++)
    {
        types[i].init();
    }
    lookup_type = shader_type_invalid;
    lookup_configuration = -1;
}
