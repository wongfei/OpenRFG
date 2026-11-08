// Original: volition/vlib/code/parse/parse.cpp
//
// Text tokenizer (see parse.h). Keywords match case-insensitively by prefix. White space includes // and /* */
// comments (and, since chars are signed, every byte >= 0x80); parse_gray skips the same but stops at line ends.
//
// Partial: parse_mat2s (0x0058DEF0, :1556, needs matrix::extract_angles) and get_line are not implemented yet.
//
// Notes (decompiled behaviour, kept as is):
// - Errors are only printed; parsing carries on from where it was. The list parsers rewind to a bad element after
//   reporting it, so a bad element in an int/flag list repeats forever.
// - parse_string and parse_string_alphanumeric copy the whole token even when it does not fit (parse_string reports
//   it first); parse_xml_required/optional_string copy the element text without a bound when the close tag is found.
// - parse_skip_to compares the position against size, which open_from_mem_no_copy does not set.
// - parse_optional_string allows white space after the opening quote but steps over the match as if there was none.
// - error() reads 40 bytes from the current position even past the end of the text.

#include "volition/vlib/code/parse/parse.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/cfile_internal.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// 0x0058D280 (:39)
static void get_xml_tags(const char* tag, char* open, char* close)
{
    sprintf(open, "<%s>", tag);
    sprintf(close, "</%s>", tag);
}

// 0x0058C5E0 (.h:27)
bool isNewLine(char c)
{
    return c == '\r' || c == '\n';
}

// 0x0058D2B0 (:53)
parse::parse()
{
    text = nullptr;
    cur_ptr = nullptr;
    filename[0] = '\0';
    did_allocate = false;
}

// 0x005B01F0 (:63)
parse::~parse()
{
    if (text != nullptr && did_allocate)
    {
        Vlib_temp_mempool->restore_to_mark(mempool_mark);
    }
    text = nullptr;
}

// 0x0058D2D0 (:70)
void parse::parse_comment()
{
    // The rest of a /* */ comment (both '\r' and '\n' count as line ends here).
    while (*cur_ptr != '\0' && !(cur_ptr[0] == '*' && cur_ptr[1] == '/'))
    {
        if (isNewLine(*cur_ptr))
        {
            ++line_num;
        }
        ++cur_ptr;
    }
    if (*cur_ptr != '\0')
    {
        cur_ptr += 2;
    }
}

// Skips a // comment up to (not over) the line end; inlined in parse_gray and parse_white.
static void parse_skip_line_comment(char*& cur_ptr)
{
    do
    {
        if (isNewLine(*cur_ptr))
        {
            break;
        }
        ++cur_ptr;
    }
    while (*cur_ptr != '\0');
}

// 0x0058D310 (:88)
void parse::parse_gray()
{
    while (*cur_ptr != '\0')
    {
        const char c = *cur_ptr;
        if (c < '!' && !isNewLine(c))
        {
            ++cur_ptr;
        }
        else if (c == '/' && cur_ptr[1] == '/')
        {
            parse_skip_line_comment(cur_ptr);
        }
        else if (c == '/' && cur_ptr[1] == '*')
        {
            cur_ptr += 2;
            parse_comment();
        }
        else
        {
            return;
        }
    }
}

// 0x0058D3C0 (:119)
void parse::parse_white()
{
    while (*cur_ptr != '\0')
    {
        const char c = *cur_ptr;
        if (c < '!' && !isNewLine(c))
        {
            ++cur_ptr;
        }
        else if (isNewLine(c))
        {
            ++cur_ptr;
            if (c == '\n')
            {
                ++line_num;
            }
        }
        else if (c == '/' && cur_ptr[1] == '/')
        {
            parse_skip_line_comment(cur_ptr);
        }
        else if (c == '/' && cur_ptr[1] == '*')
        {
            cur_ptr += 2;
            parse_comment();
        }
        else
        {
            return;
        }
    }
}

// 0x0058D490 (:156)
void parse::parse_skip_line()
{
    while (*cur_ptr != '\0' && !isNewLine(*cur_ptr))
    {
        ++cur_ptr;
    }
    while (isNewLine(*cur_ptr))
    {
        const char c = *cur_ptr++;
        if (c == '\n')
        {
            ++line_num;
        }
    }
}

// 0x005B0220 (:178)
bool parse::parse_wrapped_skip_to(const char* keyword)
{
    // Searches from the current position, then from the start.
    if (parse_skip_to(keyword, nullptr))
    {
        return true;
    }
    char* const saved_ptr = cur_ptr;
    const unsigned int saved_line = line_num;
    reset();
    if (parse_skip_to(keyword, nullptr))
    {
        return true;
    }
    line_num = saved_line;
    cur_ptr = saved_ptr;
    return false;
}

// 0x0058D4D0 (:208)
bool parse::is_comment(const char* pos)
{
    // Whether pos is inside a comment, scanning from the current position.
    int state = 0; // 1 line comment, 2 block comment
    for (const char* p = cur_ptr; p != pos; ++p)
    {
        if (state == 0)
        {
            if (p[0] == '/')
            {
                if (p[1] == '/')
                {
                    state = 1;
                }
                if (p[1] == '*')
                {
                    state = 2;
                }
            }
        }
        else if (state == 1)
        {
            if (isNewLine(*p))
            {
                state = 0;
            }
        }
        else if (state == 2)
        {
            if (p[0] == '*' && p[1] == '/')
            {
                state = 0;
            }
        }
        else
        {
            VLIB_CRITICAL_FAILURE();
        }
    }
    return state != 0;
}

// 0x0058D560 (:266)
bool parse::parse_skip_to(const char* keyword, const char* end_mark)
{
    // Moves to the next keyword outside comments, unless end_mark comes first.
    if ((unsigned int)(cur_ptr - text) > size)
    {
        return false;
    }
    parse_white();
    char* pos = cur_ptr - 1;
    do
    {
        pos = strstr(pos + 1, keyword);
        if (pos == nullptr)
        {
            return false;
        }
    }
    while (is_comment(pos));
    if (end_mark != nullptr)
    {
        char* end = cur_ptr - 1;
        for (;;)
        {
            end = strstr(end + 1, end_mark);
            if (end == nullptr || !is_comment(end))
            {
                break;
            }
        }
        if (end != nullptr && end < pos)
        {
            return false;
        }
    }
    while (cur_ptr != pos)
    {
        if (*cur_ptr++ == '\n')
        {
            ++line_num;
        }
    }
    return true;
}

// 0x005BE2E0 (:324)
bool parse::parse_wrapped_skip_over(const char* keyword)
{
    if (!parse_skip_to(keyword, nullptr))
    {
        const unsigned int saved_line = line_num;
        char* const saved_ptr = cur_ptr;
        reset();
        if (!parse_skip_to(keyword, nullptr))
        {
            line_num = saved_line;
            cur_ptr = saved_ptr;
            return false;
        }
    }
    parse_required(keyword);
    return true;
}

// 0x005BE350 (:343)
bool parse::parse_skip_over(const char* keyword, const char* end_mark)
{
    if (!parse_skip_to(keyword, end_mark))
    {
        return false;
    }
    parse_required(keyword);
    return true;
}

// 0x005B0290 (:364)
unsigned int parse::parse_string_to(char* str, unsigned int max_len, const char* keyword, const char* end_mark)
{
    // The text up to keyword, or the rest of the line when keyword is not found.
    char* const start = cur_ptr;
    if (!parse_skip_to(keyword, end_mark))
    {
        parse_line(str, max_len);
        return (unsigned int)strlen(str);
    }
    const size_t length = cur_ptr - start;
    strncpy(str, start, length);
    str[length] = '\0';
    return (unsigned int)length;
}

// 0x0058D620 (:388)
void parse::parse_save_position(parse_position_struct& pos)
{
    pos.line_num = line_num;
    pos.ptr = cur_ptr;
}

// 0x0058D640 (:398)
void parse::parse_restore_position(const parse_position_struct& pos)
{
    line_num = pos.line_num;
    cur_ptr = pos.ptr;
}

// 0x005CD230 (:415)
int parse::open(const char* fname)
{
    // Reads the file into Vlib_temp_mempool (released by close or the destructor). -1 if it cannot be opened.
    if (Vlib_temp_mempool == nullptr)
    {
        char message[512];
        sprintf(message, "Parse cannot open file %s: Vlib temp mempool was not assigned", fname);
        keen::debug::nativePrintArgs("%s", message);
    }
    strcpy(filename, fname);
    cfile* fp = cf_open(filename, "rb", PLATFORM_PC, false); // cf_open_internal(..., CF_IO_MEDIA_HDD) in the original
    if (fp == nullptr)
    {
        return -1;
    }
    size = fp->size;
    if (!Vlib_temp_mempool->can_alloc(size + 1, 0))
    {
        char message[512];
        sprintf(message, "Parse cannot open file %s: Vlib temp mempool not large enough", fname);
        keen::debug::nativePrintArgs("%s", message);
    }
    mempool_mark = Vlib_temp_mempool->mark();
    text = (char*)Vlib_temp_mempool->alloc(size + 1, 0);
    cf_read(text, size, fp, false);
    did_allocate = true;
    text[size] = '\0';
    cf_close(fp);
    reset();
    return 0;
}

// 0x005B02F0 (:460)
int parse::open_from_mem(const char* str)
{
    // Parses a copy of str (size includes the terminator).
    if (Vlib_temp_mempool == nullptr)
    {
        keen::debug::nativePrintArgs("Parse cannot copy buffer: Vlib temp mempool was not assigned");
    }
    const unsigned int length = (unsigned int)strlen(str) + 1;
    if (!Vlib_temp_mempool->can_alloc(length, 0))
    {
        keen::debug::nativePrintArgs("Parse cannot copy buffer: Vlib temp mempool was not large enough");
    }
    mempool_mark = Vlib_temp_mempool->mark();
    text = (char*)Vlib_temp_mempool->alloc(length, 0);
    size = length;
    did_allocate = true;
    strcpy(text, str);
    reset();
    return 0;
}

// 0x005B03D0 (:491)
int parse::open_from_mem_no_copy(char* str)
{
    text = str;
    did_allocate = false;
    reset();
    return 0;
}

// 0x0058D660 (:504)
void parse::close()
{
    if (text != nullptr && did_allocate)
    {
        Vlib_temp_mempool->restore_to_mark(mempool_mark);
    }
    text = nullptr;
}

// 0x0058D690 (:518)
void parse::reset()
{
    if (text != nullptr)
    {
        line_num = 1;
        cur_ptr = text;
    }
}

// 0x0058D6B0 (:531)
void parse::error(const char* str)
{
    // Prints the file, line, what was expected and the text around the position (control characters as 0x16).
    char next_text[44];
    unsigned int i;
    for (i = 0; i < 40; ++i)
    {
        const char c = cur_ptr[i];
        next_text[i] = (c == '\0' || c > 0x1F) ? c : '\x16';
    }
    next_text[i] = '\0';
    char prior_text[44];
    const char* p = cur_ptr - 40 < text ? text : cur_ptr - 40;
    for (i = 0; i < 40 && p != cur_ptr; ++i, ++p)
    {
        prior_text[i] = *p < ' ' ? '\x16' : *p;
    }
    prior_text[i] = '\0';
    // The original formats into 256 bytes with sprintf; snprintf keeps a long file name from overrunning it.
    char message[256];
    _snprintf(message, sizeof(message), "Parsing file \"%s\", at line #%zu\nExpected %s but found this text:\n\n[%s]\n\nPrior text was this:\n\n[%s]\n", filename, (size_t)line_num, str, next_text, prior_text);
    message[sizeof(message) - 1] = '\0';
    keen::debug::nativePrintArgs("%s", message);
}

// 0x0058D790 (:582)
bool parse::parse_line(char* str, unsigned int max_len)
{
    // The rest of the line; the line end is stepped over. Fails without moving when it does not fit.
    if (*cur_ptr == '\0')
    {
        *str = '\0';
        return false;
    }
    unsigned int length = 0;
    do
    {
        if (isNewLine(cur_ptr[length]))
        {
            break;
        }
        ++length;
    }
    while (cur_ptr[length] != '\0');
    if (length >= max_len)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    strncpy(str, cur_ptr, length);
    str[length] = '\0';
    cur_ptr += length;
    while (isNewLine(*cur_ptr))
    {
        if (*cur_ptr++ == '\n')
        {
            ++line_num;
        }
    }
    return true;
}

// 0x0058D830 (:623)
bool parse::parse_line_skip_comments(char* str, unsigned int max_len)
{
    parse_white();
    return parse_line(str, max_len);
}

// 0x005B0400 (:637)
bool parse::check_for(const char* str)
{
    const size_t length = strlen(str);
    parse_white();
    if (*cur_ptr == '\0')
    {
        return false;
    }
    return keen::compareStringNoCase(cur_ptr, str, (uint32)length) == 0;
}

// 0x005B0450 (:666)
bool parse::parse_optional(const char* str)
{
    const size_t length = strlen(str);
    parse_white();
    if (*cur_ptr == '\0')
    {
        return false;
    }
    if (keen::compareStringNoCase(cur_ptr, str, (uint32)length) != 0)
    {
        return false;
    }
    cur_ptr += length;
    return true;
}

// 0x0058D840 (:698)
bool parse::parse_optional_int(int* val)
{
    // Decimal or 0x hex, optionally negative; the position is restored when there are no digits.
    char* const saved_ptr = cur_ptr;
    const unsigned int saved_line = line_num;
    int value = 0;
    int num_digits = 0;
    parse_white();
    const char first = *cur_ptr;
    if (first != '\0')
    {
        if (first == '-')
        {
            ++cur_ptr;
        }
        if (cur_ptr[0] == '0' && (cur_ptr[1] == 'x' || cur_ptr[1] == 'X'))
        {
            cur_ptr += 2;
            while (*cur_ptr != '\0' && strchr("0123456789abcdefABCDEF", *cur_ptr) != nullptr)
            {
                const char c = *cur_ptr++;
                ++num_digits;
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
            }
        }
        else
        {
            while (*cur_ptr >= '0' && *cur_ptr <= '9')
            {
                value = value * 10 + (*cur_ptr++ - '0');
                ++num_digits;
            }
        }
        if (first == '-')
        {
            value = -value;
        }
        if (num_digits != 0)
        {
            *val = value;
            return true;
        }
    }
    line_num = saved_line;
    cur_ptr = saved_ptr;
    return false;
}

// 0x0058D970 (:770)
bool parse::parse_optional_float(float* val)
{
    // An integer part (or none before '.') and decimal digits; no exponent, no hex.
    char* const saved_ptr = cur_ptr;
    const unsigned int saved_line = line_num;
    int whole = 0;
    float fraction = 0.0f;
    parse_white();
    const char first = *cur_ptr;
    if (first == '\0')
    {
        line_num = saved_line;
        cur_ptr = saved_ptr;
        return false;
    }
    if (first == '-')
    {
        ++cur_ptr;
    }
    bool has_fraction = true;
    if (*cur_ptr != '.')
    {
        if ((cur_ptr[0] == '0' && (cur_ptr[1] == 'x' || cur_ptr[1] == 'X')) || !parse_optional_int(&whole))
        {
            line_num = saved_line;
            cur_ptr = saved_ptr;
            return false;
        }
        has_fraction = *cur_ptr == '.';
    }
    if (has_fraction)
    {
        ++cur_ptr;
        float scale = 0.1f;
        while (*cur_ptr >= '0' && *cur_ptr <= '9')
        {
            fraction += (float)(*cur_ptr++ - '0') * scale;
            scale = (float)((double)scale * (double)0.1f);
        }
    }
    *val = (float)whole + fraction;
    if (first == '-')
    {
        *val = -((float)whole + fraction);
    }
    return true;
}

// 0x005B04A0 (:852)
bool parse::parse_optional_string(const char* str)
{
    // "str" (quoted, case-insensitive).
    parse_white();
    char* const start = cur_ptr;
    if (*start != '"')
    {
        return false;
    }
    const unsigned int saved_line = line_num;
    ++cur_ptr;
    const size_t length = strlen(str);
    parse_white();
    bool match = false;
    if (*cur_ptr != '\0' && keen::compareStringNoCase(cur_ptr, str, (uint32)length) == 0)
    {
        cur_ptr += length;
        match = *cur_ptr == '"';
    }
    line_num = saved_line;
    cur_ptr = start;
    if (!match)
    {
        return false;
    }
    cur_ptr = start + length + 2;
    return true;
}

// 0x005B0560 (:901)
void parse::parse_required(const char* str)
{
    const size_t length = strlen(str);
    parse_white();
    if (*cur_ptr != '\0' && keen::compareStringNoCase(cur_ptr, str, (uint32)length) == 0)
    {
        cur_ptr += length;
        return;
    }
    char expected[256];
    sprintf(expected, "\"%s\"", str);
    error(expected);
}

// 0x005B0600 (:916)
bool parse::parse_bool()
{
    // true/yes or false/no.
    parse_white();
    if (parse_optional("true")) // inlined in the original
    {
        return true;
    }
    if (parse_optional("yes"))
    {
        return true;
    }
    if (!parse_optional("false") && !parse_optional("no"))
    {
        error("a boolean value");
    }
    return false;
}

// 0x005B06A0 (:939)
int parse::parse_int()
{
    parse_white();
    const bool negative = *cur_ptr == '-';
    if (negative)
    {
        ++cur_ptr;
    }
    const unsigned int value = parse_uint();
    return negative ? -(int)value : (int)value;
}

// 0x0058DAC0 (:968)
unsigned int parse::parse_uint()
{
    int num_digits = 0;
    unsigned int value = 0;
    parse_white();
    if (cur_ptr[0] == '0' && (cur_ptr[1] == 'x' || cur_ptr[1] == 'X'))
    {
        cur_ptr += 2;
        while (*cur_ptr != '\0' && strchr("0123456789abcdefABCDEF", *cur_ptr) != nullptr)
        {
            const char c = *cur_ptr++;
            ++num_digits;
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
        }
    }
    else
    {
        while (*cur_ptr >= '0' && *cur_ptr <= '9')
        {
            value = value * 10 + (*cur_ptr++ - '0');
            ++num_digits;
        }
    }
    if (num_digits == 0)
    {
        error("a uint32");
    }
    return value;
}

// 0x005B06D0 (:1012)
float parse::parse_float()
{
    parse_white();
    const bool negative = *cur_ptr == '-';
    if (negative)
    {
        ++cur_ptr;
    }
    int whole = 0;
    if (*cur_ptr != '.')
    {
        if (cur_ptr[0] == '0' && (cur_ptr[1] == 'x' || cur_ptr[1] == 'X'))
        {
            error("a float");
        }
        whole = parse_int(); // inlined in the original
        if (*cur_ptr != '.')
        {
            return (float)(negative ? -whole : whole);
        }
    }
    float fraction = 0.0f;
    float scale = 0.1f;
    ++cur_ptr;
    while (*cur_ptr >= '0' && *cur_ptr <= '9')
    {
        fraction += (float)(*cur_ptr++ - '0') * scale;
        scale = (float)((double)scale * (double)0.1f);
    }
    if (negative)
    {
        fraction = -fraction;
        whole = -whole;
    }
    return (float)whole + fraction;
}

// 0x005B0810 (:1074)
void parse::parse_vector(vector& vec)
{
    // <x, y, z> (the commas are optional).
    parse_white();
    if (*cur_ptr != '<')
    {
        error("a vector");
    }
    ++cur_ptr;
    vec.x = parse_float();
    parse_optional(","); // inlined in the original
    vec.y = parse_float();
    parse_optional(",");
    vec.z = parse_float();
    parse_white();
    if (*cur_ptr != '>')
    {
        error("a vector terminator");
    }
    ++cur_ptr;
}

// 0x005B08D0 (:1107)
void parse::parse_matrix(matrix& mat)
{
    // [I], [<rvec>, <uvec>, <fvec>] or [heading].
    parse_white();
    if (*cur_ptr != '[')
    {
        error("a matrix");
    }
    ++cur_ptr;
    parse_white();
    if (parse_optional("I"))
    {
        mat = IDENTITY_MATRIX;
    }
    else if (*cur_ptr == '<')
    {
        parse_vector(mat.rvec);
        parse_optional(",");
        parse_vector(mat.uvec);
        parse_optional(",");
        parse_vector(mat.fvec);
    }
    else
    {
        mat.set(0.0f, 0.0f, parse_float());
    }
    parse_white();
    if (*cur_ptr != ']')
    {
        error("a matrix terminator");
    }
    ++cur_ptr;
}

// 0x0058DB90 (:1152)
unsigned int parse::parse_string(char* str, unsigned int max_len, char string_begin_char, char string_end_char)
{
    // A delimited string on one line.
    parse_white();
    if (*cur_ptr != string_begin_char)
    {
        error("a string");
    }
    ++cur_ptr;
    unsigned int length = 0;
    while (cur_ptr[length] != '\0' && !isNewLine(cur_ptr[length]) && cur_ptr[length] != string_end_char)
    {
        ++length;
    }
    if (cur_ptr[length] != string_end_char)
    {
        error("a string terminator");
    }
    if (length >= max_len)
    {
        char message[256];
        sprintf(message, "Not enough storage provided (%zu)\nfor parsed C string (len=%zu)", (size_t)max_len, (size_t)length);
        keen::debug::nativePrintArgs("%s", message);
    }
    strncpy(str, cur_ptr, length);
    str[length] = '\0';
    cur_ptr += length + 1;
    return length;
}

// 0x005B09F0 (:1193)
int parse::parse_string_localized(char* str, unsigned int max_len, char string_begin_char, char string_end_char)
{
    // XSTR(id, "text"); returns the id.
    parse_required("XSTR(");
    const int id = parse_int();
    parse_required(",");
    parse_string(str, max_len, string_begin_char, string_end_char);
    parse_required(")");
    return id;
}

// 0x0058DC70 (:1213)
unsigned int parse::parse_string_alphanumeric(char* str, unsigned int max_len)
{
    parse_white();
    size_t length = 0;
    while (cur_ptr[length] != '\0' && (isalnum(cur_ptr[length]) || cur_ptr[length] == '_'))
    {
        ++length;
    }
    strncpy(str, cur_ptr, length);
    str[length] = '\0';
    cur_ptr += length;
    return (unsigned int)length;
}

// 0x0058DCE0 (:1249)
unsigned int parse::parse_string_list(char* list, unsigned int max_string_len, unsigned int list_max)
{
    // ("a" "b" ...) into consecutive max_string_len slots; list_max is not checked.
    parse_white();
    if (*cur_ptr != '(')
    {
        error("a string list");
    }
    ++cur_ptr;
    parse_white();
    unsigned int count = 0;
    while (*cur_ptr != '\0' && *cur_ptr != ')')
    {
        parse_string(list, max_string_len, '"', '"');
        list += max_string_len;
        ++count;
        parse_white();
    }
    if (*cur_ptr != ')')
    {
        error("a string list terminator");
    }
    ++cur_ptr;
    return count;
}

// 0x005B0A60 (:1297)
unsigned int parse::parse_int_list(int* list, unsigned int list_max, bool (*func)(int&, unsigned int, const char*))
{
    // (1 2 "name" ...): quoted elements are converted by func(value, index, text). list_max is not checked.
    parse_white();
    if (*cur_ptr != '(')
    {
        error("an integer list");
    }
    ++cur_ptr;
    parse_white();
    unsigned int count = 0;
    while (*cur_ptr != '\0' && *cur_ptr != ')')
    {
        if (*cur_ptr == '"')
        {
            char* const start = cur_ptr;
            const unsigned int saved_line = line_num;
            char element[256];
            parse_string(element, sizeof(element), '"', '"');
            if (func == nullptr || !func(list[count], count, element))
            {
                line_num = saved_line;
                cur_ptr = start;
                error("an integer list element");
            }
        }
        else
        {
            list[count] = parse_int();
        }
        ++count;
        parse_white();
    }
    if (*cur_ptr != ')')
    {
        error("an integer list terminator");
    }
    ++cur_ptr;
    return count;
}

// 0x005B0BA0 (:1358)
unsigned int parse::parse_float_list(float* list, unsigned int list_max)
{
    parse_white();
    if (*cur_ptr != '(')
    {
        error("a float list");
    }
    ++cur_ptr;
    parse_white();
    unsigned int count = 0;
    while (*cur_ptr != '\0' && *cur_ptr != ')')
    {
        list[count++] = parse_float();
        parse_white();
    }
    if (*cur_ptr != ')')
    {
        error("a float list terminator");
    }
    ++cur_ptr;
    return count;
}

// 0x005B0C20 (:1403)
int parse::parse_flags(const char** list, unsigned int list_size)
{
    // ("flag" ...): bit i for list[i]. A null list entry ends the list and accepts any name.
    unsigned int flags = 0;
    parse_white();
    if (*cur_ptr != '(')
    {
        error("a flags list");
    }
    ++cur_ptr;
    parse_white();
    while (*cur_ptr != '\0' && *cur_ptr != ')')
    {
        char* const start = cur_ptr;
        const unsigned int saved_line = line_num;
        char name[256];
        parse_string(name, sizeof(name), '"', '"');
        bool known = false;
        for (unsigned int i = 0; i < list_size; ++i)
        {
            if (list[i] == nullptr)
            {
                known = true;
                break;
            }
            if (keen::compareStringNoCase(name, list[i]) == 0)
            {
                flags |= 1u << (i & 31);
                known = true;
                break;
            }
        }
        if (!known)
        {
            cur_ptr = start;
            line_num = saved_line;
            error("a valid flag name");
        }
        parse_white();
    }
    if (*cur_ptr != ')')
    {
        error("a flags list terminator");
    }
    ++cur_ptr;
    return (int)flags;
}

// 0x005B0D50 (:1458)
unsigned int parse::parse_type_from_list(const char* const* list, unsigned int list_size)
{
    // Index of the quoted name in list; 0 (after an error) if it is not there.
    parse_white();
    char* const start = cur_ptr;
    const unsigned int saved_line = line_num;
    char name[256];
    parse_string(name, sizeof(name), '"', '"');
    for (unsigned int i = 0; i < list_size && list[i] != nullptr; ++i)
    {
        if (keen::compareStringNoCase(name, list[i]) == 0)
        {
            return i;
        }
    }
    line_num = saved_line;
    cur_ptr = start;
    error("a valid type string");
    return 0;
}

// 0x0058DD70 (:1495)
void parse_bool2s(char* str, unsigned int size, bool b)
{
    strcpy(str, b ? "true" : "false");
}

// 0x0058DDB0 (:1519)
static void append_value_as_string(char* str, float f, int precision, const char* end)
{
    // Whole numbers without decimals.
    char value[64];
    if (f == (float)(int)f)
    {
        sprintf(value, "%d%s", (int)f, end);
    }
    else
    {
        sprintf(value, "%.*f%s", precision, (double)f, end);
    }
    strcat(str, value);
}

// 0x0058DE60 (:1538)
void parse_vec2s(char* str, unsigned int size, const vector& vec, int prec)
{
    strcpy(str, "<");
    append_value_as_string(str, vec.x, prec, " ");
    append_value_as_string(str, vec.y, prec, " ");
    append_value_as_string(str, vec.z, prec, "");
    strcat(str, ">");
}

// 0x0058E520 (:1594)
bool parse::parse_eof()
{
    return *cur_ptr == '\0';
}

// 0x005B0E10 (:1603)
bool parse::parse_xml_required_string(const char* tag, char* str, unsigned int max_len)
{
    // <tag>text</tag>; without the close tag the rest of the line is taken.
    char open_tag[256];
    char close_tag[256];
    get_xml_tags(tag, open_tag, close_tag); // inlined in the original
    parse_required(open_tag);
    char* const start = cur_ptr;
    size_t length;
    if (parse_skip_to(close_tag, nullptr))
    {
        length = cur_ptr - start;
        strncpy(str, start, length);
        str[length] = '\0';
    }
    else
    {
        parse_line(str, max_len);
        length = strlen(str);
    }
    if (length != 0)
    {
        parse_required(close_tag);
    }
    return length != 0;
}

// 0x005B0EF0 (:1624)
bool parse::parse_xml_optional_string(const char* tag, char* str, unsigned int max_len)
{
    char open_tag[256];
    char close_tag[256];
    get_xml_tags(tag, open_tag, close_tag); // inlined in the original
    if (!parse_optional(open_tag))
    {
        return false;
    }
    char* const start = cur_ptr;
    size_t length;
    if (parse_skip_to(close_tag, nullptr))
    {
        length = cur_ptr - start;
        strncpy(str, start, length);
        str[length] = '\0';
    }
    else
    {
        parse_line(str, max_len);
        length = strlen(str);
    }
    if (length == 0)
    {
        return false;
    }
    parse_required(close_tag);
    return true;
}

// 0x005B1000 (:1687)
unsigned int parse::parse_xml_count(const char* tag)
{
    // Counts the <tag> elements at the current nesting level up to the enclosing close tag; the position is restored.
    char close_tag[256];
    sprintf(close_tag, "/%s", tag); // unused
    const unsigned int saved_line = line_num;
    char* const saved_ptr = cur_ptr;
    unsigned int count = 0;
    int depth = 0;
    bool in_tag = false;
    while (parse_skip_to("<", nullptr))
    {
        char name[256];
        parse_string(name, sizeof(name), '<', '>');
        if (name[0] == '/')
        {
            if (depth == 0)
            {
                break;
            }
            --depth;
            if (depth == 0 && in_tag)
            {
                ++count;
                in_tag = false;
            }
        }
        else
        {
            if (depth == 0 && keen::compareStringNoCase(tag, name) == 0)
            {
                in_tag = true;
            }
            ++depth;
        }
    }
    line_num = saved_line;
    cur_ptr = saved_ptr;
    return count;
}
