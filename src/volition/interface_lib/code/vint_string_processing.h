// Original: volition/interface_lib/code/vint_string_processing.h
// Implementation: volition/interface_lib/code/vint_string_processing.cpp
//
// Text markup and layout: the [format], [color], [image] and [scale] tags, insertion formats, the render instructions a
// parse produces (color, image, text runs), line breaking into frames, alignment and forced case. Placement of the
// types inferred.
#pragma once

#include "volition/interface_lib/code/vint_external.h"
#include "volition/vlib/code/math/vector2.h"

class matrix23;
class vint_line_frame;
class vint_resource_font;
struct vint_text_render_params;
struct vint_variant_color;

enum vint_text_force_case
{
    VINT_TEXT_CASE_NONE = 0,
    VINT_TEXT_CASE_UPPER = 1,
    VINT_TEXT_CASE_LOWER = 2,
};

enum vint_text_horz_align
{
    VINT_TEXT_HALIGN_LEFT = 0,
    VINT_TEXT_HALIGN_CENTER = 1,
    VINT_TEXT_HALIGN_RIGHT = 2,
};

enum vint_text_vert_align
{
    VINT_TEXT_VALIGN_TOP = 0,
    VINT_TEXT_VALIGN_CENTER = 1,
    VINT_TEXT_VALIGN_BOTTOM = 2,
};

enum vint_insersion_format
{
    VINT_INSERSION_NO_FORMATTING = -1,
    VINT_INSERSION_TEXT_TAG_CRC = 0,
    VINT_INSERSION_NOT_LOCALIZED = 1,
    NUM_VINT_INSERSION_FORMATS = 2,
};

enum vint_render_instruction
{
    VINT_RENDER_STOP = 0,
    VINT_RENDER_MOVE_ABS = 1,
    VINT_RENDER_TEXT = 2,
    VINT_RENDER_IMAGE = 3,
    VINT_RENDER_COLOR = 4,
    NUM_VINT_RENDER_INSTRUCTIONS = 5,
};

// size 0x10
struct vint_render_data_color
{
    float channels[4]; // 0x00
};

// size 0x18
struct vint_render_data_image
{
    vint_render_data_image(); // 0x00466C00

    int handle; // 0x00
    vector2 pos; // 0x04
    vector2 scale; // 0x0C
    bool tinted; // 0x14
};

// size 0x1C
struct vint_render_data_text
{
    vint_render_data_text(); // 0x00466BF0

    int handle; // 0x00
    vector2 pos; // 0x04
    vector2 scale; // 0x0C
    unsigned int num_chars; // 0x14
    bool reversed; // 0x18
};

// size 0x18
struct vint_text_format
{
    vint_text_format(); // 0x004669E0

    vector2 scale; // 0x00
    vint_render_data_color color; // 0x08
};

// size 0x10
struct vint_text_line_info
{
    vector2 dims; // 0x00
    vector2 offset; // 0x08
};

// size 0x14
struct vint_string_section_info
{
    vector2* pos; // 0x00
    unsigned int line_num; // 0x04
    float height; // 0x08
    vector2 pos_offset; // 0x0C
};

// size 0x1B8
struct vint_text_parse_status
{
    vint_text_parse_status(); // 0x0046D130 (:111)
    vector2& scale(); // 0x0046D150 (:114)
    vint_render_data_color& color(); // 0x0046D170 (:119)
    void format_push(); // 0x00466980 (:124)
    void format_pop(); // 0x004669C0 (:133)

    float wrap_width; // 0x00
    vint_resource_font* font; // 0x04
    bool font_reverse; // 0x08
    bool use_hyphens; // 0x09
    vint_text_line_info* line_infos; // 0x0C
    unsigned int max_line_infos; // 0x10
    unsigned int num_lines; // 0x14
    vint_text_force_case force_case; // 0x18
    signed char leading; // 0x1C
    vint_string_section_info* section_infos; // 0x20
    unsigned int max_section_infos; // 0x24
    unsigned int num_section_infos; // 0x28
    bool start_new_line; // 0x2C
    vint_text_format format_stack[16]; // 0x30
    unsigned int format_depth; // 0x1B0
    vint_line_frame* line_frame; // 0x1B4
};

// size 0x30
class vint_line_frame
{
public:
    vint_line_frame(const vint_text_render_params& _params); // 0x0046D880 (:608)
    void process_eol(vint_text_parse_status& _params, const vint_text_line_info& _line_info, unsigned char*& _instr_list, unsigned int& _instr_list_len); // 0x0046D940 (:646)
    void process_bol(vint_text_parse_status& _params, unsigned char*& _instr_list, unsigned int& _instr_list_len); // 0x0046DA80 (:680)

private:
    int line_frame_w_bms; // 0x00
    int line_frame_m_bms; // 0x04
    int line_frame_e_bms; // 0x08
    vector2i line_frame_w_dims; // 0x0C
    vector2i line_frame_m_dims; // 0x14
    vector2i line_frame_e_dims; // 0x1C
    vint_render_data_image* w_header; // 0x24
    vint_render_data_image* m_header; // 0x28
    vint_render_data_image* e_header; // 0x2C
};

const wchar_t* vint_string_parse_section(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status); // 0x0046E340 (:1381)
const wchar_t* vint_string_parse_all_sections(const wchar_t* _source_string, unsigned char** _instr_buffer, unsigned int* _instr_buffer_len, vint_text_parse_status& _status); // 0x0046E400 (:1417)
unsigned int vint_build_instruction_list(unsigned char* _buffer, unsigned int _buff_size, vector2& _size, bool reverse_text, bool use_hyphens, const wchar_t* _source, const vector2& _text_area, const vint_text_render_params& _params); // 0x0046E550 (:1479)
void vint_render_instruction_list(const unsigned char* _instruction_list, unsigned int _instruction_len, const matrix23& _xform, const vector2& _pos, const vint_variant_color& _color, float _alpha, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y); // 0x0046EE50 (:1633)
