// Original: volition/interface_lib/code/vint_document.h
// Implementation: volition/interface_lib/code/vint_document.cpp
//
// Documents: a loaded .vint_doc (object tree, tweens, animations, critical resources, metadata, the Lua script and
// state), its load buffers and string table, processing (time index, tweens), rendering with the clip stack, and the
// document list. The binary file header and the resolution callback are placed here by inference.
#pragma once

#include "volition/interface_lib/code/vint_external.h"

struct cfile;
class mempool_base;
class vector2;
class vint_element_base;
class vint_load_buffer;
class vint_object_base;
class vint_variant;
struct xml_element;

enum vint_critical_resource_type
{
    VINT_CRIT_RSC_INVALID = -1,
    VINT_CRIT_RSC_PEG = 0,
};

// size 0x8
struct vint_critical_resources
{
    vint_critical_resource_type type; // 0x00
    char* name; // 0x04
};

// size 0x20
struct vint_bin_file_header
{
    unsigned int ID; // 0x00
    unsigned int doc_name; // 0x04
    unsigned short version; // 0x08
    float doc_time_index; // 0x0C
    unsigned int num_metadata_entries; // 0x10
    unsigned int num_crit_resources; // 0x14
    unsigned int strings_offset; // 0x18
    unsigned short num_root_element_children; // 0x1C
    unsigned short num_root_object_children; // 0x1E
};

// size 0x18
class vint_bin_file_strings
{
public:
    vint_bin_file_strings(); // 0x0045BBE0 (.h:422)
    vint_bin_file_strings(mempool_base* _mem_pool, unsigned int _max_strings, unsigned int _max_bytes); // 0x004644E0 (:74)
    ~vint_bin_file_strings(); // 0x0045BBF0 (.h:423)
    unsigned int add_string(const char* _s); // 0x00464560 (:91)
    const char* get_string(unsigned int handle) const; // 0x00464630 (:122)
    const char* operator[](unsigned int _h) const; // 0x00468100 (.h:427)
    bool save_to_file(cfile* _cf) const; // 0x00464650 (:131)
    bool load_from_buffer(vint_load_buffer& _buffer, mempool_base* _temp_pool); // 0x0046BCD0 (:162)

private:
    unsigned int m_max_strings; // 0x00
    unsigned int m_num_strings; // 0x04
    unsigned int* m_string_table; // 0x08
    char* m_string_data; // 0x0C
    unsigned int m_next_string_offset; // 0x10
    unsigned int m_max_string_bytes; // 0x14
};

// size 0xC
class vint_load_buffer
{
public:
    vint_load_buffer(const vint_load_buffer& _in); // 0x0045BAD0 (.h:106)
    vint_load_buffer(unsigned char* _buffer, unsigned int _size); // 0x0045BAB0 (.h:100)
    vint_load_buffer& operator=(const vint_load_buffer& _in); // 0x0045BAF0 (.h:109)
    void set_pos(unsigned int _pos); // 0x0045BB10 (.h:117)
    unsigned int get_pos() const; // 0x0045BB30 (.h:123)
    const unsigned char* get_pos_ptr() const; // 0x0045BB40 (.h:129)
    const unsigned int get_size() const; // 0x0045BB50 (.h:134)
    // <unsigned char> 0x004675E0, <int> 0x00467710, <unsigned __int64> 0x00467660, <unsigned short> 0x00467760,
    // <float> 0x00467610, <unsigned int> 0x00467590 (.h:140). Original bug kept: the end test is inclusive, so a
    // value that ends exactly at the end of the buffer cannot be read.
    template<typename T>
    bool get_data(T& _out)
    {
        if (buffer + size <= pos + sizeof(T))
        {
            return false;
        }
        _out = *(const T*)pos;
        pos += sizeof(T);
        return true;
    }

private:
    const unsigned char* buffer; // 0x00
    unsigned int size; // 0x04
    const unsigned char* pos; // 0x08
};

// size 0x10
struct vint_document_data_buffer
{
    vint_document_data_buffer(); // 0x004646D0 (:240)

    unsigned int name_crc; // 0x00
    vint_load_buffer data; // 0x04
};

// size 0x40
struct vint_doc_metadata_pair
{
    char name[32]; // 0x00
    char value[32]; // 0x20
};

// size 0x4
class vint_resolution_callback
{
public:
    vint_resolution_callback();
    virtual ~vint_resolution_callback(); // vtbl+0x00
    virtual bool binary_save_overrides(cfile*, vint_bin_file_strings&, const vint_object_base*) = 0; // vtbl+0x04
    virtual bool xml_save_overrides(xml_element*, const vint_object_base*) = 0; // vtbl+0x08
    virtual void on_load_notify_resolution_override(const vector2i&, const vint_object_base*, const char*, const vint_variant&) = 0; // vtbl+0x0C
    virtual void on_load_notify_resolution_override(const char*, const vint_object_base*, const char*, const vint_variant&) = 0; // vtbl+0x10
    virtual void xml_cleanup_mem() = 0; // vtbl+0x14
    virtual void init_for_overrides(bool) = 0; // vtbl+0x18

    // 0x00 vfptr
};

// size 0x5E0
class vint_document
{
public:
    // size 0x20
    struct clip_stack_data
    {
        clip_stack_data(); // 0x00464820

        rect_i region; // 0x00
        vint_res_offset res_offset_x; // 0x10
        vint_res_offset res_offset_y; // 0x14
        vint_res_scale res_scale_x; // 0x18
        vint_res_scale res_scale_y; // 0x1C
    };

    vint_document(); // 0x0046BE20 (:400)
    ~vint_document(); // 0x0045BB60 (.h:245)
    const char* get_name() const; // 0x0045BB70 (.h:247)
    unsigned int get_handle() const; // 0x0045BB80 (.h:248)
    void set_name(const char* name); // 0x00464830 (:412)
    void add_ref(); // 0x00464860 (:441)
    void release(); // 0x00464870 (:447)
    void destroy(); // 0x004819D0 (:421)
    void process(float dtime); // 0x004649B0 (:595)
    void render(); // 0x0047C7B0 (:612)
    bool add_object(vint_object_base* _child); // 0x0046BE80 (:531)
    void remove_object(vint_object_base* _child); // 0x004648C0 (:538)
    bool add_element(vint_element_base* _child); // 0x0046BE40 (:513)
    void remove_element(vint_element_base* _child); // 0x004648A0 (:521)
    const char* find_metadata(const char* _key, int* _index); // 0x0046C090 (:1294)
    bool add_resolution(const vector2&);
    bool get_resolution(vector2&, int);
    bool set_resolution_current(int);
    float get_time_index() const; // 0x0045BB90 (.h:279)
    bool is_time_index_paused() const { return m_time_index_paused; } // inlined in rfg.exe (ui_document_hud.cpp)
    void set_time_index_paused(bool paused) { m_time_index_paused = paused; } // inlined in rfg.exe (ui_document_hud.cpp)
    void set_time_index(float _v); // 0x0045BBA0 (.h:280)
    bool is_paused() const; // 0x00883330 (.h:281)
    void pause(bool _p); // 0x00883340 (.h:282)
    int get_depth() const; // 0x0045BBC0 (.h:284)
    void set_depth(int);
    static vint_document* find(unsigned int _handle); // 0x00464C70 (:1029)
    static vint_document* find(const char* _name); // 0x00464C30 (:1001)
    static vint_document* create_empty(const char* _name); // 0x004819F0 (:635)
    static void unload_all(); // 0x00481A90 (:1374)
    static void reload_all(mempool_base* temp_pool); // 0x00482A30 (:1384)
    static void system_init(); // 0x00480D20 (:1216)
    static void system_shutdown(); // 0x00464CA0 (:1244)
    static void process_all(float dtime); // 0x00481B40 (:1420)
    static void render_all(); // 0x0047CA00 (:1456)
    static vint_document* load(const char* _name, unsigned char* _doc_buffer, unsigned int doc_size, unsigned char* _lua_script_buffer, unsigned int script_size, mempool_base* _temp_pool); // 0x00482380 (:1108)
    static vint_document* load(const char* _name, mempool_base* _temp_pool); // 0x00482250 (:1057)
    bool load_from_buffer(const char* _filename, unsigned char* buffer_data, unsigned int buffer_size, mempool_base* _temp_pool); // 0x004820A0 (:817)
    bool load_from_file(const char* _filename, mempool_base* _temp_pool); // 0x00482180 (:861)
    bool load_from_xml(vint_load_buffer& _buffer, mempool_base* _temp_pool); // 0x00481F90 (:936)
    bool load_from_binary(vint_load_buffer& _buffer, mempool_base* _mem_pool); // 0x0047CA70 (:1505)
    bool reload(mempool_base* temp_pool); // 0x00482470 (:1396)
    bool reload_overrides_from_buffer(const char* _filename, mempool_base* _temp_pool); // 0x0047F4E0 (:2686)
    bool reload_overrides_from_file(const char* _filename, mempool_base* _temp_pool); // 0x0047F570 (:2717)
    bool reload_overrides_from_binary(vint_load_buffer& _buffer, mempool_base* _mem_pool); // 0x0047F220 (:2607)
    bool reload_overrides(mempool_base* temp_pool); // 0x0047F5C0 (:2755)
    static void reload_overrides_all(mempool_base* temp_pool); // 0x004818A0 (:2744)
    void clip_push(const rect_i& _region, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y); // 0x0046C110 (:1316)
    void clip_pop(); // 0x0046C250 (:1356)
    unsigned short get_num_critical_resources(); // 0x00464CB0 (:1489)
    const vint_critical_resources* get_critical_resource(int which); // 0x00464CC0 (:1495)
    bool add_critical_resource_peg(const char* peg_name); // 0x00464B30 (:765)
    void clear_critical_resources(); // 0x00464BC0 (:785)
    void set_resolution_callback(vint_resolution_callback*);
    vint_resolution_callback* get_resolution_callback();
    void kill_at_end_of_frame(); // 0x0045BBD0 (.h:333)
protected:
    void parse_xml_metadata(xml_element* _root); // 0x00464A30 (:715)
    void parse_xml_critical_resources(xml_element* _root); // 0x0046BEC0 (:683)
    void parse_xml_objects_v2(xml_element* _root, vint_object_base* _parent, const char* _tag); // 0x00480C60 (:666)
private:
    bool init(const char* _name); // 0x0046BF60 (:1172)
    void clean_up(); // 0x00480B00 (:468)
    void return_to_factory(); // 0x004648E0 (:545)
    void process_object_branch(vint_object_base* obj); // 0x00464950 (:553)
    bool parse_xml_doc_version_2(xml_element* _root); // 0x00480CD0 (:800)
public:
    vint_object_base* get_root_object() const;
    vint_element_base* get_root_element() const; // 0x00883350 (.h:396)
protected:
    void parse_xml_property_set(xml_element* _root); // 0x00480F90 (:1860)
    void parse_xml_elements(xml_element* _root); // 0x004816C0 (:2319)
    void parse_xml_objects(xml_element* _root); // 0x004817C0 (:2391)
    void parse_xml_properties(xml_element* _root, bool for_anims); // 0x00481CD0 (:1968)
    void parse_xml_resolution_property_set(xml_element* _root, const vector2i& resolution, bool for_anims, bool set_info); // 0x004811C0 (:2094)
    bool parse_xml_doc_version_1(xml_element* _root); // 0x00481EC0 (:2457)

public:
    vint_document* document_next; // 0x00
    vint_document* document_prev; // 0x04
private:
    bool m_loaded_as_binary; // 0x08
    char m_name[32]; // 0x09
    unsigned int m_name_crc; // 0x2C
    unsigned int m_handle; // 0x30
    vint_doc_metadata_pair m_metadata[16]; // 0x34
    int m_num_metadata_pairs; // 0x434
    vint_object_base* m_root_object; // 0x438
    vint_element_base* m_root_element; // 0x43C
    float m_time_index; // 0x440
    bool m_time_index_paused; // 0x444
    clip_stack_data m_clip_stack_data[8]; // 0x448
    unsigned int m_clip_stack_depth; // 0x548
    vint_resolution_callback* m_resolution_callback; // 0x54C
    unsigned short m_which_resolution; // 0x550
    vint_critical_resources m_critical_resources[16]; // 0x554
    unsigned short m_num_critical_resources; // 0x5D4
    bool m_marked_for_termination; // 0x5D6
    int m_document_depth; // 0x5D8
    unsigned int m_ref_count; // 0x5DC
    // friends are not recorded in rfg.pdb: vint_render resets the counter
    friend void vint_render();
    static unsigned int Objects_processed; // 0x01A486C4
    static unsigned int Elements_rendered; // 0x01A486C8
};

extern unsigned int Vint_num_buffered_documents; // 0x01A486CC

void unused_dcf_vint_doc_load(); // 0x00482950 (:299)
void unused_dcf_vint_doc_unload(); // 0x00481F10 (:317)
void unused_dcf_vint_doc_unload_all(); // 0x00481F80 (:332)
void unused_dcf_vint_doc_reload_all(); // 0x00482A60 (:339)
void unused_dcf_vint_doc_reload(); // 0x004829D0 (:346)
void vint_document_render(); // 0x0047C8C0 (:1250)
