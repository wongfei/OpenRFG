// Original: volition/rfg/code/scripting/action_script/script_types.h
// Implementation: volition/rfg/code/scripting/action_script/script_types.cpp
//
// Nodes of the action script runtime: a script is a tree of script_node (functions, variables, constants), each with a
// class and a value type; script_variant holds a value (number, string, object handle, point). Placement of the enums,
// game_handle, script_point and script_variant inferred.
#pragma once

class action_script;
struct cfile;
struct xml_element;

enum script_node_classes
{
    SNC_INVALID = 0,
    SNC_ATOM = 1,
    SNC_EVENT = 2,
    SNC_FUNCTION = 3,
    SNC_VARIABLE = 4,
    NUM_SN_CLASSES = 5,
};

enum script_node_type
{
    SN_VOID = 0,
    SN_NUMBER = 1,
    SN_STRING = 2,
    SN_BOOL = 3,
    SN_OBJECT = 4,
    SN_OBJECT_LIST = 5,
    SN_POINT = 6,
    SN_VARIABLE = 7,
    SN_VARIABLE_REF = 8,
    SN_SCRIPT = 9,
    SN_CONDITION = 10,
    SN_ACTION = 11,
    SN_EVENT = 12,
    SN_FUNCTION = 13,
    SN_DELAY = 14,
    SN_MIN = 15,
    SN_MAX = 16,
    SN_REF = 17,
    SN_GROUP = 18,
    SN_REPEATABLE = 19,
    SN_DISABLED = 20,
    SN_MANAGED = 21,
    SN_TIMER = 22,
    SN_AIR_BOMB_HANDLE = 23,
    SN_GAME_HANDLE = 24,
    NUM_SN_TYPES = 25,
};

enum game_handle
{
    GAME_HANDLE_UNDEFINED = -1,
    GAME_HANDLE_FORCE_UINT32 = 0x7FFFFFFF,
};

// size 0x8
struct script_point
{
    unsigned int ref_handle; // 0x00
    int index; // 0x04
};

// size 0x8
union script_variant
{
    float number; // 0x00
    char* string; // 0x00
    unsigned int obj; // 0x00
    unsigned int* obj_ptr; // 0x00
    game_handle handle; // 0x00
    script_point point; // 0x00
};

// size 0xC
struct script_node
{
    virtual ~script_node(); // vtbl+0x00 0x00B1F430 (.h:239)
    virtual void release() = 0; // vtbl+0x04
    virtual void reset() = 0; // vtbl+0x08
    virtual bool save(cfile* file); // vtbl+0x0C 0x00B231E0 (:280)
    virtual void load(cfile* file); // vtbl+0x10 0x00B23220 (:294)
    virtual void init(); // vtbl+0x14 0x00B23250 (:300)
    virtual operator const char*(); // vtbl+0x18
    virtual operator bool(); // vtbl+0x1C 0x00B1F460 (.h:272)
    virtual operator float(); // vtbl+0x20 0x00B1F480 (.h:273)
    virtual operator int(); // vtbl+0x24 0x00B1F4A0 (.h:274)
    virtual operator unsigned int(); // vtbl+0x28 0x00B1F4C0 (.h:275)
    virtual operator unsigned int*(); // vtbl+0x2C 0x00B1F4E0 (.h:276)
    virtual operator game_handle(); // vtbl+0x30 0x00B1F500 (.h:277)
    virtual operator script_point(); // vtbl+0x34
    script_node(); // 0x00B230D0

    // 0x00 vfptr
    script_node_classes class_type; // 0x04
    script_node_type type; // 0x08
};

// size 0x10
struct script_variable : public script_node
{
    virtual void release(); // vtbl+0x04 0x00B39480 (:380)
    virtual void reset(); // vtbl+0x08 0x00B23290 (:387)
    virtual bool save(cfile* file); // vtbl+0x0C 0x00B232A0 (:396)
    virtual void load(cfile* file); // vtbl+0x10 0x00B232F0 (:415)
    virtual operator unsigned int(); // vtbl+0x28 0x00B2D390 (:127)
    script_variable(); // 0x00B2D3B0 (:136)
    script_variable(int _idx); // 0x00B230E0 (:132)
    virtual void init(); // vtbl+0x14 0x00B23320 (:426)
    virtual ~script_variable(); // vtbl+0x00 0x00B23100

    int idx; // 0x0C
};

// size 0x14
struct script_atom : public script_node
{
    virtual void release(); // vtbl+0x04 0x00B39470 (:315)
    virtual void reset(); // vtbl+0x08 0x00B23260 (:322)
    virtual bool save(cfile* file); // vtbl+0x0C 0x00B2D410 (:331)
    virtual void load(cfile* file); // vtbl+0x10 0x00B32E40 (:350)
    virtual void init(); // vtbl+0x14 0x00B23270 (:361)
    virtual operator const char*(); // vtbl+0x18
    virtual operator bool(); // vtbl+0x1C 0x00B2D1D0 (:76)
    virtual operator float(); // vtbl+0x20 0x00B2D1F0 (:77)
    virtual operator int(); // vtbl+0x24 0x00B2D200 (:78)
    virtual operator unsigned int(); // vtbl+0x28 0x00B2D210 (:79)
    virtual operator unsigned int*(); // vtbl+0x2C 0x00B2D220 (:80)
    virtual operator game_handle(); // vtbl+0x30 0x00B2D230 (:81)
    virtual operator script_point(); // vtbl+0x34
    script_atom(); // 0x00B2D350 (:98)
    script_atom(script_point& _point); // 0x00B2D320 (:94)
    script_atom(game_handle _handle); // 0x00B2D300 (:93)
    script_atom(unsigned int* _obj_ptr); // 0x00B2D2E0 (:92)
    script_atom(unsigned int _obj); // 0x00B2D2C0 (:91)
    script_atom(int);
    script_atom(float _n); // 0x00B2D290 (:89)
    script_atom(char* _string); // 0x00B2D270 (:88)
    script_atom(bool _flag); // 0x00B2D180 (:87)
    virtual ~script_atom(); // vtbl+0x00 0x00B2D260

    script_variant value; // 0x0C
};

// size 0x8
struct script_variable_value
{
    void clear(); // 0x00B23110 (:156)
    operator unsigned int(); // 0x00B23120 (:160)
    void set(unsigned int obj); // 0x00B23130 (:164)
    script_variable_value(); // 0x00B23140 (:168)
    bool save(cfile* file); // 0x00B23340 (:447)
    void load(cfile* file); // 0x00B23380 (:466)

    bool valid; // 0x00
    unsigned int handle; // 0x04
};

// size 0x18
struct script_arg_list
{
    script_arg_list(int, ...); // 0x00B1F580 (.h:302)
    script_arg_list();

    script_node_type type[5]; // 0x00
    int count; // 0x14
};

script_atom* verify_new_script_atom(script_atom* atom); // 0x00B23170 (:233)
script_variable* verify_new_script_variable(script_variable* variable); // 0x00B23180 (:243)
void script_mempools_set_thread_ownership(); // 0x00B231D0 (:266)
bool script_variant_save(cfile* file, script_variant& value, script_node_type type); // 0x00B233B0 (:484)
void script_variant_load(cfile* file, script_variant& value, script_node_type type); // 0x00B2D460 (:564)
char* script_alloc_string(const char* text); // 0x00B235A0 (:630)
script_node* script_node_parse(xml_element* elt, action_script* script); // 0x00B39490 (:653)
bool script_arg_list_parse(script_node** dest, script_arg_list* args, xml_element* elt, action_script* script); // 0x00B39D40 (:1001)
script_node** script_param_alloc(int num_params); // 0x00B235F0 (:1042)
bool script_params_save(cfile* file, script_node**&params); // 0x00B23620 (:1053)
void script_node_load(script_node*& node, cfile* file); // 0x00B32E70 (:1094)
void script_params_load(script_node**&params, cfile* file); // 0x00B32FB0 (:1130)
void scripting_clear_action_return_value(); // 0x00B236D0 (:1149)
void scripting_set_action_return_value(unsigned int return_value); // 0x00B236F0 (:1157)
void scripting_get_action_return_value(unsigned int& return_value); // 0x00B23710 (:1165)
bool scripting_action_return_value_valid(); // 0x00B23720 (:1173)
void script_mempool_clear(); // 0x00B23730 (:1181)
void script_atoms_clear_pool(); // 0x00B3D6A0 (:1189)
void script_variables_clear(); // 0x00B3D6F0 (:1197)
bool script_variable_is_set(int idx); // 0x00B23740 (:1211)
unsigned int script_variable_get(int idx); // 0x00B23750 (:1219)
void script_variable_set(int idx, unsigned int variable_value); // 0x00B23770 (:1232)
void script_variable_clear(script_node* pnode); // 0x00B23790 (:1240)
bool script_variables_save(cfile* file); // 0x00B237B0 (:1248)
void script_variables_load(cfile* file); // 0x00B23810 (:1268)
