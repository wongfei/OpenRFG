// Original: volition/vlib/code/parse/xml_table_tools.cpp
//
// .xtbl readers (see xml_table_tools.h). Error messages are formatted into a scratch string and printed; the getters
// carry on (a _req getter that fails leaves its output untouched).
//
// Notes (decompiled behaviour, kept as is):
// - parse_uint accepts decimal or 0x-prefixed hex and stops at the first other character; parse_float builds the
//   value from the integer part and the decimal digits after '.' (no exponent, no hex).
// - xml_table_get_int_list, _float_list and _string_list do not check list_max.
// - xml_table_get_flags treats a null entry in the flag list as the end of the list, without a warning for the name.

#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static page_pool* Xml_table_page_pool; // 0x01CDA5B8, xml_set_page_pool
static virtual_mempool* Xml_table_mempool; // 0x01CE87CC, pool of the table being parsed (xml_table_parse)
static xml_element* Last_parsed_root; // 0x01CE87D0, <Table> element of the last parse

// The original formats into a thread-local scratch string (_snprintf_safe, 0x400 bytes) and prints it.
static void xml_table_log(const char* fmt, ...)
{
    char message[0x400];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    message[sizeof(message) - 1] = '\0';
    keen::debug::nativePrintArgs("%s", message);
}

// First child named name (case-insensitive), or nullptr.
static xml_element* xml_table_find_child(xml_element* root, const char* name)
{
    if (root != nullptr)
    {
        for (xml_element* child = root->elements; child != nullptr; child = child->next)
        {
            if (keen::compareStringNoCase(child->name, name) == 0)
            {
                return child;
            }
        }
    }
    return nullptr;
}

// 0x0058E530 (:23)
void xml_set_page_pool(page_pool* pPool)
{
    Xml_table_page_pool = pPool;
}

// 0x005B1110 (:64)
bool parse_bool(const char* buf, unsigned int buf_size)
{
    // "true"/"yes" or "false"/"no" (case-insensitive, compared over buf_size characters).
    if (keen::compareStringNoCase(buf, "true", buf_size) == 0 || keen::compareStringNoCase(buf, "yes", buf_size) == 0)
    {
        return true;
    }
    if (keen::compareStringNoCase(buf, "false", buf_size) != 0 && keen::compareStringNoCase(buf, "no", buf_size) != 0)
    {
        xml_table_log("\"%s\" is not a Boolean value.", buf);
    }
    return false;
}

// 0x005B11B0 (:94)
unsigned int parse_uint(char* buf, int buf_size)
{
    static const char* const hex_digits = "0123456789abcdefABCDEF";
    buf[buf_size - 1] = '\0';
    int num_digits = 0;
    unsigned int value = 0;
    if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X'))
    {
        for (const char* p = buf + 2; *p != '\0' && strchr(hex_digits, *p) != nullptr; ++p)
        {
            const char c = *p;
            if (c >= '0' && c <= '9')
            {
                value = value * 16 + (c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                value = value * 16 + (c - 'a' + 10);
            }
            else
            {
                value = value * 16 + (c - 'A' + 10);
            }
            ++num_digits;
        }
    }
    else
    {
        for (const char* p = buf; *p >= '0' && *p <= '9'; ++p)
        {
            value = value * 10 + (*p - '0');
            ++num_digits;
        }
    }
    if (num_digits == 0)
    {
        xml_table_log("Could not parse \"%s\" into an integer.", buf);
    }
    return value;
}

// 0x005B12A0 (:147)
int parse_int(char* buf, int buf_size)
{
    buf[buf_size - 1] = '\0';
    const bool negative = buf[0] == '-';
    if (negative)
    {
        ++buf;
        --buf_size;
    }
    const unsigned int value = parse_uint(buf, buf_size);
    return negative ? -(int)value : (int)value;
}

// 0x005B12D0 (:185)
float parse_float(char* buf, int buf_size)
{
    buf[buf_size - 1] = '\0';
    const bool negative = buf[0] == '-';
    char* p = negative ? buf + 1 : buf;
    int whole;
    if (*p == '.')
    {
        whole = 0;
    }
    else
    {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        {
            xml_table_log("Could not convert \"%s\" to a float.", buf);
            return 0.0f;
        }
        whole = parse_int(p, negative ? buf_size - 1 : buf_size);
        p = strchr(buf, '.');
        if (p == nullptr)
        {
            return (float)(negative ? -whole : whole);
        }
    }
    float fraction = 0.0f;
    float scale = 0.1f;
    for (const char* digit = p + 1; *digit >= '0' && *digit <= '9'; ++digit)
    {
        fraction += (float)(*digit - '0') * scale;
        scale *= 0.1f;
    }
    if (negative)
    {
        fraction = -fraction;
        whole = -whole;
    }
    return (float)whole + fraction;
}

// 0x005CFB60 (:277)
xml_element* xml_table_parse(const char* filename)
{
    // The table gets its own virtual_mempool (released by xml_table_close); returns the <Table> element.
    Xml_table_mempool = Xml_table_page_pool->get_virtual_mempool(nullptr, 0);
    xml_element* root = xml_parse(filename, Xml_table_mempool);
    if (root == nullptr)
    {
        xml_table_log("The table file \"%s\" is missing or invalid.", filename);
        return nullptr;
    }
    Last_parsed_root = xml_table_find_req(root, "Table");
    return Last_parsed_root;
}

// 0x005C5A60 (:328)
xml_element* xml_table_parse_from_memory(unsigned char* file_loc)
{
    Xml_table_mempool = Xml_table_page_pool->get_virtual_mempool(nullptr, 0);
    xml_element* root = xml_parse_from_string((char*)file_loc, Xml_table_mempool, nullptr);
    if (root == nullptr)
    {
        xml_table_log("The table file is missing or invalid.");
        return nullptr;
    }
    Last_parsed_root = xml_table_find_req(root, "Table");
    return Last_parsed_root;
}

// 0x005B1480 (:376)
void xml_table_close(xml_element* root)
{
    Xml_table_page_pool->release_virtual_mempool(Xml_table_mempool);
    Xml_table_mempool = nullptr;
}

// 0x005BE380 (:408)
xml_element* xml_table_find_req(xml_element* root, const char* attribute_name)
{
    xml_element* element = xml_table_find_child(root, attribute_name);
    if (element == nullptr)
    {
        xml_table_log("Could not find required attribute %s:%s.", root != nullptr ? root->name : "", attribute_name);
    }
    return element;
}

// 0x005BE410 (:433)
xml_element* xml_table_find_opt(xml_element* root, const char* attribute_name)
{
    return xml_table_find_child(root, attribute_name);
}

// 0x005BE450 (:453)
xml_element* xml_table_find_next(xml_element* root, xml_element* previous, const char* attribute_name)
{
    if (root != nullptr && previous != nullptr)
    {
        for (previous = previous->next; previous != nullptr; previous = previous->next)
        {
            if (keen::compareStringNoCase(previous->name, attribute_name) == 0)
            {
                return previous;
            }
        }
    }
    return nullptr;
}

// 0x005BE490 (:465)
int xml_table_count(xml_element* root, const char* name)
{
    int count = 0;
    if (root != nullptr)
    {
        for (xml_element* child = root->elements; child != nullptr; child = child->next)
        {
            if (keen::compareStringNoCase(child->name, name) == 0)
            {
                ++count;
            }
        }
    }
    return count;
}

// 0x005BE4A0 (:482)
char* xml_table_get_req_string_ref(xml_element* root, const char* attribute_name)
{
    // With a null attribute_name the text of root itself.
    if (root == nullptr)
    {
        xml_table_log("Specified root does not exist.");
    }
    xml_element* element = root;
    if (attribute_name != nullptr)
    {
        element = xml_table_find_req(root, attribute_name);
        if (element == nullptr)
        {
            return nullptr;
        }
    }
    if (element->text != nullptr)
    {
        return element->text;
    }
    xml_table_log("Attribute \"%s\" contains no text.", attribute_name != nullptr ? attribute_name : element->name);
    return nullptr;
}

// 0x005BE550 (:535)
char* xml_table_get_opt_string_ref(xml_element* root, const char* attribute_name)
{
    if (root == nullptr)
    {
        return nullptr;
    }
    if (attribute_name == nullptr)
    {
        return root->text;
    }
    xml_element* element = xml_table_find_opt(root, attribute_name);
    return element != nullptr ? element->text : nullptr;
}

// 0x005BE580 (:572)
void xml_table_get_req_string(char* buf, int buf_size, xml_element* root, const char* attribute_name)
{
    if (root == nullptr)
    {
        xml_table_log("Specified root does not exist.");
    }
    xml_element* element = root;
    if (attribute_name != nullptr)
    {
        element = xml_table_find_req(root, attribute_name);
        if (element == nullptr)
        {
            xml_table_log("Attribute \"%s\" not found.", attribute_name);
            xml_table_log("Attribute \"%s\" could not be found.", attribute_name);
            return;
        }
    }
    if (element->text == nullptr)
    {
        xml_table_log("Attribute \"%s\" contains no text.", attribute_name != nullptr ? attribute_name : element->name);
        xml_table_log("Attribute \"%s\" could not be found.", attribute_name != nullptr ? attribute_name : element->name);
        return;
    }
    strncpy(buf, element->text, buf_size);
    buf[buf_size - 1] = '\0';
}

// 0x005BE680 (:631)
bool xml_table_get_opt_string(char* buf, int buf_size, xml_element* root, const char* attribute_name)
{
    if (root == nullptr)
    {
        return false;
    }
    if (attribute_name != nullptr)
    {
        root = xml_table_find_opt(root, attribute_name);
        if (root == nullptr)
        {
            return false;
        }
    }
    if (root->text == nullptr)
    {
        return false;
    }
    strncpy(buf, root->text, buf_size);
    buf[buf_size - 1] = '\0';
    return true;
}

// Text of root or its child attribute_name copied into buf (the _opt getters, inlined in the original).
static bool xml_table_copy_opt_text(char* buf, int buf_size, xml_element* root, const char* attribute_name)
{
    if (root == nullptr)
    {
        return false;
    }
    if (attribute_name != nullptr)
    {
        root = xml_table_find_opt(root, attribute_name);
        if (root == nullptr)
        {
            return false;
        }
    }
    if (root->text == nullptr)
    {
        return false;
    }
    strncpy(buf, root->text, buf_size);
    buf[buf_size - 1] = '\0';
    return true;
}

// 0x005BE6D0 (:667)
void xml_table_get_req_int(int& value, xml_element* root, const char* attribute_name)
{
    char buf[0x400];
    xml_table_get_req_string(buf, sizeof(buf), root, attribute_name);
    value = parse_int(buf, sizeof(buf));
}

// 0x005BE760 (:688)
bool xml_table_get_opt_int(int& value, xml_element* root, const char* attribute_name)
{
    char buf[0x400];
    if (!xml_table_copy_opt_text(buf, sizeof(buf), root, attribute_name))
    {
        return false;
    }
    value = parse_int(buf, sizeof(buf));
    return true;
}

// 0x005BE830 (:712)
void xml_table_get_req_uint(unsigned int& value, xml_element* root, const char* attribute_name)
{
    char buf[0x400];
    xml_table_get_req_string(buf, sizeof(buf), root, attribute_name);
    value = parse_uint(buf, sizeof(buf));
}

// 0x005BE8A0 (:733)
bool xml_table_get_opt_uint(unsigned int& value, xml_element* root, const char* attribute_name)
{
    char buf[0x400];
    if (!xml_table_copy_opt_text(buf, sizeof(buf), root, attribute_name))
    {
        return false;
    }
    value = parse_uint(buf, sizeof(buf));
    return true;
}

// 0x005BE950 (:757)
void xml_table_get_req_float(float& value, xml_element* root, const char* attribute_name)
{
    char buf[0x400];
    xml_table_get_req_string(buf, sizeof(buf), root, attribute_name);
    value = parse_float(buf, sizeof(buf));
}

// 0x005BE9C0 (:779)
bool xml_table_get_opt_float(float& value, xml_element* root, const char* attribute_name)
{
    char buf[0x400];
    if (!xml_table_copy_opt_text(buf, sizeof(buf), root, attribute_name))
    {
        return false;
    }
    value = parse_float(buf, sizeof(buf));
    return true;
}

// 0x005B14A0 (:799)
float xml_table_get_float(const xml_element* e)
{
    char buf[0x400];
    strncpy(buf, e->text, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    return parse_float(buf, sizeof(buf));
}

// 0x005BEA70 (:816)
void xml_table_get_req_bool(bool& value, xml_element* root, const char* attribute_name)
{
    char buf[0x400];
    xml_table_get_req_string(buf, sizeof(buf), root, attribute_name);
    value = parse_bool(buf, sizeof(buf));
}

// 0x005BEAE0 (:838)
bool xml_table_get_opt_bool(bool& value, xml_element* root, const char* attribute_name)
{
    char buf[0x400];
    if (!xml_table_copy_opt_text(buf, sizeof(buf), root, attribute_name))
    {
        return false;
    }
    value = parse_bool(buf, sizeof(buf));
    return true;
}

// Next <name> sibling after element (the iteration of the list getters, inlined in the original).
static xml_element* xml_table_next_named(xml_element* element, const char* name)
{
    for (element = element->next; element != nullptr; element = element->next)
    {
        if (keen::compareStringNoCase(element->name, name) == 0)
        {
            return element;
        }
    }
    return nullptr;
}

// 0x005BEB90 (:863)
int xml_table_get_string_list(char* list, int max_string_len, int list_max, xml_element* root)
{
    // <Element> texts into consecutive max_string_len slots.
    int count = 0;
    for (xml_element* element = xml_table_find_child(root, "Element"); element != nullptr; element = xml_table_next_named(element, "Element"))
    {
        const char* text = element->text;
        if (text == nullptr)
        {
            xml_table_log("Attribute \"%s\" contains no text.", element->name);
        }
        strncpy(list, text, max_string_len);
        list[max_string_len - 1] = '\0';
        list += max_string_len;
        ++count;
    }
    return count;
}

// 0x005BEC70 (:907)
int xml_table_get_int_list(int* list, int list_max, bool (*func)(int&, int, const char*), xml_element* root)
{
    // A quoted element is converted by func(value, index, unquoted text); others are parsed as integers.
    int count = 0;
    char buf[0x400];
    for (xml_element* element = xml_table_find_child(root, "Element"); element != nullptr; element = xml_table_next_named(element, "Element"))
    {
        const char* text = element->text;
        if (text[0] == '"')
        {
            const size_t length = strcspn(text + 1, "\"");
            strncpy(buf, text + 1, length + 1);
            buf[length] = '\0';
            if (!func(list[count], count, buf))
            {
                xml_table_log("String %s could not be converted to an integer.", buf);
            }
        }
        else
        {
            strncpy(buf, text, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';
            list[count] = parse_int(buf, sizeof(buf));
        }
        ++count;
    }
    return count;
}

// 0x005BEE20 (:964)
int xml_table_get_float_list(float* list, int list_max, xml_element* root)
{
    // An element without text repeats the previous value (the buffer is reused); the first starts empty here.
    int count = 0;
    char buf[0x400];
    buf[0] = '\0';
    for (xml_element* element = xml_table_find_child(root, "Element"); element != nullptr; element = xml_table_next_named(element, "Element"))
    {
        if (element->text != nullptr)
        {
            strncpy(buf, element->text, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';
        }
        list[count++] = parse_float(buf, sizeof(buf));
    }
    return count;
}

// 0x005BEF10 (:1006)
int xml_table_get_flags(const char* const* list, int list_size, xml_element* root, bool ignore_unknown_flags)
{
    // Each <Flag> names bit i of list[i] (case-insensitive).
    unsigned int flags = 0;
    for (xml_element* element = xml_table_find_child(root, "Flag"); element != nullptr; element = xml_table_next_named(element, "Flag"))
    {
        const char* text = element->text;
        if (text == nullptr)
        {
            xml_table_log("Attribute \"%s\" contains no text.", element->name);
        }
        bool known = false;
        for (int i = 0; i < list_size; ++i)
        {
            if (list[i] == nullptr)
            {
                known = true;
                break;
            }
            if (keen::compareStringNoCase(text, list[i]) == 0)
            {
                flags |= 1u << (i & 31);
                known = true;
                break;
            }
        }
        if (!known && !ignore_unknown_flags)
        {
            xml_table_log("\"%s\" is not a valid flag name!", text);
        }
    }
    return (int)flags;
}

// 0x005BF080 (:1054)
int xml_table_get_type_from_list(const char* const* list, int list_size, xml_element* root)
{
    // Index of root's text in list (case-insensitive), -1 if it is not there.
    if (root == nullptr)
    {
        xml_table_log("Specified root does not exist.");
    }
    const char* text = root->text;
    if (text == nullptr)
    {
        xml_table_log("Attribute \"%s\" contains no text.", root->name);
    }
    for (int i = 0; i < list_size && list[i] != nullptr; ++i)
    {
        if (keen::compareStringNoCase(text, list[i]) == 0)
        {
            return i;
        }
    }
    xml_table_log("\"%s\" is not a valid type string!", text);
    return -1;
}

// 0x005BF150 (:1080)
void xml_table_get_vector(vector& vec, xml_element* root)
{
    char buf[0x400];
    xml_table_get_req_string(buf, sizeof(buf), root, "X");
    vec.x = parse_float(buf, sizeof(buf));
    xml_table_get_req_string(buf, sizeof(buf), root, "Y");
    vec.y = parse_float(buf, sizeof(buf));
    xml_table_get_req_string(buf, sizeof(buf), root, "Z");
    vec.z = parse_float(buf, sizeof(buf));
}

// 0x005BF210 (:1095)
void xml_table_get_req_vector(vector& vec, xml_element* root, const char* const attribute_name)
{
    // Despite the name, a missing element leaves vec untouched without a message.
    xml_element* element = xml_table_find_child(root, attribute_name);
    if (element != nullptr)
    {
        xml_table_get_vector(vec, element);
    }
}

// 0x005BF260 (:1116)
void xml_table_get_matrix(matrix& mat, xml_element* root)
{
    // <Identity>, a <Heading> angle, or <Rvec>/<Uvec>/<Fvec>.
    if (xml_table_find_child(root, "Identity") != nullptr)
    {
        mat = IDENTITY_MATRIX;
        return;
    }
    xml_element* heading = xml_table_find_child(root, "Heading");
    if (heading != nullptr)
    {
        char buf[0x400];
        xml_table_get_req_string(buf, sizeof(buf), heading, nullptr);
        mat.set(0.0f, 0.0f, parse_float(buf, sizeof(buf)));
        return;
    }
    xml_table_get_vector(mat.rvec, xml_table_find_req(root, "Rvec"));
    xml_table_get_vector(mat.uvec, xml_table_find_req(root, "Uvec"));
    xml_table_get_vector(mat.fvec, xml_table_find_req(root, "Fvec"));
}

// 0x005C5AE0 (:1156)
void xml_table_get_req_color(color& col, xml_element* root, const char* attribute_name)
{
    if (!xml_table_get_opt_color(col, root, attribute_name))
    {
        xml_table_log("Failed to read color: \"%s\"", attribute_name);
    }
}

// 0x005BF3E0 (:1173)
bool xml_table_get_opt_color(color& col, xml_element* root, const char* attribute_name)
{
    // <R>/<G>/<B> 0..255 (alpha 255). Out-of-range components leave col untouched; a missing one is read as 0 but makes
    // the result false.
    xml_element* element = xml_table_find_child(root, attribute_name);
    if (element == nullptr)
    {
        return false;
    }
    int red = 0;
    int green = 0;
    int blue = 0;
    const bool complete = xml_table_get_opt_int(red, element, "R") && xml_table_get_opt_int(green, element, "G") && xml_table_get_opt_int(blue, element, "B");
    if ((unsigned int)red > 0xFF || (unsigned int)green > 0xFF || (unsigned int)blue > 0xFF)
    {
        return false;
    }
    col.red = (unsigned char)red;
    col.blue = (unsigned char)blue;
    col.green = (unsigned char)green;
    col.alpha = 0xFF;
    return complete;
}

// 0x005BF4C0 (:1219)
char* xml_table_get_req_filename_ref(xml_element* root, const char* attribute_name)
{
    if (attribute_name != nullptr)
    {
        root = xml_table_find_req(root, attribute_name);
    }
    return xml_table_get_req_string_ref(root, "Filename");
}

// 0x005BF500 (:1244)
char* xml_table_get_opt_filename_ref(xml_element* root, const char* attribute_name)
{
    if (root == nullptr)
    {
        return nullptr;
    }
    if (attribute_name != nullptr)
    {
        root = xml_table_find_opt(root, attribute_name);
        if (root == nullptr)
        {
            return nullptr;
        }
    }
    xml_element* filename = xml_table_find_opt(root, "Filename");
    return filename != nullptr ? filename->text : nullptr;
}

// 0x005BF540 (:1277)
void xml_table_get_req_filename(char* buf, int buf_size, xml_element* root, const char* attribute_name)
{
    if (attribute_name != nullptr)
    {
        root = xml_table_find_req(root, attribute_name);
    }
    xml_table_get_req_string(buf, buf_size, root, "Filename");
}

// 0x005BF580 (:1302)
bool xml_table_get_opt_filename(char* buf, int buf_size, xml_element* root, const char* attribute_name)
{
    if (root == nullptr)
    {
        return false;
    }
    if (attribute_name != nullptr)
    {
        root = xml_table_find_opt(root, attribute_name);
        if (root == nullptr)
        {
            return false;
        }
    }
    return xml_table_get_opt_string(buf, buf_size, root, "Filename");
}
