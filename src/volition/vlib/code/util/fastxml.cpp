// Original: volition/vlib/code/util/fastxml.cpp
//
// In-place XML parser and DOM helpers (see fastxml.h).
//
// Parsing (decompiled behaviour, kept as is):
// - Tags have no attributes. A tag name runs to the first '>' or whitespace; after whitespace only "/>" (empty
//   element) or '>' may follow. In that case the '>' is not consumed and is read as text. "<a/>" is not an empty
//   element: its name is "a/".
// - Text runs keep their inner whitespace and lose the trailing whitespace. While parsing, xml_element::text of an
//   element holds a list of text nodes (name = the text, chained through next): a comment between two text runs starts
//   a new node. fix_xml_tree joins the nodes into one string and decodes &amp; &lt; &gt; &#N; and 2-byte UTF-8
//   sequences (to Latin-1) in place.
// - A second top-level element replaces the first one as the root.
// - Close tags are matched case-insensitively.

#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/cfile_internal.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/string.hpp"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char Xml_parse_error[512]; // 0x01CDA3B8
dynamic_string_hash_table<preloaded_table_data, 64> table_hash_table; // 0x0163E9B8

// ---- file-static globals
static mempool_base* Xml_mempool; // 0x01CE87DC, default pool for nodes and file buffers (xml_system_init)
static xml_element* Parse_stack[64]; // 0x01CE87E0, open elements while parsing
static int Parse_stack_depth; // 0x01CE87D8
static xml_append_cache Xml_append_cache[16]; // 0x01CDE450
static int Xml_append_cache_depth; // 0x01CE88E0
static bool Xml_precaching_enabled; // 0x01CE87B9

// Indentation of write_xml_file and xml_write (function-local statics in the original, 0x0163EB78 / 0x0163EB98).
static const char Xml_tabs[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

// 0x005CD360 (:68)
static cfile* open_table_file(const char* filename)
{
    // Serves the table from table_hash_table while precaching is enabled (and the priority packfile does not have it).
    preloaded_table_data table;
    if (Xml_precaching_enabled && !packfile_exists_in_priority(filename) && table_hash_table.get(filename, table))
    {
        return cf_open(table.data, table.size, "rb", PLATFORM_PC);
    }
    return cf_open(filename, "rb", PLATFORM_PC, false);
}

// 0x0058EE30 (:93)
static bool xml_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(Xml_parse_error, fmt, args);
    va_end(args);
    return false;
}

// Node allocation of the parser (inlined in the original).
static xml_element* xml_alloc_node(mempool_base* pool, char* name)
{
    xml_element* element = (xml_element*)pool->alloc(sizeof(xml_element), 4);
    if (element == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        xml_error("Mempool \"%s\" not large enough!", pool->get_name());
        return nullptr;
    }
    element->name = name;
    element->text = nullptr;
    element->elements = nullptr;
    element->next = nullptr;
    return element;
}

static inline bool xml_is_space(char c)
{
    return (unsigned char)c <= ' ';
}

// 0x005BF5D0 (:109)
static bool parse_start_tag(char** buffer)
{
    ++*buffer;
    while (**buffer != '\0' && xml_is_space(**buffer))
    {
        ++*buffer;
    }
    char* p = *buffer;
    if (Parse_stack_depth == 64)
    {
        return xml_error("Parse stack overflow in XML module!");
    }
    const int depth = Parse_stack_depth++;
    if (depth == 0)
    {
        Parse_stack[0] = xml_alloc_node(Xml_mempool, p);
    }
    else
    {
        Parse_stack[depth] = xml_new(p, nullptr, Parse_stack[depth - 1], nullptr);
    }
    while (*p != '\0' && *p != '>' && !xml_is_space(*p))
    {
        ++p;
    }
    const char end = *p;
    *p++ = '\0';
    if (end != '>')
    {
        while (*p != '\0' && xml_is_space(*p))
        {
            ++p;
        }
        if (p[0] == '/' && p[1] == '>')
        {
            p += 2;
            --Parse_stack_depth;
        }
        else if (*p != '>')
        {
            return xml_error("Syntax error in open tag");
        }
    }
    *buffer = p;
    return true;
}

// 0x005B1760 (:165)
static bool parse_end_tag(char** buffer)
{
    *buffer += 2;
    while (**buffer != '\0' && xml_is_space(**buffer))
    {
        ++*buffer;
    }
    char* name = *buffer;
    if (Parse_stack_depth == 0)
    {
        return xml_error("Parse stack underflow in XML module!");
    }
    const int depth = Parse_stack_depth - 1;
    char* p = name;
    while (*p != '\0' && *p != '>' && !xml_is_space(*p))
    {
        ++p;
    }
    const char end = *p;
    Parse_stack_depth = depth;
    *p++ = '\0';
    if (end != '>')
    {
        while (*p != '\0' && xml_is_space(*p))
        {
            ++p;
        }
        if (*p != '>')
        {
            return xml_error("Syntax error in close tag");
        }
    }
    if (keen::compareStringNoCase(name, Parse_stack[depth]->name) != 0)
    {
        return xml_error("Malformed XML close tag (doesn't match \"%s\")!", Parse_stack[depth]->name);
    }
    *buffer = p;
    return true;
}

// 0x0058EE50 (:211)
static bool parse_comment(char** buffer, unsigned int* line_number)
{
    unsigned int line = *line_number;
    *buffer += 4;
    char* p = *buffer;
    for (;;)
    {
        while (*p != '\0' && *p != '-')
        {
            if (*p == '\n')
            {
                ++line;
            }
            ++p;
        }
        if (*p != '-')
        {
            return xml_error("Unterminated comment");
        }
        if (p[1] == '-' && p[2] == '>')
        {
            *buffer = p + 3;
            *line_number = line;
            return true;
        }
        ++p;
    }
}

// 0x005BF710 (:243)
xml_element* add_xml_text(xml_element* element, const char* text)
{
    // Appends a text node to the element's text list (see the file comment).
    xml_element* node = xml_alloc_node(Xml_mempool, (char*)text);
    if (node == nullptr)
    {
        return nullptr;
    }
    xml_element* last = (xml_element*)element->text;
    if (last == nullptr)
    {
        element->text = (char*)node;
        return node;
    }
    while (last->next != nullptr)
    {
        last = last->next;
    }
    last->next = node;
    return node;
}

// 0x005BF7A0 (:273)
static bool parse_xml_buffer(char* buffer, unsigned int* line_number)
{
    xml_element* text_node = nullptr; // last text node of the current run of text
    *line_number = 1;
    char* p = buffer;
    for (;;)
    {
        while (*p != '\0' && xml_is_space(*p))
        {
            if (*p == '\n')
            {
                ++*line_number;
            }
            ++p;
        }
        if (*p == '\0')
        {
            return true;
        }
        if (*p == '<')
        {
            *p = '\0'; // ends the text before the tag
            bool result;
            if (p[1] == '!' && p[2] == '-' && p[3] == '-')
            {
                result = parse_comment(&p, line_number);
            }
            else
            {
                text_node = nullptr;
                result = p[1] == '/' ? parse_end_tag(&p) : parse_start_tag(&p);
            }
            if (!result)
            {
                return false;
            }
        }
        else
        {
            if (Parse_stack_depth < 1)
            {
                return xml_error("Text outside of a tag!");
            }
            if (text_node == nullptr)
            {
                text_node = add_xml_text(Parse_stack[Parse_stack_depth - 1], p);
                if (text_node == nullptr)
                {
                    return false;
                }
            }
            else
            {
                xml_element* node = xml_alloc_node(Xml_mempool, p);
                text_node->next = node;
                text_node = node;
            }
            // Words and the whitespace between them, up to the next tag; the trailing whitespace is cut off.
            char* word_end;
            do
            {
                word_end = p;
                while (*word_end != '\0' && *word_end != '<' && !xml_is_space(*word_end))
                {
                    ++word_end;
                }
                if (*word_end == '\0')
                {
                    return true;
                }
                p = word_end;
                while (xml_is_space(*p))
                {
                    if (*p == '\n')
                    {
                        ++*line_number;
                    }
                    ++p;
                    if (*p == '\0')
                    {
                        break;
                    }
                }
            }
            while (*p != '\0' && *p != '<');
            if (word_end != p)
            {
                *word_end = '\0';
            }
        }
        if (*p == '\0')
        {
            return true;
        }
    }
}

// 0x0058EEA0 (:362)
static bool fix_special_characters(char* text)
{
    char* src = text;
    char* dst = text;
    while (*src != '\0')
    {
        const char c = *src;
        if (c == '&')
        {
            if (src[1] == '#')
            {
                char* digits = src + 2;
                char* p = digits;
                while (*p != '\0' && *p != ';')
                {
                    if (!isdigit((unsigned char)*p))
                    {
                        return xml_error("Invalid xml numbered character sequence (non-number characters). Text is '%s'", text);
                    }
                    ++p;
                }
                if (*p != ';')
                {
                    return xml_error("Invalid xml numbered character sequence (missing semicolon). Text is '%s'", text);
                }
                const unsigned int value = (unsigned int)atoi(digits);
                if (value > 0xFF)
                {
                    return xml_error("Invalid xml numbered character sequence (out of range). Text is '%s'", text);
                }
                *dst = (char)value;
                src = p + 1;
            }
            else if (strncmp(src + 1, "amp;", 4) == 0)
            {
                *dst = '&';
                src += 5;
            }
            else if (strncmp(src + 1, "gt;", 3) == 0)
            {
                *dst = '>';
                src += 4;
            }
            else if (strncmp(src + 1, "lt;", 3) == 0)
            {
                *dst = '<';
                src += 4;
            }
            else
            {
                return xml_error("Unrecognized xml character sequence. Text is '%s'", text);
            }
        }
        else if (((unsigned char)c & 0xE0) == 0xC0 && ((unsigned char)src[1] & 0xC0) == 0x80)
        {
            const unsigned short value = (unsigned short)(((c & 0x1F) << 6) | (src[1] & 0x3F));
            if (value > 0xFF)
            {
                return xml_error("Unsupported UTF-8 2byte sequence: %x,%x", (unsigned char)c, (unsigned char)src[1]);
            }
            *dst = (char)value;
            src += 2;
        }
        else
        {
            *dst = c;
            ++src;
        }
        ++dst;
    }
    *dst = '\0';
    return true;
}

// 0x0058F040 (:434)
static bool fix_xml_tree(xml_element* root)
{
    for (; root != nullptr; root = root->next)
    {
        xml_element* text_node = (xml_element*)root->text;
        if (text_node != nullptr)
        {
            if (text_node->next == nullptr)
            {
                root->text = (char*)text_node->name;
            }
            else
            {
                // Grown one character at a time in place; the original writes each character before growing the
                // block to cover it.
                unsigned int new_size = 0;
                root->text = (char*)Xml_mempool->alloc(1, 4);
                for (; text_node != nullptr; text_node = text_node->next)
                {
                    for (const char* p = text_node->name; *p != '\0'; ++p)
                    {
                        root->text[new_size++] = *p;
                        if (Xml_mempool->realloc(root->text, new_size) == nullptr)
                        {
                            return xml_error("Error reallocating mempool memory in %s", Xml_mempool->get_name());
                        }
                    }
                }
                root->text[new_size] = '\0';
            }
        }
        if (root->text != nullptr && !fix_special_characters(root->text))
        {
            return false;
        }
        if (!fix_xml_tree(root->elements))
        {
            return false;
        }
    }
    return true;
}

// 0x0058F120 (:493)
bool write_xml_file(xml_element* element, unsigned int depth, cfile* p_cfile)
{
    // Tabs, "<name>", text, children on their own lines, "</name>\n".
    static char tag_name[128]; // 0x01D579B0
    for (; element != nullptr; element = element->next)
    {
        if (depth != 0)
        {
            cf_write(Xml_tabs, depth, p_cfile);
        }
        sprintf(tag_name, "<%s>", element->name);
        cf_write(tag_name, (unsigned int)strlen(tag_name), p_cfile);
        if (element->text != nullptr)
        {
            cf_write(element->text, (unsigned int)strlen(element->text), p_cfile);
        }
        if (element->elements != nullptr)
        {
            cf_write_char('\n', p_cfile);
            if (!write_xml_file(element->elements, depth + 1, p_cfile))
            {
                return false;
            }
            cf_write(Xml_tabs, depth, p_cfile);
        }
        sprintf(tag_name, "</%s>\n", element->name);
        cf_write(tag_name, (unsigned int)strlen(tag_name), p_cfile);
    }
    return true;
}

// 0x0058F320 (:535)
static xml_element* clone_xml_element(xml_element* root, mempool_base* dest, bool clone_siblings)
{
    // Deep copy (names and texts too). Returns nullptr when dest runs out.
    if (root == nullptr)
    {
        return nullptr;
    }
    char* name = (char*)dest->alloc((unsigned int)strlen(root->name) + 1, 0);
    if (name == nullptr)
    {
        return nullptr;
    }
    strcpy(name, root->name);
    xml_element* clone = (xml_element*)dest->alloc(sizeof(xml_element), 0);
    if (clone == nullptr)
    {
        return nullptr;
    }
    clone->name = name;
    clone->elements = clone_xml_element(root->elements, dest, true);
    if (clone->elements == nullptr && root->elements != nullptr)
    {
        return nullptr;
    }
    clone->next = nullptr;
    if (clone_siblings)
    {
        xml_element* last = clone;
        for (xml_element* sibling = root->next; sibling != nullptr; sibling = sibling->next)
        {
            last->next = clone_xml_element(sibling, dest, false);
            if (last->next == nullptr)
            {
                return nullptr;
            }
            last = last->next;
        }
    }
    if (root->text == nullptr)
    {
        clone->text = nullptr;
        return clone;
    }
    clone->text = (char*)dest->alloc((unsigned int)strlen(root->text) + 1, 0);
    if (clone->text == nullptr)
    {
        return nullptr;
    }
    strcpy(clone->text, root->text);
    return clone;
}

// 0x0058F430 (:615)
void xml_system_init(mempool_base* dest)
{
    Xml_mempool = dest;
    Xml_append_cache_depth = 0;
}

// 0x005B1840 (:635)
xml_element* xml_new(const char* tag, const char* text, xml_element* parent, mempool_base* dest)
{
    // tag and text are not copied (xml_new_dup copies them).
    mempool_base* previous_pool = Xml_mempool;
    if (dest != nullptr)
    {
        Xml_mempool = dest;
    }
    xml_element* element = (xml_element*)Xml_mempool->alloc(sizeof(xml_element), 4);
    if (element == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        Xml_mempool = previous_pool;
        xml_error("Mempool \"%s\" not large enough!", (dest != nullptr ? dest : previous_pool)->get_name());
        return nullptr;
    }
    element->name = tag;
    element->text = (char*)text;
    element->elements = nullptr;
    element->next = nullptr;
    if (parent != nullptr)
    {
        xml_append(parent, element);
    }
    Xml_mempool = previous_pool;
    return element;
}

// 0x005B18E0 (:688)
xml_element* xml_new_dup(const char* tag, const char* text, xml_element* parent, mempool_base* dest)
{
    if (dest == nullptr)
    {
        dest = Xml_mempool;
    }
    char* tag_copy = (char*)dest->alloc((unsigned int)strlen(tag) + 1, 4);
    if (tag_copy == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    strcpy(tag_copy, tag);
    char* text_copy = nullptr;
    if (text != nullptr)
    {
        text_copy = (char*)dest->alloc((unsigned int)strlen(text) + 1, 4);
        if (text_copy == nullptr)
        {
            VLIB_CRITICAL_FAILURE();
            return nullptr;
        }
        strcpy(text_copy, text);
    }
    return xml_new(tag_copy, text_copy, parent, dest);
}

// 0x005CD3D0 (:737)
xml_element* xml_parse(const char* filename, mempool_base* dest)
{
    // The file is read into the pool and parsed in place; the tree points into that buffer.
    cfile* fp = open_table_file(filename);
    mempool_base* previous_pool = Xml_mempool;
    if (fp == nullptr)
    {
        xml_error("File \"%s\" does not exist.", filename);
        return nullptr;
    }
    if (dest != nullptr)
    {
        Xml_mempool = dest;
    }
    const unsigned int size = fp->size;
    if (size == 0)
    {
        cf_close(fp);
        Xml_mempool = previous_pool;
        xml_error("File %s is 0 bytes!", filename);
        return nullptr;
    }
    Parse_stack[0] = nullptr;
    Parse_stack_depth = 0;
    char* buf = (char*)Xml_mempool->alloc(size + 1, 4);
    Xml_append_cache_depth = 0;
    if (buf == nullptr)
    {
        xml_error("Mempool \"%s\" not large enough!", Xml_mempool->get_name());
        cf_close(fp);
    }
    else
    {
        const unsigned int num_read = cf_read(buf, size, fp, false);
        cf_close(fp);
        if (num_read != 0xFFFFFFFF && num_read != 0xFFFFFFFE)
        {
            if (num_read != 0)
            {
                buf[num_read] = '\0';
                xml_element* root = xml_parse_from_string(buf, dest, nullptr);
                Xml_mempool = previous_pool;
                return root;
            }
            xml_error("Failed to read entire file!");
        }
    }
    // The original also formats "%s : %s" (filename, Xml_parse_error) into a thread-local scratch string that nothing
    // reads.
    Xml_mempool = previous_pool;
    return nullptr;
}

// 0x005BF960 (:830)
xml_element* xml_parse_from_string(char* buf, mempool_base* dest, const char* filename_orig)
{
    // buf is modified (and on failure overwritten with the message copied to Xml_parse_error).
    mempool_base* previous_pool = Xml_mempool;
    if (dest != nullptr)
    {
        Xml_mempool = dest;
    }
    Parse_stack_depth = 0;
    Parse_stack[0] = nullptr;
    Xml_append_cache_depth = 0;
    unsigned int line_number = 0;
    if (!parse_xml_buffer(buf, &line_number))
    {
        if (filename_orig == nullptr)
        {
            sprintf(buf, "(line %d): %s", line_number, Xml_parse_error);
        }
        else
        {
            sprintf(buf, "%s (line %d): %s", filename_orig, line_number, Xml_parse_error);
        }
        strcpy(Xml_parse_error, buf);
        Xml_mempool = previous_pool;
        return nullptr;
    }
    xml_element* root = Parse_stack[0];
    if (root == nullptr)
    {
        xml_error("No data in XML file!");
        Xml_mempool = previous_pool;
        return nullptr;
    }
    if (Parse_stack_depth > 0)
    {
        xml_error("Unclosed tag in XML file!");
        Xml_mempool = previous_pool;
        return nullptr;
    }
    const bool fixed = fix_xml_tree(root);
    Xml_mempool = previous_pool;
    return fixed ? root : nullptr;
}

// 0x005CD510 (:894)
bool xml_write(const char* filename, xml_element* root)
{
    cfile* fp = cf_open(filename, "wt", PLATFORM_PC, false);
    if (fp == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    const bool result = xml_write(fp, root, 0);
    cf_close(fp);
    return result;
}

// 0x0058F450 (:912)
static bool xml_write_string_tabbed(_iobuf* f, const char* src, int num_tabs)
{
    // '\v' stands for num_tabs + 1 tabs. Returns whether src has a line break. A '\v' is not stepped over, so a text
    // containing one never ends (as in the original).
    bool has_line_break = false;
    if (*src == '\0')
    {
        return false;
    }
    for (char c = *src; c != '\0'; c = *src)
    {
        if (c == '\n')
        {
            has_line_break = true;
        }
        if (c == '\v')
        {
            for (int i = num_tabs + 1; i > 0; --i)
            {
                fprintf((FILE*)f, "%c", '\t');
            }
        }
        else
        {
            fprintf((FILE*)f, "%c", c);
            ++src;
        }
    }
    return has_line_break;
}

// 0x0058F4C0 (:934)
static bool xml_write_string_tabbed(cfile* p_file, const char* src, int num_tabs)
{
    // As above, into a cfile.
    bool has_line_break = false;
    if (*src == '\0')
    {
        return false;
    }
    for (char c = *src; c != '\0'; c = *src)
    {
        if (c == '\n')
        {
            has_line_break = true;
        }
        if (c == '\v')
        {
            for (int i = num_tabs + 1; i > 0; --i)
            {
                cf_write_char('\t', p_file);
            }
        }
        else
        {
            cf_write_char(c, p_file);
            ++src;
        }
    }
    return has_line_break;
}

// 0x0058F590 (:957)
bool xml_write(_iobuf* f, xml_element* root, unsigned int depth)
{
    // Each element is indented by depth + 1 tabs (30 at most).
    char tabs[31];
    memset(tabs, '\t', 30);
    tabs[30] = '\0';
    tabs[depth + 1] = '\0';
    for (; root != nullptr; root = root->next)
    {
        fputs(tabs, (FILE*)f);
        fprintf((FILE*)f, "<%s>", root->name);
        if (root->text != nullptr)
        {
            xml_write_string_tabbed(f, root->text, (int)depth);
        }
        if (root->elements != nullptr)
        {
            fprintf((FILE*)f, "\n");
            if (!xml_write(f, root->elements, depth + 1))
            {
                return false;
            }
            fputs(tabs, (FILE*)f);
        }
        fprintf((FILE*)f, "</%s>\n", root->name);
    }
    return true;
}

// 0x0058F700 (:1004)
bool xml_write(cfile* p_cfile, xml_element* element, unsigned int depth)
{
    // CRLF line ends; elements are indented by depth - 1 tabs, and top-level entries (depth 1) are separated by an
    // empty line.
    static char tag_name[128]; // 0x01D57A30
    for (; element != nullptr; element = element->next)
    {
        const unsigned int num_tabs = depth == 0 ? 0 : depth - 1;
        if (num_tabs != 0)
        {
            cf_write(Xml_tabs, num_tabs, p_cfile);
        }
        sprintf(tag_name, "<%s>", element->name);
        cf_write(tag_name, (unsigned int)strlen(tag_name), p_cfile);
        if (element->text != nullptr)
        {
            xml_write_string_tabbed(p_cfile, element->text, (int)depth);
        }
        if (element->elements == nullptr)
        {
            if (depth == 1)
            {
                cf_write("\r\n", 2, p_cfile);
                cf_write(Xml_tabs, 1, p_cfile);
            }
        }
        else
        {
            cf_write_char('\r', p_cfile);
            cf_write_char('\n', p_cfile);
            if (!xml_write(p_cfile, element->elements, depth + 1))
            {
                return false;
            }
            cf_write(Xml_tabs, depth, p_cfile);
        }
        sprintf(tag_name, "</%s>\r\n", element->name);
        cf_write(tag_name, (unsigned int)strlen(tag_name), p_cfile);
        if (depth == 1 && element->next != nullptr)
        {
            cf_write("\r\n", 2, p_cfile);
        }
    }
    return true;
}

// 0x005B19C0 (:1065)
xml_element* xml_find(const xml_element* element, const char* tag)
{
    // First child named tag (case-insensitive).
    if (element != nullptr)
    {
        for (xml_element* child = element->elements; child != nullptr; child = child->next)
        {
            if (keen::compareStringNoCase(child->name, tag) == 0)
            {
                return child;
            }
        }
    }
    return nullptr;
}

// 0x005B1A00 (:1093)
xml_element* xml_find_next(xml_element* element, xml_element* cur, const char* tag)
{
    if (element != nullptr && cur != nullptr)
    {
        for (cur = cur->next; cur != nullptr; cur = cur->next)
        {
            if (keen::compareStringNoCase(cur->name, tag) == 0)
            {
                return cur;
            }
        }
    }
    return nullptr;
}

// 0x005B1A40 (:1116)
xml_element* xml_find_nth(xml_element* element, const char* tag, unsigned int index)
{
    if (element == nullptr)
    {
        return nullptr;
    }
    for (xml_element* child = element->elements; child != nullptr; child = child->next)
    {
        if (keen::compareStringNoCase(child->name, tag) == 0)
        {
            if (index == 0)
            {
                return child;
            }
            --index;
        }
    }
    return nullptr;
}

// 0x0058F950 (:1147)
bool xml_remove(xml_element* root, xml_element* element)
{
    Xml_append_cache_depth = 0;
    if (root == nullptr || element == nullptr)
    {
        return false;
    }
    xml_element* child = root->elements;
    if (child == element)
    {
        root->elements = element->next;
        element->next = nullptr;
        return true;
    }
    for (; child->next != nullptr; child = child->next)
    {
        if (child->next == element)
        {
            child->next = element->next;
            element->next = nullptr;
            return true;
        }
    }
    return false;
}

// 0x0058F9B0 (:1246)
bool xml_append(xml_element* root, xml_element* element)
{
    // Appends element as root's last child. The last child of up to 16 recent parents is cached (the cache wraps around
    // to slot 0 when full).
    if (root == nullptr || element == nullptr)
    {
        return false;
    }
    xml_append_cache* cache = nullptr;
    for (int i = Xml_append_cache_depth - 1; i >= 0; --i)
    {
        if (Xml_append_cache[i].parent == root)
        {
            cache = &Xml_append_cache[i];
            break;
        }
    }
    if (cache == nullptr)
    {
        int index = Xml_append_cache_depth;
        if (index > 15)
        {
            index = 0;
        }
        cache = &Xml_append_cache[index];
        Xml_append_cache_depth = index + 1;
        cache->parent = root;
        cache->last_child = nullptr;
    }
    xml_element* last = cache->last_child;
    if (last == nullptr)
    {
        last = root->elements;
        if (last == nullptr)
        {
            root->elements = element;
            cache->last_child = element;
            return true;
        }
        while (last->next != nullptr)
        {
            last = last->next;
        }
    }
    last->next = element;
    cache->last_child = element;
    return true;
}

// 0x0058FA40 (:1320)
bool xml_insert(xml_element* root, xml_element* element)
{
    // Inserts element (and the siblings chained to it) in front of root's children.
    if (root == nullptr || element == nullptr)
    {
        return false;
    }
    if (root->elements != nullptr)
    {
        xml_element* last = element;
        while (last->next != nullptr)
        {
            last = last->next;
        }
        last->next = root->elements;
    }
    root->elements = element;
    return true;
}

// 0x0058FA80 (:1356)
bool xml_insert_before(xml_element* root, xml_element* element, xml_element* element_to_insert)
{
    if (root == nullptr || element == nullptr || element_to_insert == nullptr)
    {
        return false;
    }
    xml_element** link = &root->elements;
    if (*link == nullptr)
    {
        return false;
    }
    // Stops at element, or at the last child (the original then inserts before that one).
    while (*link != element && (*link)->next != nullptr)
    {
        link = &(*link)->next;
    }
    xml_element* last = element_to_insert;
    while (last->next != nullptr)
    {
        last = last->next;
    }
    last->next = *link;
    *link = element_to_insert;
    return true;
}

// 0x005B1A90 (:1397)
unsigned int xml_count(xml_element* element, const char* tag)
{
    unsigned int count = 0;
    if (element == nullptr)
    {
        return 0;
    }
    for (xml_element* child = element->elements; child != nullptr; child = child->next)
    {
        if (keen::compareStringNoCase(child->name, tag) == 0)
        {
            ++count;
        }
    }
    return count;
}

// 0x0058FAE0 (:1426)
unsigned int xml_checksum(xml_element* root, unsigned int accumulator)
{
    if (root != nullptr)
    {
        accumulator = crc_str(root->name, accumulator);
        if (root->text != nullptr)
        {
            accumulator = crc_str(root->text, accumulator);
        }
        for (xml_element* child = root->elements; child != nullptr; child = child->next)
        {
            accumulator = xml_checksum(child, accumulator);
        }
    }
    return accumulator;
}

// 0x0058FB30 (:1457)
xml_element* xml_clone(xml_element* root, mempool_base* dest)
{
    return clone_xml_element(root, dest != nullptr ? dest : Xml_mempool, false);
}

// 0x0058FB50 (:1474)
char* xml_get_string(xml_element* e, char* default_val)
{
    return e != nullptr && e->text != nullptr ? e->text : default_val;
}

// 0x005B1AD0 (:1487)
bool xml_get_bool(xml_element* e, bool default_val)
{
    // "yes", "true" and "1" are true (case-insensitive), any other text false.
    if (e == nullptr || e->text == nullptr)
    {
        return default_val;
    }
    return keen::compareStringNoCase(e->text, "yes") == 0 || keen::compareStringNoCase(e->text, "true") == 0 || keen::compareStringNoCase(e->text, "1") == 0;
}

// 0x0058FB70 (:1504)
float xml_get_float(xml_element* e, float default_val)
{
    if (e != nullptr && e->text != nullptr)
    {
        return (float)strtod(e->text, nullptr);
    }
    return default_val;
}

// 0x0058FBA0 (:1517)
float xml_get_float_hex(xml_element* e, float default_val)
{
    // The integer value converted to float (not the bits).
    if (e != nullptr && e->text != nullptr)
    {
        return (float)strtoul(e->text, nullptr, 0);
    }
    return default_val;
}

// 0x0058FBD0 (:1533)
int xml_get_int(xml_element* e, int default_val)
{
    if (e != nullptr && e->text != nullptr)
    {
        return (int)strtol(e->text, nullptr, 0);
    }
    return default_val;
}

// 0x0058FC00 (:1546)
unsigned int xml_get_uint(const xml_element* e, unsigned int default_val)
{
    unsigned int value;
    if (e != nullptr && e->text != nullptr && sscanf(e->text, "%u", &value) == 1)
    {
        return value;
    }
    return default_val;
}

// 0x0058FC40 (:1566)
unsigned int xml_get_flags(xml_element* e, const char** flags, unsigned int num_flags, unsigned int default_val)
{
    // The text is a list of flag names separated by whitespace; each names bit i of flags[i] (case-insensitive, whole
    // words). Notes (as in the original): the text must not be null; with an unknown name the scan resumes where the
    // comparison with the last flag stopped (inside the word); with num_flags == 0 a non-empty text never ends.
    if (e == nullptr)
    {
        return default_val;
    }
    unsigned int result = 0;
    const char* p = e->text;
    while (*p != '\0')
    {
        if (xml_is_space(*p))
        {
            ++p;
            continue;
        }
        if (num_flags == 0)
        {
            continue;
        }
        const char* stop = p;
        unsigned int i;
        for (i = 0; i < num_flags; ++i)
        {
            const char* flag = flags[i];
            const char* q = p;
            bool match = true;
            while (!xml_is_space(*q) && *flag != '\0')
            {
                const bool same = tolower(*flag) == tolower(*q);
                ++flag;
                ++q;
                if (!same)
                {
                    match = false;
                    break;
                }
            }
            stop = q;
            if (xml_is_space(*q) && match && *flag == '\0')
            {
                result |= 1u << (i & 31);
                break;
            }
        }
        p = stop;
        if (i == num_flags)
        {
            VLIB_CRITICAL_FAILURE();
        }
    }
    return result;
}

// 0x0058FD70 (:1623)
bool xml_clear_cache()
{
    if (Xml_mempool != nullptr)
    {
        Xml_append_cache_depth = 0;
    }
    return Xml_mempool != nullptr;
}

// 0x0058FD90 (:1639)
void xml_disable_precache()
{
    Xml_precaching_enabled = false;
}

// 0x0058FDA0 (:1644)
bool xml_precaching_enabled()
{
    return Xml_precaching_enabled;
}

// 0x005CFBE0 (:1649)
bool xml_enable_precache(const char* filename, mempool_base* table_preload_pool_ptr, mempool_base* temp_mempool, unsigned int max_table_files)
{
    // The table packfile's directory goes to temp_mempool (released again), its data to table_preload_pool_ptr. A
    // split mempool (flag 0x40) has its GPU half marked and restored too.
    unsigned int mark = 0;
    unsigned int gpu_mark = 0;
    const bool split = temp_mempool != nullptr && (temp_mempool->get_flags() & 0x40) != 0;
    if (temp_mempool != nullptr)
    {
        mark = temp_mempool->mark();
        if (split)
        {
            mempool* gpu_pool = static_cast<split_mempool*>(temp_mempool)->get_gpu_pool();
            if (gpu_pool == nullptr)
            {
                VLIB_CRITICAL_FAILURE();
                gpu_mark = 0;
            }
            else
            {
                gpu_mark = gpu_pool->mark();
            }
        }
    }
    v_packfile packfile;
    bool result = packfile_read_condensed(filename, temp_mempool, table_preload_pool_ptr, &packfile, false);
    if (result)
    {
        result = xml_enable_precache(&packfile, table_preload_pool_ptr, max_table_files);
    }
    else
    {
        table_preload_pool_ptr->clear();
    }
    if (temp_mempool != nullptr)
    {
        if (!split)
        {
            temp_mempool->restore_to_mark(mark);
        }
        else
        {
            split_mempool* split_pool = static_cast<split_mempool*>(temp_mempool);
            split_pool->restore_to_mark_cpu(mark);
            if (split_pool->get_gpu_pool() != nullptr)
            {
                split_pool->get_gpu_pool()->restore_to_mark(gpu_mark);
            }
        }
    }
    return result;
}

// 0x005CD570 (:1665)
bool xml_enable_precache(v_packfile* pf, mempool_base* table_preload_pool_ptr, unsigned int max_table_files)
{
    // Indexes every file of the (condensed, loaded) packfile by name; open_table_file then serves them from memory.
    table_hash_table.init(*table_preload_pool_ptr, max_table_files);
    for (unsigned int i = 0; i < pf->num_files; ++i)
    {
        const v_packfile_entry& entry = pf->dir[i];
        preloaded_table_data table;
        table.data = pf->data + entry.start;
        table.size = entry.size;
        table_hash_table.insert(entry.name, table); // find(name, true) + data store, inlined in the original
    }
    Xml_precaching_enabled = true;
    return true;
}
