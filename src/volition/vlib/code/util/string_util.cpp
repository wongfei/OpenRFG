// Original: volition/vlib/code/util/string_util.cpp
//
// String helpers. Functions that return a string without an output buffer (str_commas, str_repeat) use a ring of 16
// static buffers of 513 bytes, so a result stays valid for the next 15 such calls. Whitespace is ' ', '\t', '\n' and
// '\r'. The *_safe functions always terminate the destination. str_match patterns know '*' (any run) and '?' (any
// character).

#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/color.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// color's named constants (color.h is inline only; placement inferred, UI.md UI5: the menus colour locked items red).
// Read from .rdata (0x0163EA00), in address order.
const color color::BLACK(0, 0, 0, 255); // 0x0163EA00
const color color::WHITE(255, 255, 255, 255); // 0x0163EA04
const color color::RED(255, 0, 0, 255); // 0x0163EA08
const color color::GREEN(0, 255, 0, 255); // 0x0163EA0C
const color color::BLUE(0, 0, 255, 255); // 0x0163EA10
const color color::YELLOW(255, 255, 0, 255); // 0x0163EA14
const color color::CYAN(0, 255, 255, 255); // 0x0163EA18
const color color::MAGENTA(255, 0, 255, 255); // 0x0163EA1C
const color color::ORANGE(255, 128, 0, 255); // 0x0163EA20
const color color::GREY(128, 128, 128, 255); // 0x0163EA24
const color color::MAROON(128, 0, 0, 255); // 0x0163EA28
const color color::DARKGREEN(0, 128, 0, 255); // 0x0163EA2C
const color color::NAVY(0, 0, 128, 255); // 0x0163EA30
const color color::OLIVE(128, 128, 0, 255); // 0x0163EA34
const color color::TEAL(0, 128, 128, 255); // 0x0163EA38
const color color::BROWN(165, 42, 42, 255); // 0x0163EA3C
const color color::VIOLET(238, 130, 238, 255); // 0x0163EA40
const color color::TURQUOISE(64, 224, 208, 255); // 0x0163EA44
const color color::SILVER(192, 192, 192, 255); // 0x0163EA48
const color color::PINK(255, 192, 203, 255); // 0x0163EA4C

static char Static_strs[16][513]; // file static (inferred: not recorded in the PDB)
static int Static_str_next; // file static (inferred)

static inline bool is_white_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// 0x00592A90 (:36)
static char* next_static_string()
{
    char* s = Static_strs[Static_str_next];
    Static_str_next = Static_str_next == 15 ? 0 : Static_str_next + 1;
    return s;
}

// 0x00592AC0 (:56)
bool str_blank(const char* text)
{
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
    {
        if (!is_white_space(text[i]))
        {
            return false;
        }
    }
    return true;
}

// 0x00592B10 (:81)
void str_ltrim(char* output, const char* text)
{
    const size_t length = strlen(text);
    size_t start = 0;
    while (start < length && is_white_space(text[start]))
    {
        ++start;
    }
    strcpy(output, text + start);
}

// 0x00592B70 (:105)
void str_rtrim(char* output, const char* text)
{
    int count = (int)strlen(text);
    while (count - 1 >= 0 && is_white_space(text[count - 1]))
    {
        --count;
    }
    strncpy(output, text, count);
    output[count] = '\0';
}

// 0x00592BD0 (:134)
void str_trim(char* output, const char* text, int max_len)
{
    // max_len is not used.
    const int length = (int)strlen(text);
    int start = 0;
    while (start < length && is_white_space(text[start]))
    {
        ++start;
    }
    if (start == length)
    {
        *output = '\0';
        return;
    }
    int last = length - 1;
    while (last >= 0 && is_white_space(text[last]))
    {
        --last;
    }
    const size_t count = (size_t)(last - start + 1);
    strncpy(output, text + start, count);
    output[count] = '\0';
}

// Appends n spaces (inlined strcat loops in the original).
static void append_spaces(char* outbuf, int n)
{
    for (; n > 0; --n)
    {
        strcat(outbuf, " ");
    }
}

// 0x00592C60 (:189)
void str_sprintf_rj(char* outbuf, int left_indent, int just_col, const char* tag, const char* format, ...)
{
    // "<indent><tag><padding><value>" with the value right-aligned to just_col (at least one space).
    char value[2048];
    va_list arguments;
    va_start(arguments, format);
    vsprintf(value, format, arguments);
    va_end(arguments);
    const int value_length = (int)strlen(value);
    *outbuf = '\0';
    append_spaces(outbuf, left_indent);
    strcat(outbuf, tag);
    const int length = (int)strlen(outbuf);
    append_spaces(outbuf, length + value_length < just_col ? just_col - length - value_length : 1);
    strcat(outbuf, value);
}

// 0x00592DA0 (:249)
void str_sprintf_lj(char* outbuf, int left_indent, int just_col, const char* tag, const char* format, ...)
{
    // "<indent><tag><padding><value>" with the value starting at just_col (at least one space).
    char value[2048];
    va_list arguments;
    va_start(arguments, format);
    vsprintf(value, format, arguments);
    va_end(arguments);
    *outbuf = '\0';
    append_spaces(outbuf, left_indent);
    strcat(outbuf, tag);
    const int length = (int)strlen(outbuf);
    append_spaces(outbuf, length < just_col ? just_col - length : 1);
    strcat(outbuf, value);
}

// Inserts a comma every three digits of a decimal string (shared by the two converters below).
static char* comma_delimit(char* output, int output_size, const char* digits, int sign_length)
{
    const int length = (int)strlen(digits);
    const int digit_count = length - sign_length;
    const int comma_count = (digit_count - 1) / 3;
    if (comma_count + length >= output_size)
    {
        return nullptr;
    }
    if (comma_count == 0)
    {
        strcpy(output, digits);
        return output;
    }
    int first_group = digit_count % 3;
    if (first_group == 0)
    {
        first_group = 3;
    }
    size_t source = (size_t)(sign_length + first_group);
    strncpy(output, digits, source);
    size_t dest = source;
    for (; (int)source < length; source += 3)
    {
        output[dest] = ',';
        strncpy(output + dest + 1, digits + source, 3);
        dest += 4;
    }
    output[dest] = '\0';
    return output;
}

// 0x00592EC0 (:298)
char* str_int_to_comma_delimited_string(char* output, int output_size, int number)
{
    char digits[32];
    sprintf(digits, "%d", number);
    return comma_delimit(output, output_size, digits, number < 0 ? 1 : 0);
}

// 0x00592FE0 (:360)
char* str_uint64_to_comma_delimited_string(char* output, int output_size, uint64_t number)
{
    char digits[32];
    sprintf(digits, "%llu", number);
    return comma_delimit(output, output_size, digits, 0);
}

// 0x005930F0 (:408)
char* str_commas(uint64_t number)
{
    return str_uint64_to_comma_delimited_string(next_static_string(), 512, number);
}

// 0x00593140 (:414)
char* str_commas(unsigned int number)
{
    return str_uint64_to_comma_delimited_string(next_static_string(), 512, number);
}

// 0x00593190 (:420)
char* str_commas(int number)
{
    return str_int_to_comma_delimited_string(next_static_string(), 512, number);
}

// 0x005931F0 (:427)
int str_to_int_commas(const char* str)
{
    char digits[128];
    int length = 0;
    for (; *str != '\0'; ++str)
    {
        if (*str != ',')
        {
            digits[length++] = *str;
        }
    }
    digits[length] = '\0';
    return atoi(digits);
}

// 0x00593250 (:452)
bool str_is_number(const char* text)
{
    // Leading white space, an optional sign, digits with at most one '.', and an optional exponent with a sign.
    // Trailing characters after a valid prefix are checked too; an empty or blank string counts as a number.
    const int length = (int)strlen(text);
    bool has_exponent = false;
    bool has_exponent_sign = false;
    bool has_point = false;
    int i = 0;
    if (length <= 0)
    {
        return true;
    }
    while (is_white_space(text[i]))
    {
        if (++i >= length)
        {
            return true;
        }
    }
    if (text[i] == '-' || text[i] == '+')
    {
        ++i;
    }
    for (; i < length; ++i)
    {
        switch (text[i])
        {
        case '+':
        case '-':
            if (!has_exponent || has_exponent_sign || (text[i - 1] != 'e' && text[i - 1] != 'E'))
            {
                return false;
            }
            has_exponent_sign = true;
            break;
        case '.':
            if (has_point)
            {
                return false;
            }
            has_point = true;
            break;
        case 'E':
        case 'e':
            if (has_exponent)
            {
                return false;
            }
            has_exponent = true;
            break;
        default:
            if (!isdigit(text[i]))
            {
                return false;
            }
            break;
        }
    }
    return true;
}

// 0x00593390 (:533)
void str_ccat(char* str, char c)
{
    const size_t length = strlen(str);
    str[length] = c;
    str[length + 1] = '\0';
}

// 0x005933C0 (:548)
char* str_stri(const char* str, const char* substr)
{
    // Case-insensitive search. A mismatch restarts the match at the next character without backtracking.
    const char* match = nullptr;
    const char* p = substr;
    for (; *str != '\0' && *p != '\0'; ++str)
    {
        if (tolower(*str) == tolower(*p))
        {
            if (match == nullptr)
            {
                match = str;
            }
            ++p;
        }
        else if (match != nullptr)
        {
            match = nullptr;
            p = substr;
        }
    }
    return *p == '\0' ? (char*)match : nullptr;
}

// 0x00593420 (:576)
void str_make_lower(char* str)
{
    const int length = (int)strlen(str);
    for (int i = 0; i < length; ++i)
    {
        str[i] = (char)tolower(str[i]);
    }
}

// 0x00593460 (:590)
void str_make_upper(char* str)
{
    const int length = (int)strlen(str);
    for (int i = 0; i < length; ++i)
    {
        str[i] = (char)toupper(str[i]);
    }
}

// 0x005B25F0 (:606)
void str_copy_to_lower(char* dest, unsigned int dest_array_size, const char* source)
{
    unsigned int length = (unsigned int)strlen(source);
    if (length > dest_array_size - 1)
    {
        length = dest_array_size - 1;
    }
    for (unsigned int i = 0; i < length; ++i)
    {
        dest[i] = (char)tolower(source[i]);
    }
    dest[length] = '\0';
}

// 0x005B2650 (:626)
void str_copy_to_upper(char* dest, unsigned int dest_array_size, const char* source)
{
    unsigned int length = (unsigned int)strlen(source);
    if (length > dest_array_size - 1)
    {
        length = dest_array_size - 1;
    }
    for (unsigned int i = 0; i < length; ++i)
    {
        dest[i] = (char)toupper(source[i]);
    }
    dest[length] = '\0';
}

// 0x005934A0 (:646)
void str_i2s(char* output, int max_size, int i)
{
    // max_size is not used.
    char buffer[32];
    sprintf(buffer, "%d", i);
    strcpy(output, buffer);
}

// 0x005934F0 (:662)
void str_i2s_hex(char* output, int max_size, int i)
{
    char buffer[32];
    sprintf(buffer, "0x%x", i);
    strcpy(output, buffer);
}

// 0x00593540 (:679)
void str_f2s(char* output, int max_size, float f, int prec)
{
    char buffer[32];
    sprintf(buffer, "%.*f", prec, (double)f);
    strcpy(output, buffer);
}

// 0x005935A0 (:701)
void str_size2s(char* output, int max_size, unsigned int size)
{
    char buffer[64];
    if (size < 0x400)
    {
        sprintf(buffer, "%d bytes", size);
    }
    else
    {
        const float kilobytes = (float)size * (1.0f / 1024.0f);
        if (kilobytes >= 1024.0f)
        {
            sprintf(buffer, "%.2f MB", (double)(float)(kilobytes * (1.0 / 1024.0)));
        }
        else
        {
            sprintf(buffer, "%.2f KB", (double)kilobytes);
        }
    }
    strcpy(output, buffer);
}

// 0x00593670 (:730)
void str_v2s(char* output, int max_size, const vector* v, int precision)
{
    char buffer[256];
    sprintf(buffer, "< %.*f, %.*f, %.*f >", precision, (double)v->x, precision, (double)v->y, precision, (double)v->z);
    strcpy(output, buffer);
}

// 0x00593710 (:748)
void str_x2s(char* output, int max_size, void* addr)
{
    char buffer[32];
    sprintf(buffer, "0x%p", addr);
    strcpy(output, buffer);
}

// 0x00593760 (:765)
void str_color2s_hex(char* output, int max_size, const color& col, bool include_prefix)
{
    char buffer[32];
    sprintf(buffer, include_prefix ? "0x%.2X%.2X%.2X%.2X" : "%.2X%.2X%.2X%.2X", (unsigned int)col.red, (unsigned int)col.green, (unsigned int)col.blue, (unsigned int)col.alpha);
    strcpy(output, buffer);
}

// 0x005937D0 (:786)
unsigned int str_hash_string(const char* str)
{
    // Case-insensitive rotate-xor hash (the packfile name hash uses the same function).
    if (str == nullptr)
    {
        return 0xFFFFFFFFu;
    }
    unsigned int hash = 0;
    for (; *str != '\0'; ++str)
    {
        unsigned int c = (unsigned int)(int)*str;
        if (c - 'A' < 26)
        {
            c += 0x20;
        }
        hash = ((hash << 6) | (hash >> 26)) ^ c;
    }
    return hash;
}

// 0x00593810 (:820)
int _snprintf_safe(char* dest, unsigned int dest_count, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const int result = _vsnprintf(dest, dest_count, format, arguments);
    va_end(arguments);
    dest[dest_count - 1] = '\0';
    return result;
}

// 0x00593840 (:857)
int _vsnprintf_safe(char* dest, unsigned int dest_count, const char* format, char* arg_list)
{
    const int result = _vsnprintf(dest, dest_count, format, (va_list)arg_list);
    dest[dest_count - 1] = '\0';
    return result;
}

// 0x00593870 (:884)
char* strncpy_safe(char* dest, const char* src, unsigned int dest_count)
{
    char* result = strncpy(dest, src, dest_count);
    dest[dest_count - 1] = '\0';
    return result;
}

// 0x005938A0 (:899)
static bool str_match_internal(const char* name, const char* pattern, bool case_insensitive)
{
    // After a '*', a mismatch backs up to the character after the star and tries the next name position.
    int pattern_index = 0;
    int name_index = 0;
    int matched_since_star = 0;
    bool after_star = false;
    while (name[name_index] != '\0')
    {
        char p = pattern[pattern_index];
        if (p == '*')
        {
            p = pattern[++pattern_index];
            matched_since_star = 0;
            after_star = true;
            if (p == '\0')
            {
                break;
            }
        }
        if (name[name_index] == p || p == '?' || (case_insensitive && tolower(name[name_index]) == tolower(pattern[pattern_index])))
        {
            ++pattern_index;
            if (after_star)
            {
                ++matched_since_star;
            }
        }
        else
        {
            if (!after_star)
            {
                return false;
            }
            name_index -= matched_since_star;
            pattern_index -= matched_since_star;
            matched_since_star = 0;
        }
        ++name_index;
    }
    while (pattern[pattern_index] == '*')
    {
        ++pattern_index;
    }
    return pattern[pattern_index] == '\0';
}

// 0x00593970 (:952)
bool str_match(const char* name, const char* pattern)
{
    return str_match_internal(name, pattern, false);
}

// 0x00593990 (:958)
bool str_matchi(const char* name, const char* pattern)
{
    return str_match_internal(name, pattern, true);
}

// 0x005B26B0 (:973)
char* strcat_safe(char* dest, const char* src, unsigned int dest_count)
{
    const unsigned int remaining = dest_count - (unsigned int)strlen(dest) - 1;
    char* result = strncat(dest, src, remaining != 0 ? remaining : 0);
    dest[dest_count - 1] = '\0';
    return result;
}

// 0x005939B0 (:997)
char* str_repeat(char c, int n)
{
    char* s = next_static_string();
    s[0] = '\0';
    if (n <= 512)
    {
        int length = 0;
        if (n > 0)
        {
            memset(s, c, n);
            length = n;
        }
        s[length] = '\0';
    }
    return s;
}
