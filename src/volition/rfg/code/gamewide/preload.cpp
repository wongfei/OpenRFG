// Original: volition/rfg/code/gamewide/preload.cpp
//
// Preloading (stub_impl.md S5): subsystems register a loader per file extension; preload_all walks the animation
// tables of the base game or a DLC bundle and hands every file marked Preload to the loader of its extension.

#include "volition/rfg/code/gamewide/preload.h"

#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>
#include <string.h>

// size 0x10
struct registered_loader
{
    char extension[10]; // 0x00, without the dot
    void (*load)(const char*); // 0x0C
};

static registered_loader Registered_loaders[16]; // 0x025112A0
static int Num_registered_loaders; // 0x0253392C

static void maybe_preload_file(xml_element* root, const char* filename); // 0x007C5A40 (:77)
static void find_preloaded_files(xml_element* root, const char* filename); // 0x007C5AF0 (:120)

// 0x007C59A0 (:56)
// The loader index of an extension (with or without its dot), or -1.
int get_ext_index(const char* in_ext)
{
    char ext[64];
    strcpy(ext, in_ext[0] == '.' ? in_ext + 1 : in_ext);
    for (int i = 0; i < Num_registered_loaders; i++)
    {
        if (v_stricmp(Registered_loaders[i].extension, ext) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x007C5A40 (:77)
// LTCG: root in EAX. An element with Preload True: its Filename to the loader of its extension (an .animx file is
// loaded by its .anim name).
static void maybe_preload_file(xml_element* root, const char* filename)
{
    if (root == nullptr)
    {
        return;
    }
    char* name = xml_get_string(xml_find(root, "Filename"), nullptr);
    if (name == nullptr)
    {
        return;
    }
    char* preload = xml_get_string(xml_find(root, "Preload"), nullptr);
    if (preload == nullptr || v_stricmp(preload, "True") != 0)
    {
        return;
    }
    const char* ext = cf_get_ext(name);
    if (v_stricmp(ext, ".animx") == 0)
    {
        name[strlen(name) - 1] = '\0'; // in place: ext is now ".anim"
    }
    const int index = get_ext_index(ext);
    if (index >= 0)
    {
        Registered_loaders[index].load(name);
    }
}

// 0x007C5AF0 (:120)
// The element's children: a Filename child makes the element an entry, other children are searched.
static void find_preloaded_files(xml_element* root, const char* filename)
{
    if (root == nullptr)
    {
        return;
    }
    xml_element* e = root->elements;
    while (e != nullptr)
    {
        if (v_stricmp(e->name, "Filename") == 0)
        {
            maybe_preload_file(root, filename); // inlined in the binary
        }
        else
        {
            find_preloaded_files(e, filename);
        }
        e = e->next;
        if (e == root->elements)
        {
            break;
        }
    }
}

// 0x007C5BF0 (:150)
// The animation files table (the DLC bundle's own with a bundle id) and the blend trees table, each parsed into the
// common pool and released after its walk.
void preload_all(int dlc_id)
{
    char anim_files[256];
    const char* files[2] = {anim_files, "data\\tables\\anim_blend_trees.xtbl"};
    const unsigned int mark = Common_mempool.mark();
    if (dlc_id == -1)
    {
        sprintf(anim_files, "data\\tables\\anim_files.xtbl");
    }
    else
    {
        sprintf(anim_files, "data\\dlc\\dlc%02d\\tables\\dlc%02d_anim_files.xtbl", dlc_id, dlc_id);
    }
    for (int i = 0; i < 2; i++)
    {
        const char* filename = files[i];
        xml_element* root = xml_parse(filename, &Common_mempool);
        if (root != nullptr)
        {
            xml_element* e = root->elements;
            while (e != nullptr)
            {
                if (v_stricmp(e->name, "Filename") == 0)
                {
                    maybe_preload_file(root, filename);
                }
                else
                {
                    find_preloaded_files(e, filename);
                }
                e = e->next;
                if (e == root->elements)
                {
                    break;
                }
            }
        }
        Common_mempool.restore_to_mark(mark);
    }
}

// 0x007C5CF0 (:186)
// A loader for an extension (with or without its dot); a registered extension gets the new loader.
void preload_register_extension(const char* in_ext, void (*loader)(const char*))
{
    char ext[64];
    strcpy(ext, in_ext[0] == '.' ? in_ext + 1 : in_ext);
    const int index = get_ext_index(ext);
    if (index >= 0)
    {
        Registered_loaders[index].load = loader;
        return;
    }
    // Original bug kept: 11 characters into the 10-character extension (it spills into the padding).
    strncpy_safe(Registered_loaders[Num_registered_loaders].extension, ext, 11);
    Registered_loaders[Num_registered_loaders].load = loader;
    Num_registered_loaders++;
}
