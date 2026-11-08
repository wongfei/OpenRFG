// Original: volition/vlib/code/util/xml_util.cpp
//
// Partial: the int, float, vector and vector4 readers (the time of day tables read their keys with them). The other
// readers and the writers are not implemented yet.

#include "volition/vlib/code/util/xml_util.h"

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

#include "keen/base/string.hpp"

#include <stdlib.h>
#include <string.h>

// The child of parent named tag_name (case-insensitive), or null.
static const xml_element* find_child(const xml_element* parent, const char* tag_name)
{
    if (parent == nullptr)
    {
        return nullptr;
    }
    for (const xml_element* e = parent->elements; e != nullptr; e = e->next)
    {
        if (keen::compareStringNoCase(e->name, tag_name) == 0)
        {
            return e;
        }
    }
    return nullptr;
}

// 0x005B27F0 (:100): a missing element gives the default (and true); an empty one gives false.
bool read_xml_int(const xml_element* parent, const char* tag_name, int& val, int default_val)
{
    const xml_element* e = find_child(parent, tag_name != nullptr ? tag_name : "int");
    if (e == nullptr)
    {
        val = default_val;
        return true;
    }
    return read_xml_int(e, val);
}

// 0x005942F0 (:114): decimal, or hex/octal with a 0x/0 prefix (strtol base 0).
bool read_xml_int(const xml_element* e, int& val)
{
    if (e->text == nullptr)
    {
        return false;
    }
    val = strtol(e->text, nullptr, 0);
    return true;
}

// 0x005B2860 (:172)
bool read_xml_float(const xml_element* parent, const char* tag_name, float& val, float default_val)
{
    const xml_element* e = find_child(parent, tag_name != nullptr ? tag_name : "flt");
    if (e == nullptr)
    {
        val = default_val;
        return true;
    }
    return read_xml_float(e, val);
}

// 0x00594350 (:186)
bool read_xml_float(const xml_element* e, float& val)
{
    if (e->text == nullptr)
    {
        return false;
    }
    val = (float)atof(e->text);
    return true;
}

// 0x005B2940 (:328)
bool read_xml_vector(const xml_element* parent, const char* tag_name, vector& val, const vector& default_val)
{
    const xml_element* e = find_child(parent, tag_name != nullptr ? tag_name : "vec");
    if (e == nullptr)
    {
        val.x = default_val.x;
        val.y = default_val.y;
        val.z = default_val.z;
        return true;
    }
    return read_xml_vector(e, val);
}

// 0x005944B0 (:342): whitespace-separated components, parsed from a 1 KB copy of the text (strtok). A missing
// component is atof(nullptr) in the original; the texts always have all of them.
bool read_xml_vector(const xml_element* e, vector& val)
{
    if (e->text == nullptr)
    {
        return false;
    }
    char buffer[1024];
    strcpy(buffer, e->text);
    val.x = (float)atof(strtok(buffer, "\r\n\t "));
    val.y = (float)atof(strtok(nullptr, "\r\n\t "));
    val.z = (float)atof(strtok(nullptr, "\r\n\t "));
    return true;
}

// 0x005B2A10 (:402)
bool read_xml_vector4(const xml_element* parent, const char* tag_name, vector4& val, const vector4& default_val)
{
    const xml_element* e = find_child(parent, tag_name != nullptr ? tag_name : "vec");
    if (e == nullptr)
    {
        val.x = default_val.x;
        val.y = default_val.y;
        val.z = default_val.z;
        val.w = default_val.w;
        return true;
    }
    return read_xml_vector4(e, val);
}

// 0x00594630 (:416)
bool read_xml_vector4(const xml_element* e, vector4& val)
{
    if (e->text == nullptr)
    {
        return false;
    }
    char buffer[1024];
    strcpy(buffer, e->text);
    val.x = (float)atof(strtok(buffer, "\r\n\t "));
    val.y = (float)atof(strtok(nullptr, "\r\n\t "));
    val.z = (float)atof(strtok(nullptr, "\r\n\t "));
    val.w = (float)atof(strtok(nullptr, "\r\n\t "));
    return true;
}
