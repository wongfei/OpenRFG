// Original: volition/vlib/code/parse/parse.h
// Implementation: volition/vlib/code/parse/parse.cpp
//
// Text tokenizer for the older text formats (console scripts, gamestart.txt and similar). parse::open reads a whole
// file into Vlib_temp_mempool (mempool_mark keeps the mark to release it); open_from_mem copies a string there,
// open_from_mem_no_copy works in place. Errors are reported with the file name and line ("Parsing file ... at line #N").
// Comments are skipped by parse_gray/parse_white; parse_string_localized accepts XSTR(...) strings.
// The parse_xml_* members are a minimal reader for <tag>value</tag> pairs inside such text files.
#pragma once

class vector;
class matrix;

// size 0x8
struct parse_position_struct
{
    unsigned int line_num; // 0x00
    char* ptr; // 0x04
};

// size 0x9C
class parse
{
    void error(const char* str); // 0x0058D6B0 (:531)
    void parse_comment(); // 0x0058D2D0 (:70)
public:
    parse(); // 0x0058D2B0 (:53)
    ~parse(); // 0x005B01F0 (:63)
    int open(const char* fname); // 0x005CD230 (:415)
    int open_from_mem(const char* str); // 0x005B02F0 (:460)
    int open_from_mem_no_copy(char* str); // 0x005B03D0 (:491)
    void close(); // 0x0058D660 (:504)
    void reset(); // 0x0058D690 (:518)
    bool is_comment(const char* pos); // 0x0058D4D0 (:208)
    void parse_gray(); // 0x0058D310 (:88)
    void parse_white(); // 0x0058D3C0 (:119)
    void parse_skip_line(); // 0x0058D490 (:156)
    bool parse_wrapped_skip_to(const char* keyword); // 0x005B0220 (:178)
    bool parse_skip_to(const char* keyword, const char* end_mark); // 0x0058D560 (:266)
    bool parse_wrapped_skip_over(const char* keyword); // 0x005BE2E0 (:324)
    bool parse_skip_over(const char* keyword, const char* end_mark); // 0x005BE350 (:343)
    void parse_save_position(parse_position_struct& pos); // 0x0058D620 (:388)
    void parse_restore_position(const parse_position_struct& pos); // 0x0058D640 (:398)
    bool check_for(const char* str); // 0x005B0400 (:637)
    bool parse_optional(const char* str); // 0x005B0450 (:666)
    void parse_required(const char* str); // 0x005B0560 (:901)
    bool parse_line(char* str, unsigned int max_len); // 0x0058D790 (:582)
    bool parse_line_skip_comments(char* str, unsigned int max_len); // 0x0058D830 (:623)
    bool parse_bool(); // 0x005B0600 (:916)
    int parse_int(); // 0x005B06A0 (:939)
    unsigned int parse_uint(); // 0x0058DAC0 (:968)
    float parse_float(); // 0x005B06D0 (:1012)
    void parse_vector(vector& vec); // 0x005B0810 (:1074)
    void parse_matrix(matrix& mat); // 0x005B08D0 (:1107)
    bool parse_optional_int(int* val); // 0x0058D840 (:698)
    bool parse_optional_float(float* val); // 0x0058D970 (:770)
    bool parse_optional_string(const char* str); // 0x005B04A0 (:852)
    unsigned int parse_string(char* str, unsigned int max_len, char string_begin_char, char string_end_char); // 0x0058DB90 (:1152)
    unsigned int parse_string_to(char* str, unsigned int max_len, const char* keyword, const char* end_mark); // 0x005B0290 (:364)
    int parse_string_localized(char* str, unsigned int max_len, char string_begin_char, char string_end_char); // 0x005B09F0 (:1193)
    unsigned int parse_string_alphanumeric(char* str, unsigned int max_len); // 0x0058DC70 (:1213)
    unsigned int parse_int_list(int* list, unsigned int list_max, bool (*func)(int&, unsigned int, const char*)); // 0x005B0A60 (:1297)
    unsigned int parse_float_list(float* list, unsigned int list_max); // 0x005B0BA0 (:1358)
    int parse_flags(const char** list, unsigned int list_size); // 0x005B0C20 (:1403)
    unsigned int parse_type_from_list(const char* const * list, unsigned int list_size); // 0x005B0D50 (:1458)
    unsigned int parse_string_list(char* list, unsigned int max_string_len, unsigned int list_max); // 0x0058DCE0 (:1249)
    bool parse_eof(); // 0x0058E520 (:1594)
    bool parse_xml_required_string(const char* tag, char* str, unsigned int max_len); // 0x005B0E10 (:1603)
    bool parse_xml_optional_string(const char* tag, char* str, unsigned int max_len); // 0x005B0EF0 (:1624)
    unsigned int parse_xml_count(const char* tag); // 0x005B1000 (:1687)
    unsigned int get_line();

private:
    char* text; // 0x00
    char* cur_ptr; // 0x04
    char filename[129]; // 0x08
    unsigned int line_num; // 0x8C
    unsigned int size; // 0x90
    bool did_allocate; // 0x94
    unsigned int mempool_mark; // 0x98
};

void parse_bool2s(char* str, unsigned int size, bool b); // 0x0058DD70 (:1495)
void parse_vec2s(char* str, unsigned int size, const vector& vec, int prec); // 0x0058DE60 (:1538)
void parse_mat2s(char* str, unsigned int size, const matrix& mat, int prec); // 0x0058DEF0 (:1556)
bool isNewLine(char c); // 0x0058C5E0 (.h:27)
