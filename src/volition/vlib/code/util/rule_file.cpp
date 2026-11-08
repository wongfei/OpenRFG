// Original: volition/vlib/code/util/rule_file.cpp
//
// Partial: slw_get_textures (stub_impl.md S3, the always-loaded tables). The rule writers (rule_open ... rule_close,
// used by the tools that build the rule files) are not ported.

#include "volition/vlib/code/util/rule_file.h"

#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/fastxml.h"

#include "keen/base/string.hpp"

#include <string.h>

// The extension of a path ("" when there is none, or when the last '.' is in a directory name).
static const char* rule_get_ext(const char* filename)
{
    const char* ext = strrchr(filename, '.');
    if (ext == nullptr || strchr(ext, '/') != nullptr || strchr(ext, '\\') != nullptr)
    {
        return "";
    }
    return ext;
}

// 0x005CD770 (:288)
// Adds every source of the rule file's <rule><platform> block for plat to dst (data 0), except the files with the rule
// file's own extension or the platform's GPU texture extension; strip_extensions drops the extensions first.
bool slw_get_textures(const char* filename, str_data_hash_table<unsigned char,1024,64,unsigned short>& dst, vlib_platform plat, bool strip_extensions)
{
    const unsigned int mark = Vlib_temp_mempool->mark();
    // original: the early returns below leave the temp pool's mark in place (the parsed file is not released)
    xml_element* element = xml_parse(filename, Vlib_temp_mempool);
    if (element == nullptr)
    {
        return false;
    }
    element = element->elements;
    while (true)
    {
        if (element == nullptr)
        {
            return false;
        }
        if (keen::compareStringNoCase(element->name, "rule") == 0)
        {
            break;
        }
        element = element->next;
    }
    element = xml_find(element, "platform");
    if (element == nullptr)
    {
        return false;
    }
    while (keen::compareStringNoCase(element->text, Vlib_platforms[plat]) != 0)
    {
        do
        {
            element = element->next;
            if (element == nullptr)
            {
                return false;
            }
        } while (keen::compareStringNoCase(element->name, "platform") != 0);
    }
    const char* rule_ext = rule_get_ext(filename);
    for (element = element->elements; element != nullptr; element = element->next)
    {
        if (keen::compareStringNoCase(element->name, "source") != 0)
        {
            continue;
        }
        // from the first source on, only the following "source" siblings are visited
        while (true)
        {
            const char* ext = rule_get_ext(element->text);
            if (keen::compareStringNoCase(ext, rule_ext) != 0 && keen::compareStringNoCase(ext, Vbm_gpu_platform_ext[plat]) != 0)
            {
                const unsigned char data = 0;
                if (strip_extensions)
                {
                    char name[64];
                    cf_drop_ext(name, sizeof(name), element->text);
                    dst.insert_key_data(name, data);
                }
                else
                {
                    dst.insert_key_data(element->text, data);
                }
            }
            do
            {
                element = element->next;
                if (element == nullptr)
                {
                    Vlib_temp_mempool->restore_to_mark(mark);
                    return true;
                }
            } while (keen::compareStringNoCase(element->name, "source") != 0);
        }
    }
    Vlib_temp_mempool->restore_to_mark(mark);
    return true;
}
