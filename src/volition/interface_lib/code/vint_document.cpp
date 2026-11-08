// Original: volition/interface_lib/code/vint_document.cpp
//
// Documents (UI.md UI2): 16 slots, each with a root animation object ("root_animation") and a root group element
// ("safe_frame"). A document loads from a .vint_doc (binary, ID 0x3027 version 1: header, critical resource pegs,
// metadata, element and object trees by string-table index) or an XML file (versions 1 and 2); each loaded file is
// kept in one of 128 data buffers by name so reloading and the resolution overrides read it again. Metadata names
// the document's depth (render order) and its dependencies (loaded and reference counted with it). Processing
// advances the document clock and processes every object; rendering walks the root element with the clip stack.
// Documents marked for termination are destroyed after processing. Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/vint_document.h"

#include "volition/interface_lib/code/internal/vint_memory.h"
#include "volition/interface_lib/code/objects/animation/vint_object_animation.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_group.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/objects/vint_object_factory.h"
#include "volition/interface_lib/code/objects/vint_object_types.h"
#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"
#include "volition/interface_lib/code/resources/vint_resource_font.h"
#include "volition/interface_lib/code/scripting/vint_lua_actions.h"
#include "volition/interface_lib/code/vint_callback.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_input_internal.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "library/modules/runtime/base/include/keen/base/string.hpp"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The binary document's magic ID and its only version.
static const unsigned int VINT_BIN_DOC_ID = 0x3027;
static const unsigned short VINT_BIN_DOC_VERSION = 1;

// ---- class statics
unsigned int vint_document::Objects_processed; // 0x01A486C4
unsigned int vint_document::Elements_rendered; // 0x01A486C8

// ---- globals
unsigned int Vint_num_buffered_documents; // 0x01A486CC

// ---- file-static globals
static unsigned short Document_count = 1; // 0x014F5AB8
static vint_document* Avail_docs; // 0x01A486BC
static vint_document* Active_docs; // 0x01A486C0
static vint_document Documents[16]; // 0x01A48CD8
static vint_document_data_buffer Vint_buffered_documents[128]; // 0x01A55300

// ---- vint_bin_file_strings

// 0x0045BBE0 (.h:422)
vint_bin_file_strings::vint_bin_file_strings()
{
    m_max_strings = 0;
}

// 0x004644E0 (:74)
vint_bin_file_strings::vint_bin_file_strings(mempool_base* _mem_pool, unsigned int _max_strings, unsigned int _max_bytes)
{
    // Original: without a pool or sizes the members stay uninitialized
    if (_mem_pool == nullptr || _max_strings == 0 || _max_bytes == 0)
    {
        return;
    }
    m_max_strings = 0;
    m_max_string_bytes = 0;
    m_num_strings = 0;
    m_next_string_offset = 0;
    m_string_table = (unsigned int*)_mem_pool->alloc(_max_strings * 4, 0);
    m_string_data = (char*)_mem_pool->alloc(_max_bytes, 0);
    if (m_string_table != nullptr && m_string_data != nullptr)
    {
        m_max_strings = _max_strings;
        m_max_string_bytes = _max_bytes;
    }
}

// 0x0045BBF0 (.h:423)
vint_bin_file_strings::~vint_bin_file_strings()
{
}

// 0x00464560 (:91)
unsigned int vint_bin_file_strings::add_string(const char* _s)
{
    // an existing string's handle, else a new one (0 when full: the same as the first string's)
    if (_s == nullptr)
    {
        return 0;
    }
    for (unsigned int i = 0; i < m_num_strings; ++i)
    {
        if (strcmp(m_string_data + m_string_table[i], _s) == 0)
        {
            return i;
        }
    }
    if (m_num_strings < m_max_strings)
    {
        unsigned int end = m_next_string_offset + (unsigned int)strlen(_s) + 1;
        if (end < m_max_string_bytes)
        {
            m_string_table[m_num_strings] = m_next_string_offset;
            strcpy(m_string_data + m_next_string_offset, _s);
            ++m_num_strings;
            m_next_string_offset = end;
            return m_num_strings - 1;
        }
    }
    return 0;
}

// 0x00464630 (:122)
const char* vint_bin_file_strings::get_string(unsigned int handle) const
{
    if (handle >= m_num_strings)
    {
        return nullptr;
    }
    return m_string_data + m_string_table[handle];
}

// 0x00468100 (.h:427)
const char* vint_bin_file_strings::operator[](unsigned int _h) const
{
    if (_h >= m_num_strings)
    {
        return nullptr;
    }
    return m_string_data + m_string_table[_h];
}

// 0x00464650 (:131)
bool vint_bin_file_strings::save_to_file(cfile* _cf) const
{
    if (_cf == nullptr)
    {
        return false;
    }
    cf_write_uint32(m_num_strings, _cf);
    cf_write_uint32(m_next_string_offset, _cf);
    if (m_num_strings != 0)
    {
        for (unsigned int i = 0; i < m_num_strings; ++i)
        {
            cf_write_uint32(m_string_table[i], _cf);
        }
        if (cf_write(m_string_data, m_next_string_offset, _cf) == 0)
        {
            keen::debug::nativePrintArgs("Failed to write strings");
            return false;
        }
    }
    return true;
}

// 0x0046BCD0 (:162)
bool vint_bin_file_strings::load_from_buffer(vint_load_buffer& _buffer, mempool_base* _temp_pool)
{
    // the count, the data size, the offsets (into the pool), then the string data (left in the buffer)
    if (_temp_pool == nullptr)
    {
        return false;
    }
    unsigned int data_bytes = 0;
    // Original: a count that cannot be read keeps the old count
    _buffer.get_data(m_num_strings);
    _buffer.get_data(data_bytes);
    if (m_num_strings == 0)
    {
        m_string_data = nullptr;
        m_string_table = nullptr;
        return false;
    }
    unsigned int table_bytes = m_num_strings * 4;
    if (data_bytes + table_bytes > _buffer.get_size() - _buffer.get_pos())
    {
        return false;
    }
    m_string_table = (unsigned int*)_temp_pool->alloc(table_bytes, 0);
    if (m_string_table == nullptr)
    {
        m_num_strings = 0;
        m_string_data = nullptr;
        return false;
    }
    for (unsigned int i = 0; i < m_num_strings; ++i)
    {
        _buffer.get_data(m_string_table[i]);
    }
    m_string_data = (char*)_buffer.get_pos_ptr();
    return true;
}

// ---- vint_load_buffer (header inlines)

// 0x0045BAB0 (.h:100)
vint_load_buffer::vint_load_buffer(unsigned char* _buffer, unsigned int _size)
{
    buffer = _buffer;
    size = _size;
    pos = _buffer;
}

// 0x0045BAD0 (.h:106)
vint_load_buffer::vint_load_buffer(const vint_load_buffer& _in)
{
    buffer = _in.buffer;
    size = _in.size;
    pos = _in.pos;
}

// 0x0045BAF0 (.h:109)
vint_load_buffer& vint_load_buffer::operator=(const vint_load_buffer& _in)
{
    // unlike the copy, an assignment rewinds
    buffer = _in.buffer;
    size = _in.size;
    pos = _in.buffer;
    return *this;
}

// 0x0045BB10 (.h:117)
void vint_load_buffer::set_pos(unsigned int _pos)
{
    if (_pos < size)
    {
        pos = buffer + _pos;
    }
}

// 0x0045BB30 (.h:123)
unsigned int vint_load_buffer::get_pos() const
{
    return (unsigned int)(pos - buffer);
}

// 0x0045BB40 (.h:129)
const unsigned char* vint_load_buffer::get_pos_ptr() const
{
    return pos;
}

// 0x0045BB50 (.h:134)
const unsigned int vint_load_buffer::get_size() const
{
    return size;
}

// ---- data buffers

// 0x004646D0 (:240)
vint_document_data_buffer::vint_document_data_buffer()
    : data((unsigned char*)1, 1)
{
    // Original: a placeholder buffer at address 1 (name_crc uninitialized)
}

// 0x004646F0 (:248)
// LTCG: name_crc in ESI.
static vint_load_buffer* vint_document_find_data_buffer(unsigned int name_crc)
{
    // the file's buffer, rewound
    for (unsigned int i = 0; i < Vint_num_buffered_documents; ++i)
    {
        if (Vint_buffered_documents[i].name_crc == name_crc)
        {
            vint_load_buffer* data = &Vint_buffered_documents[i].data;
            if (data->get_size() != 0)
            {
                data->set_pos(0);
            }
            return data;
        }
    }
    return nullptr;
}

// 0x00464730 (:262)
static vint_load_buffer* vint_document_add_new_data_buffer(unsigned int name_crc, unsigned char* buffer_data, unsigned int buffer_size)
{
    // a file already buffered is not copied again
    if (vint_load_buffer* data = vint_document_find_data_buffer(name_crc))
    {
        return data;
    }
    unsigned int index = Vint_num_buffered_documents;
    if (index < 128)
    {
        Vint_perm_mempool->set_thread_ownership();
        unsigned char* copy = (unsigned char*)Vint_perm_mempool->alloc(buffer_size, 0);
        memcpy(copy, buffer_data, buffer_size);
        ++Vint_num_buffered_documents;
        Vint_buffered_documents[index].data = vint_load_buffer(copy, buffer_size);
        Vint_buffered_documents[index].name_crc = name_crc;
        return &Vint_buffered_documents[index].data;
    }
    keen::debug::nativePrintArgs("[vint] No more free slots in Vint_buffered_documents!\n");
    return nullptr;
}

// ---- console commands (not registered in rfg.exe)

// 0x00482950 (:299)
void unused_dcf_vint_doc_load()
{
    if (!Console_command)
    {
        return;
    }
    console_get_arg(2, 0);
    if (vint_project_get_doc_filename(Console_arg) == nullptr)
    {
        console_printf("Cannot find vint document '%s' in project.", Console_arg);
    }
    if (vint_document::load(Console_arg, Vint_temp_mempool) == nullptr)
    {
        console_printf("Failed to load interface");
    }
}

// 0x00481F10 (:317)
void unused_dcf_vint_doc_unload()
{
    if (!Console_command)
    {
        return;
    }
    console_get_arg(2, 0);
    vint_document* doc = vint_document::find(Console_arg);
    if (doc == nullptr)
    {
        console_printf("Doc name not found");
        return;
    }
    doc->destroy();
}

// 0x00481F80 (:332)
void unused_dcf_vint_doc_unload_all()
{
    if (Console_command)
    {
        vint_document::unload_all();
    }
}

// 0x00482A60 (:339)
void unused_dcf_vint_doc_reload_all()
{
    if (Console_command)
    {
        vint_document::reload_all(Vint_temp_mempool);
    }
}

// 0x004829D0 (:346)
void unused_dcf_vint_doc_reload()
{
    if (!Console_command)
    {
        return;
    }
    console_get_arg(2, 0);
    vint_document* doc = vint_document::find(Console_arg);
    if (doc == nullptr)
    {
        console_printf("Doc name not found");
        return;
    }
    doc->reload(Vint_temp_mempool);
}

// 0x004647F0 (:368)
// The deepest document first.
static int compare_docs_for_render(const void* _left, const void* _right)
{
    if (_left == nullptr || _right == nullptr)
    {
        return 0;
    }
    return (*(vint_document* const*)_right)->get_depth() - (*(vint_document* const*)_left)->get_depth();
}

// 0x0046BDD0 (:375)
// LTCG: _type in EAX, _doc in EDI.
static vint_object_base* create_parentless_object(const char* _name, unsigned int _type, vint_document* _doc)
{
    if (_doc == nullptr || _name == nullptr)
    {
        return nullptr;
    }
    const vint_object_type_info* info = vint_object_types_get_type_info(_type);
    if (info == nullptr || info->factory == nullptr)
    {
        return nullptr;
    }
    vint_object_base* obj = info->factory->allocate();
    if (obj != nullptr)
    {
        obj->init(_name, _doc);
    }
    return obj;
}

// A document from the available list into the active list (rfg.exe inlines the list moves).
static void vint_document_list_add(vint_document*& _head, vint_document* _doc)
{
    if (_head == nullptr)
    {
        _doc->document_next = _doc;
        _doc->document_prev = _doc;
        _head = _doc;
        return;
    }
    _doc->document_prev = _head->document_prev;
    _doc->document_next = _head;
    _head->document_prev->document_next = _doc;
    _head->document_prev = _doc;
}

static void vint_document_list_remove(vint_document*& _head, vint_document* _doc)
{
    if (_head == _doc && (_head = _doc->document_next) == _doc)
    {
        _head = nullptr;
    }
    else
    {
        _doc->document_prev->document_next = _doc->document_next;
        _doc->document_next->document_prev = _doc->document_prev;
    }
    _doc->document_prev = nullptr;
    _doc->document_next = nullptr;
}

// ---- vint_document

// 0x00464820
vint_document::clip_stack_data::clip_stack_data()
{
}

// 0x0046BE20 (:400)
vint_document::vint_document()
{
    m_loaded_as_binary = true;
    m_resolution_callback = nullptr;
    clear_critical_resources();
}

// 0x0045BB60 (.h:245)
vint_document::~vint_document()
{
}

// 0x0045BB70 (.h:247)
const char* vint_document::get_name() const
{
    return m_name;
}

// 0x0045BB80 (.h:248)
unsigned int vint_document::get_handle() const
{
    return m_handle;
}

// 0x0045BB90 (.h:279)
float vint_document::get_time_index() const
{
    return m_time_index;
}

// 0x0045BBA0 (.h:280)
void vint_document::set_time_index(float _v)
{
    m_time_index = _v;
}

// 0x00883330 (.h:281)
bool vint_document::is_paused() const
{
    return m_time_index_paused;
}

// 0x00883340 (.h:282)
void vint_document::pause(bool _p)
{
    m_time_index_paused = _p;
}

// 0x0045BBC0 (.h:284)
int vint_document::get_depth() const
{
    return m_document_depth;
}

// 0x0045BBD0 (.h:333)
void vint_document::kill_at_end_of_frame()
{
    m_marked_for_termination = true;
}

// 0x00883350 (.h:396)
vint_element_base* vint_document::get_root_element() const
{
    return m_root_element;
}

// 0x00464830 (:412)
void vint_document::set_name(const char* name)
{
    if (name != nullptr)
    {
        strncpy_safe(m_name, name, sizeof(m_name));
        m_name_crc = crc_stri(m_name, 0);
    }
}

// 0x004819D0 (:421)
void vint_document::destroy()
{
    vint_lua_document_destroy(this);
    clean_up();
    return_to_factory();
}

// 0x00464860 (:441)
void vint_document::add_ref()
{
    ++m_ref_count;
    m_marked_for_termination = false;
}

// 0x00464870 (:447)
void vint_document::release()
{
    // the last reference destroys the document after processing
    if (m_ref_count < 2)
    {
        m_marked_for_termination = true;
        return;
    }
    m_marked_for_termination = false;
    --m_ref_count;
}

// 0x00480B00 (:468)
void vint_document::clean_up()
{
    if (m_root_object != nullptr)
    {
        m_root_object->destroy();
        m_root_object = nullptr;
    }
    if (m_root_element != nullptr)
    {
        m_root_element->destroy();
        m_root_element = nullptr;
    }
    vint_input_remove_doc_subscriptions(m_handle);
    vint_data_remove_doc_subscriptions(m_handle);
    // Original: a second free loop follows, dead since this zeroes the count
    clear_critical_resources();
    // the dependencies lose this document's reference
    int index = 0;
    const char* dependency = find_metadata("doc_dependency", &index);
    while (dependency != nullptr)
    {
        if (vint_document* doc = find(dependency))
        {
            doc->release();
        }
        // Original: the search goes on from the next pair (from 0 past the 16th)
        index = index + 1 < 16 ? index + 1 : 0;
        dependency = nullptr;
        for (; index < m_num_metadata_pairs; ++index)
        {
            if (v_stricmp("doc_dependency", m_metadata[index].name) == 0)
            {
                dependency = m_metadata[index].value;
                break;
            }
        }
    }
}

// 0x0046BE40 (:513)
bool vint_document::add_element(vint_element_base* _child)
{
    // the root element's first child
    if (_child == nullptr || m_root_element == nullptr)
    {
        return false;
    }
    if (_child->parent_object != nullptr)
    {
        _child->remove_from_parent();
    }
    _child->next_sibling = m_root_element->first_child;
    _child->parent_object = m_root_element;
    _child->m_document = m_root_element->m_document;
    m_root_element->first_child = _child;
    return true;
}

// 0x004648A0 (:521)
void vint_document::remove_element(vint_element_base* _child)
{
    if (m_root_element != nullptr && _child != nullptr && _child->m_document == this)
    {
        _child->remove_from_parent();
    }
}

// 0x0046BE80 (:531)
bool vint_document::add_object(vint_object_base* _child)
{
    if (_child == nullptr || m_root_object == nullptr)
    {
        return false;
    }
    if (_child->parent_object != nullptr)
    {
        _child->remove_from_parent();
    }
    _child->next_sibling = m_root_object->first_child;
    _child->parent_object = m_root_object;
    _child->m_document = m_root_object->m_document;
    m_root_object->first_child = _child;
    return true;
}

// 0x004648C0 (:538)
void vint_document::remove_object(vint_object_base* _child)
{
    if (_child != nullptr && _child->m_document == this)
    {
        _child->remove_from_parent();
    }
}

// 0x004648E0 (:545)
void vint_document::return_to_factory()
{
    vint_document_list_remove(Active_docs, this);
    vint_document_list_add(Avail_docs, this);
    m_handle = 0;
}

// 0x00464950 (:553)
void vint_document::process_object_branch(vint_object_base* obj)
{
    // depth first without recursion; siblings past the 256th stack entry are skipped
    unsigned int depth = 0;
    while (obj != nullptr)
    {
        obj->process();
        if (obj->first_child != nullptr)
        {
            if (obj->next_sibling != nullptr && depth < 256)
            {
                Process_stack[depth++] = obj->next_sibling;
            }
            obj = obj->first_child;
            continue;
        }
        obj = obj->next_sibling;
        if (obj == nullptr && depth != 0)
        {
            obj = Process_stack[--depth];
        }
    }
}

// 0x004649B0 (:595)
void vint_document::process(float dtime)
{
    if (!m_time_index_paused)
    {
        m_time_index = dtime > 0.0f ? (float)((double)m_time_index + (double)dtime) : (float)((double)m_time_index + (double)Frametime_real);
    }
    process_object_branch(m_root_object);
    process_object_branch(m_root_element);
}

// The root render parameters (rfg.exe inlines them in both renders).
static void vint_document_root_render_params(vint_render_params& _params)
{
    _params.alpha = 1.0f;
    _params.is_mask = false;
    _params.mode = VINT_RENDER_FILTER;
    _params.tint.r = 1.0f;
    _params.tint.g = 1.0f;
    _params.tint.b = 1.0f;
    _params.res_offset_x = VINT_RES_OFFSET_INHERIT;
    _params.res_offset_y = VINT_RES_OFFSET_INHERIT;
    _params.res_scale_x = VINT_RES_SCALE_INHERIT;
    _params.res_scale_y = VINT_RES_SCALE_INHERIT;
}

// 0x0047C7B0 (:612)
void vint_document::render()
{
    vint_render_params params;
    vint_document_root_render_params(params);
    if (Render_callbacks != nullptr && Render_callbacks->reset_to_default != nullptr)
    {
        Render_callbacks->reset_to_default();
    }
    if (m_root_element != nullptr)
    {
        m_root_element->render(params, Process_stack, 256);
    }
}

// 0x004819F0 (:635)
vint_document* vint_document::create_empty(const char* _name)
{
    if (_name == nullptr || Avail_docs == nullptr)
    {
        return nullptr;
    }
    vint_document* doc = Avail_docs;
    vint_document_list_remove(Avail_docs, doc);
    if (!doc->init(_name))
    {
        doc->destroy();
        return nullptr;
    }
    vint_document_list_add(Active_docs, doc);
    return doc;
}

// 0x00480C60 (:666)
void vint_document::parse_xml_objects_v2(xml_element* _root, vint_object_base* _parent, const char* _tag)
{
    if (_root == nullptr || _parent == nullptr || _tag == nullptr)
    {
        return;
    }
    xml_element* list = xml_find(_root, _tag);
    if (list == nullptr)
    {
        return;
    }
    for (xml_element* e = xml_find(list, "object"); e != nullptr; e = xml_find_next(list, e, "object"))
    {
        vint_object_base::xml_parse_object_v2(e, _parent);
    }
}

// 0x0046BEC0 (:683)
void vint_document::parse_xml_critical_resources(xml_element* _root)
{
    if (_root == nullptr)
    {
        return;
    }
    xml_element* resources = xml_find(_root, "critical_resources");
    if (resources == nullptr)
    {
        return;
    }
    if (xml_element* pegs = xml_find(resources, "pegs"))
    {
        // Original: the next filename is searched under critical_resources, not pegs
        for (xml_element* e = xml_find(pegs, "filename"); e != nullptr && m_num_critical_resources < 16; e = xml_find_next(resources, e, "filename"))
        {
            if (const char* peg_name = xml_get_string(e, nullptr))
            {
                add_critical_resource_peg(peg_name);
            }
        }
    }
    if (Doc_load_resource_callback != nullptr)
    {
        Doc_load_resource_callback(this);
    }
}

// 0x00464A30 (:715)
void vint_document::parse_xml_metadata(xml_element* _root)
{
    if (_root == nullptr)
    {
        return;
    }
    xml_element* metadata = xml_find(_root, "metadata");
    if (metadata == nullptr)
    {
        return;
    }
    for (xml_element* item = xml_find(metadata, "metadata_item"); item != nullptr && m_num_metadata_pairs < 16; item = xml_find_next(metadata, item, "metadata_item"))
    {
        xml_element* name = xml_find(item, "name");
        const char* name_string = name != nullptr ? xml_get_string(name, nullptr) : nullptr;
        if (name_string == nullptr)
        {
            continue;
        }
        xml_element* value = xml_find(item, "value");
        const char* value_string = value != nullptr ? xml_get_string(value, nullptr) : nullptr;
        if (value_string == nullptr)
        {
            continue;
        }
        strncpy_safe(m_metadata[m_num_metadata_pairs].name, name_string, 32);
        strncpy_safe(m_metadata[m_num_metadata_pairs].value, value_string, 32);
        ++m_num_metadata_pairs;
    }
}

// 0x00464B30 (:765)
bool vint_document::add_critical_resource_peg(const char* peg_name)
{
    unsigned short index = m_num_critical_resources;
    if (peg_name == nullptr || index > 15)
    {
        return false;
    }
    char* name = Memory_callbacks != nullptr && Memory_callbacks->realloc != nullptr ? (char*)Memory_callbacks->realloc(nullptr, (unsigned int)strlen(peg_name) + 1) : nullptr;
    m_critical_resources[index].name = name;
    if (name == nullptr)
    {
        return false;
    }
    strcpy(name, peg_name);
    m_critical_resources[index].type = VINT_CRIT_RSC_PEG;
    ++m_num_critical_resources;
    return true;
}

// 0x00464BC0 (:785)
void vint_document::clear_critical_resources()
{
    for (unsigned short i = 0; i < m_num_critical_resources; ++i)
    {
        if (m_critical_resources[i].name != nullptr)
        {
            if (Memory_callbacks != nullptr && Memory_callbacks->free != nullptr)
            {
                Memory_callbacks->free(m_critical_resources[i].name);
            }
            m_critical_resources[i].name = nullptr;
        }
    }
    m_num_critical_resources = 0;
}

// 0x00480CD0 (:800)
bool vint_document::parse_xml_doc_version_2(xml_element* _root)
{
    parse_xml_metadata(_root);
    parse_xml_critical_resources(_root);
    parse_xml_objects_v2(_root, m_root_element, "elements");
    parse_xml_objects_v2(_root, m_root_object, "animations");
    return true;
}

// 0x004820A0 (:817)
bool vint_document::load_from_buffer(const char* _filename, unsigned char* buffer_data, unsigned int buffer_size, mempool_base* _temp_pool)
{
    if (buffer_size == 0)
    {
        return false;
    }
    unsigned int mark = _temp_pool->mark();
    // the file is kept (by name) for reloads
    vint_load_buffer* buffer = vint_document_add_new_data_buffer(crc_stri(_filename, 0), buffer_data, buffer_size);
    if (buffer == nullptr)
    {
        // Original: the temp pool is not restored
        return false;
    }
    unsigned int id = 0;
    buffer->get_data(id);
    if (buffer->get_size() != 0)
    {
        buffer->set_pos(0);
    }
    bool result;
    if (id == VINT_BIN_DOC_ID)
    {
        result = load_from_binary(*buffer, _temp_pool);
    }
    else
    {
        // Original: the caller's buffer is terminated, not the kept copy the XML is parsed from
        buffer_data[buffer_size - 1] = '\0';
        m_loaded_as_binary = false;
        result = load_from_xml(*buffer, _temp_pool);
    }
    _temp_pool->restore_to_mark(mark);
    return result;
}

// 0x00482180 (:861)
bool vint_document::load_from_file(const char* _filename, mempool_base* _temp_pool)
{
    if (_filename == nullptr)
    {
        return false;
    }
    if (_temp_pool == nullptr)
    {
        _temp_pool = Vint_temp_mempool;
    }
    unsigned int mark = _temp_pool->mark();
    // Original: the failure paths leave the file open and the pool unrestored
    cfile* file = cf_open(_filename, "rb", PLATFORM_PC, false);
    if (file == nullptr)
    {
        return false;
    }
    unsigned int size = cf_size(file);
    if (size == 0)
    {
        return false;
    }
    unsigned char* data = (unsigned char*)_temp_pool->alloc(size + 1, 0);
    if (data == nullptr)
    {
        return false;
    }
    size = cf_read(data, size, file, false);
    if (size == 0)
    {
        return false;
    }
    cf_close(file);
    bool result = load_from_buffer(_filename, data, size, _temp_pool);
    _temp_pool->restore_to_mark(mark);
    return result;
}

// 0x00481F90 (:936)
bool vint_document::load_from_xml(vint_load_buffer& _buffer, mempool_base* _temp_pool)
{
    if (_temp_pool == nullptr)
    {
        _temp_pool = Vint_temp_mempool;
    }
    _temp_pool->set_thread_ownership();
    unsigned int mark = _temp_pool->mark();
    xml_element* root = xml_parse_from_string((char*)_buffer.get_pos_ptr(), _temp_pool, nullptr);
    xml_element* doc_type = root != nullptr ? xml_find(root, "vint_doc_type") : nullptr;
    if (doc_type == nullptr)
    {
        // Original: the temp pool is not restored
        return false;
    }
    bool result;
    if (strcmp(xml_get_string(doc_type, nullptr), "vint_document") == 0)
    {
        int version = 1;
        if (xml_element* e = xml_find(root, "vint_doc_version"))
        {
            version = xml_get_int(e, 0);
        }
        if (version == 1)
        {
            result = parse_xml_doc_version_1(root);
        }
        else if (version == 2)
        {
            result = parse_xml_doc_version_2(root);
        }
        else
        {
            keen::debug::nativePrintArgs("Unsupported Vint document version");
            result = false;
        }
    }
    else
    {
        keen::debug::nativePrintArgs("Invalid Vint document file");
        result = false;
    }
    _temp_pool->restore_to_mark(mark);
    return result;
}

// 0x00464C30 (:1001)
vint_document* vint_document::find(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    unsigned int name_crc = crc_stri(_name, 0);
    vint_document* doc = Active_docs;
    while (doc != nullptr)
    {
        if (doc->m_name_crc == name_crc)
        {
            return doc;
        }
        doc = doc->document_next;
        if (doc == Active_docs)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00464C70 (:1029)
vint_document* vint_document::find(unsigned int _handle)
{
    // the slot index in the low half
    if (_handle == 0 || (unsigned short)_handle >= 16)
    {
        return nullptr;
    }
    vint_document* doc = &Documents[_handle & 0xFFFF];
    return doc->m_handle == _handle ? doc : nullptr;
}

// The document_depth metadata (rfg.exe inlines this in both loads).
static void vint_document_read_depth(vint_doc_metadata_pair* _metadata, int _num_pairs, int& _depth)
{
    for (int i = 0; i < _num_pairs; ++i)
    {
        if (v_stricmp("document_depth", _metadata[i].name) == 0)
        {
            sscanf(_metadata[i].value, "%i", &_depth);
            break;
        }
    }
}

// 0x00482250 (:1057)
vint_document* vint_document::load(const char* _name, mempool_base* _temp_pool)
{
    if (_name == nullptr || _temp_pool == nullptr)
    {
        return nullptr;
    }
    const char* filename = vint_project_get_doc_filename(_name);
    if (filename == nullptr)
    {
        return nullptr;
    }
    vint_document* doc = create_empty(_name);
    if (doc == nullptr)
    {
        return nullptr;
    }
    if (!doc->load_from_file(filename, _temp_pool))
    {
        doc->destroy();
        return nullptr;
    }
    vint_document_read_depth(doc->m_metadata, doc->m_num_metadata_pairs, doc->m_document_depth);
    // the dependencies load first or gain a reference
    int index = 0;
    for (const char* dependency = doc->find_metadata("doc_dependency", &index); dependency != nullptr; dependency = doc->find_metadata("doc_dependency", &index))
    {
        if (vint_document* dep = find(dependency))
        {
            dep->add_ref();
        }
        else
        {
            load(dependency, Vint_temp_mempool);
        }
        ++index;
    }
    vint_lua_document_init(doc, Vint_temp_mempool);
    return doc;
}

// 0x00482380 (:1108)
vint_document* vint_document::load(const char* _name, unsigned char* _doc_buffer, unsigned int doc_size, unsigned char* _lua_script_buffer, unsigned int script_size, mempool_base* _temp_pool)
{
    // from memory: the dependencies are not loaded
    if (_name == nullptr || _doc_buffer == nullptr || _lua_script_buffer == nullptr)
    {
        return nullptr;
    }
    const char* filename = vint_project_get_doc_filename(_name);
    if (filename == nullptr)
    {
        return nullptr;
    }
    vint_document* doc = create_empty(_name);
    if (doc == nullptr)
    {
        return nullptr;
    }
    if (!doc->load_from_buffer(filename, _doc_buffer, doc_size, _temp_pool))
    {
        doc->destroy();
        return nullptr;
    }
    vint_document_read_depth(doc->m_metadata, doc->m_num_metadata_pairs, doc->m_document_depth);
    vint_lua_document_init(doc, _lua_script_buffer, script_size, Vint_temp_mempool);
    return doc;
}

// 0x0046BF60 (:1172)
bool vint_document::init(const char* _name)
{
    if (_name == nullptr)
    {
        return false;
    }
    strncpy_safe(m_name, _name, sizeof(m_name));
    m_name_crc = crc_stri(m_name, 0);
    m_num_metadata_pairs = 0;
    m_root_object = create_parentless_object("root_animation", vint_object_animation::Object_type, this);
    m_root_element = (vint_element_base*)create_parentless_object("safe_frame", vint_element_group::Object_type, this);
    m_time_index_paused = false;
    m_clip_stack_depth = 0;
    m_marked_for_termination = false;
    m_document_depth = 0;
    // the slot index, and a load count against stale handles
    m_handle = (unsigned int)(this - Documents) + ((unsigned int)Document_count << 16);
    m_time_index = 0.0f;
    m_ref_count = 1;
    ++Document_count;
    return true;
}

// 0x00480D20 (:1216)
void vint_document::system_init()
{
    for (int i = 0; i < 16; ++i)
    {
        Documents[i].clean_up();
    }
    Avail_docs = nullptr;
    Active_docs = nullptr;
    for (int i = 0; i < 16; ++i)
    {
        vint_document_list_add(Avail_docs, &Documents[i]);
    }
    Document_count = 1;
}

// 0x00464CA0 (:1244)
void vint_document::system_shutdown()
{
}

// 0x0047C8C0 (:1250)
void vint_document_render()
{
    // every document in list order (vint_render sorts by depth instead)
    vint_document* doc = Active_docs;
    while (doc != nullptr)
    {
        doc->render();
        doc = doc->document_next;
        if (doc == Active_docs)
        {
            break;
        }
    }
}

// 0x0046C090 (:1294)
const char* vint_document::find_metadata(const char* _key, int* _index)
{
    // from *_index on (0 if out of range); the found index is returned through _index
    int start = 0;
    if (_index != nullptr && *_index < 16 && *_index >= 0)
    {
        start = *_index;
    }
    for (int i = start; i < m_num_metadata_pairs; ++i)
    {
        if (_key == nullptr || v_stricmp(_key, m_metadata[i].name) == 0)
        {
            if (_index != nullptr)
            {
                *_index = i;
            }
            return m_metadata[i].value;
        }
    }
    return nullptr;
}

// 0x0046C110 (:1316)
void vint_document::clip_push(const rect_i& _region, vint_res_offset _res_offset_x, vint_res_offset _res_offset_y, vint_res_scale _res_scale_x, vint_res_scale _res_scale_y)
{
    // the region intersected with the enclosing one (empty without overlap); past 8 only the depth counts
    unsigned int depth = m_clip_stack_depth;
    if (depth < 8)
    {
        clip_stack_data& data = m_clip_stack_data[depth];
        if (depth == 0)
        {
            data.region = _region;
        }
        else
        {
            const rect_i& outer = m_clip_stack_data[depth - 1].region;
            vector2i nw(0, 0);
            vector2i se(0, 0);
            if (_region.nw.x <= outer.se.x && outer.nw.x <= _region.se.x && _region.nw.y <= outer.se.y && outer.nw.y <= _region.se.y)
            {
                nw = _region.nw;
                se = _region.se;
                if (nw.x <= outer.nw.x)
                {
                    nw.x = outer.nw.x;
                }
                if (nw.y <= outer.nw.y)
                {
                    nw.y = outer.nw.y;
                }
                if (outer.se.x <= se.x)
                {
                    se.x = outer.se.x;
                }
                if (outer.se.y <= se.y)
                {
                    se.y = outer.se.y;
                }
            }
            data.region.nw = nw;
            data.region.se = se;
        }
        data.res_offset_x = _res_offset_x;
        data.res_offset_y = _res_offset_y;
        data.res_scale_x = _res_scale_x;
        data.res_scale_y = _res_scale_y;
        if (Render_callbacks != nullptr && Render_callbacks->set_clip_region != nullptr)
        {
            Render_callbacks->set_clip_region(data.region, _res_offset_x, _res_offset_y, _res_scale_x, _res_scale_y);
        }
    }
    ++m_clip_stack_depth;
}

// 0x0046C250 (:1356)
void vint_document::clip_pop()
{
    // back to the enclosing region, or none
    if (m_clip_stack_depth == 0)
    {
        return;
    }
    unsigned int depth = --m_clip_stack_depth;
    if (depth == 0)
    {
        if (Render_callbacks != nullptr && Render_callbacks->reset_clip_region != nullptr)
        {
            Render_callbacks->reset_clip_region();
        }
    }
    else if (depth < 8)
    {
        // Original bug kept: the region popped is set again, not the enclosing one
        const clip_stack_data& data = m_clip_stack_data[depth];
        if (Render_callbacks != nullptr && Render_callbacks->set_clip_region != nullptr)
        {
            Render_callbacks->set_clip_region(data.region, data.res_offset_x, data.res_offset_y, data.res_scale_x, data.res_scale_y);
        }
    }
}

// 0x00481A90 (:1374)
void vint_document::unload_all()
{
    vint_document* doc = Active_docs;
    while (doc != nullptr)
    {
        vint_document* next = doc->document_next != Active_docs ? doc->document_next : nullptr;
        vint_lua_document_destroy(doc);
        doc->clean_up();
        doc->return_to_factory();
        doc = next;
    }
}

// 0x00482A30 (:1384)
void vint_document::reload_all(mempool_base* temp_pool)
{
    if (temp_pool == nullptr)
    {
        return;
    }
    vint_document* doc = Active_docs;
    while (doc != nullptr)
    {
        doc->reload(temp_pool);
        doc = doc->document_next;
        if (doc == Active_docs)
        {
            break;
        }
    }
}

// 0x00482470 (:1396)
bool vint_document::reload(mempool_base* temp_pool)
{
    // in place, keeping the handle; the Lua state starts again (without a destroy)
    if (temp_pool == nullptr)
    {
        return false;
    }
    clean_up();
    unsigned int handle = m_handle;
    init(m_name);
    m_handle = handle;
    if (!load_from_file(vint_project_get_doc_filename(m_name), temp_pool))
    {
        return false;
    }
    vint_lua_document_init(this, Vint_temp_mempool);
    return true;
}

// 0x00481B40 (:1420)
void vint_document::process_all(float dtime)
{
    Objects_processed = 0;
    if (dtime < 0.0f)
    {
        dtime = Frametime_real;
    }
    // at most half a second a frame; at most a millisecond is no step at all
    bool step = true;
    if ((float)((double)dtime - 0.5) < 0.0f)
    {
        step = (double)dtime > 0.001;
    }
    else
    {
        dtime = 0.5f;
    }
    if (step)
    {
        vint_document* doc = Active_docs;
        while (doc != nullptr)
        {
            doc->process(dtime);
            doc = doc->document_next;
            if (doc == Active_docs)
            {
                break;
            }
        }
    }
    vint_document* doc = Active_docs;
    while (doc != nullptr)
    {
        vint_document* next = doc->document_next != Active_docs ? doc->document_next : nullptr;
        if (doc->m_marked_for_termination)
        {
            vint_lua_document_destroy(doc);
            doc->clean_up();
            doc->return_to_factory();
        }
        doc = next;
    }
}

// 0x0047CA00 (:1456)
void vint_document::render_all()
{
    // by depth, deepest first
    vint_document* list[68];
    unsigned int count = 0;
    Elements_rendered = 0;
    vint_document* doc = Active_docs;
    while (doc != nullptr)
    {
        list[count++] = doc;
        doc = doc->document_next;
        if (doc == Active_docs)
        {
            break;
        }
    }
    qsort(list, count, sizeof(vint_document*), compare_docs_for_render);
    for (unsigned int i = 0; i < count; ++i)
    {
        list[i]->render();
    }
}

// 0x00464CB0 (:1489)
unsigned short vint_document::get_num_critical_resources()
{
    return m_num_critical_resources;
}

// 0x00464CC0 (:1495)
const vint_critical_resources* vint_document::get_critical_resource(int which)
{
    if (which < 0 || which >= (int)m_num_critical_resources)
    {
        return nullptr;
    }
    return &m_critical_resources[which];
}

// The binary header, field by field (the file is packed: no padding after the version).
static bool vint_document_read_bin_header(vint_load_buffer& _buffer, vint_bin_file_header& _header)
{
    _header = vint_bin_file_header();
    float time_index;
    _buffer.get_data(_header.ID);
    _buffer.get_data(_header.doc_name);
    _buffer.get_data(_header.version);
    // Original: the document's time index is skipped
    _buffer.get_data(time_index);
    _buffer.get_data(_header.num_metadata_entries);
    _buffer.get_data(_header.num_crit_resources);
    _buffer.get_data(_header.strings_offset);
    _buffer.get_data(_header.num_root_element_children);
    _buffer.get_data(_header.num_root_object_children);
    return _header.ID == VINT_BIN_DOC_ID && _header.version == VINT_BIN_DOC_VERSION;
}

// 0x0047CA70 (:1505)
bool vint_document::load_from_binary(vint_load_buffer& _buffer, mempool_base* _mem_pool)
{
    if (m_root_element == nullptr || m_root_object == nullptr)
    {
        return false;
    }
    if (_mem_pool == nullptr)
    {
        _mem_pool = Vint_temp_mempool;
    }
    vint_bin_file_header header;
    if (!vint_document_read_bin_header(_buffer, header))
    {
        return false;
    }
    // the string table at its offset
    vint_load_buffer strings_buffer(_buffer);
    strings_buffer.set_pos(header.strings_offset);
    vint_bin_file_strings strings;
    if (!strings.load_from_buffer(strings_buffer, _mem_pool))
    {
        keen::debug::nativePrintArgs("Error loading string table");
        return false;
    }
    for (unsigned int i = header.num_crit_resources; i != 0; --i)
    {
        unsigned char type = 0;
        unsigned int name = 0;
        _buffer.get_data(type);
        _buffer.get_data(name);
        if (type == VINT_CRIT_RSC_PEG)
        {
            add_critical_resource_peg(strings[name]);
        }
    }
    if (Doc_load_resource_callback != nullptr)
    {
        Doc_load_resource_callback(this);
    }
    for (unsigned int i = header.num_metadata_entries; i != 0; --i)
    {
        unsigned int name = 0;
        unsigned int value = 0;
        _buffer.get_data(name);
        _buffer.get_data(value);
        if (m_num_metadata_pairs < 16)
        {
            strncpy_safe(m_metadata[m_num_metadata_pairs].name, strings[name], 32);
            strncpy_safe(m_metadata[m_num_metadata_pairs].value, strings[value], 32);
            ++m_num_metadata_pairs;
        }
    }
    for (unsigned int i = header.num_root_element_children; i != 0; --i)
    {
        vint_object_base::load_from_binary(_buffer, m_root_element, strings);
    }
    for (unsigned int i = header.num_root_object_children; i != 0; --i)
    {
        vint_object_base::load_from_binary(_buffer, m_root_object, strings);
    }
    return true;
}

// 0x00480F90 (:1860)
void vint_document::parse_xml_property_set(xml_element* _root)
{
    // each named object's saved properties, in its type's property order
    if (_root == nullptr)
    {
        return;
    }
    for (xml_element* object = xml_find(_root, "object"); object != nullptr; object = xml_find_next(_root, object, "object"))
    {
        xml_element* name = xml_find(object, "name");
        const char* name_string = name != nullptr ? xml_get_string(name, nullptr) : nullptr;
        vint_object_base* target = name_string != nullptr ? vint_object_base::find_object(name_string, this) : nullptr;
        if (target == nullptr)
        {
            continue;
        }
        vint_object_properties_reflect reflect(target->get_type());
        for (int p = 0; p < reflect.m_num_props; ++p)
        {
            const vint_prop_lookup& info = reflect.m_prop_info[p];
            if (!info.editor_save)
            {
                continue;
            }
            for (xml_element* prop = xml_find(object, "property"); prop != nullptr; prop = xml_find_next(object, prop, "property"))
            {
                xml_element* prop_name = xml_find(prop, "name");
                if (prop_name == nullptr)
                {
                    continue;
                }
                const char* prop_name_string = xml_get_string(prop_name, nullptr);
                if (v_stricmp(prop_name_string, info.name) != 0 || prop_name_string == nullptr)
                {
                    continue;
                }
                xml_element* type = xml_find(prop, "type");
                const char* type_string = type != nullptr ? xml_get_string(type, nullptr) : nullptr;
                xml_element* value = type_string != nullptr ? xml_find(prop, "value") : nullptr;
                const char* value_string = value != nullptr ? xml_get_string(value, nullptr) : nullptr;
                vint_variant_type value_type = value_string != nullptr ? vint_variant::get_type_from_name(type_string) : VINT_PROP_TYPE_NONE;
                if (value_type == VINT_PROP_TYPE_NONE)
                {
                    continue;
                }
                vint_variant v;
                v.from_string(value_string, value_type);
                target->set_property_value(prop_name_string, v, 0);
                break;
            }
        }
    }
}

// 0x00481CD0 (:1968)
void vint_document::parse_xml_properties(xml_element* _root, bool for_anims)
{
    // the baseline, then the overrides of the current resolution mode
    if (_root == nullptr)
    {
        return;
    }
    xml_element* properties = xml_find(_root, for_anims ? "animation_properties" : "object_properties");
    if (properties == nullptr)
    {
        return;
    }
    if (xml_element* baseline = xml_find(properties, "baseline"))
    {
        parse_xml_property_set(baseline);
    }
    if (m_resolution_callback != nullptr)
    {
        m_resolution_callback->init_for_overrides(for_anims);
    }
    if (Vint_current_resolution_mode == -1)
    {
        return;
    }
    const vint_resolution_mode* mode = get_project_get_resolution_mode_info(Vint_current_resolution_mode);
    xml_element* overrides = xml_find(properties, "overrides");
    if (overrides == nullptr)
    {
        return;
    }
    m_which_resolution = 0;
    // up to the first matching "w,h"
    for (xml_element* res = xml_find(overrides, "resolution"); res != nullptr; res = xml_find_next(overrides, res, "resolution"))
    {
        bool done = false;
        const char* value = xml_get_string(res, nullptr);
        const char* comma = value != nullptr ? strstr(value, ",") : nullptr;
        if (comma != nullptr)
        {
            char h_string[32] = {};
            char w_string[32] = {};
            keen::formatString(w_string, 32, value);
            keen::formatString(h_string, 32, comma + 1);
            int h = atoi(h_string);
            int w = atoi(w_string);
            // Original: a null mode (an out-of-range current mode) is dereferenced
            if (mode->hres == w && mode->vres == h)
            {
                done = true;
                // Original: the resolution passed is a stack slot holding the overrides element's address (unused)
                vector2i resolution;
                resolution.x = (int)(uintptr_t)overrides;
                resolution.y = 0;
                parse_xml_resolution_property_set(res, resolution, for_anims, true);
            }
        }
        ++m_which_resolution;
        if (done)
        {
            break;
        }
    }
}

// 0x004811C0 (:2094)
void vint_document::parse_xml_resolution_property_set(xml_element* _root, const vector2i& resolution, bool for_anims, bool set_info)
{
    // typed name/value pairs set on the named objects (resolution, for_anims and set_info are unused)
    if (_root == nullptr)
    {
        return;
    }
    for (xml_element* object = xml_find(_root, "object"); object != nullptr; object = xml_find_next(_root, object, "object"))
    {
        xml_element* name = xml_find(object, "name");
        if (name == nullptr)
        {
            continue;
        }
        const char* object_name = xml_get_string(name, nullptr);
        for (xml_element* prop = xml_find(object, "property"); prop != nullptr; prop = xml_find_next(object, prop, "property"))
        {
            vint_variant value;
            xml_element* e = xml_find(prop, "name");
            const char* prop_name = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
            e = prop_name != nullptr ? xml_find(prop, "type") : nullptr;
            const char* type = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
            e = type != nullptr ? xml_find(prop, "value") : nullptr;
            const char* string = e != nullptr ? xml_get_string(e, nullptr) : nullptr;
            if (string == nullptr)
            {
                continue;
            }
            if (v_strnicmp(type, "int", 3) == 0)
            {
                value = atoi(string);
            }
            else if (v_strnicmp(type, "uint", 4) == 0)
            {
                value = (unsigned int)atoi(string);
            }
            else if (v_strnicmp(type, "float", 5) == 0)
            {
                value = (float)atof(string);
            }
            else if (v_strnicmp(type, "string", 6) == 0)
            {
                // fonts and bitmaps by name become handles
                if (v_strnicmp(prop_name, "font", 4) == 0)
                {
                    if (vint_resource_font* font = vint_resource_font::load(string))
                    {
                        value = font->get_handle();
                    }
                }
                else if (v_strnicmp(prop_name, "bitmap", 6) == 0)
                {
                    if (vint_resource_bitmap* bitmap = vint_resource_bitmap::load(string))
                    {
                        value = bitmap->get_handle();
                    }
                }
                else
                {
                    value = string;
                }
            }
            else if (v_strnicmp(type, "bool", 4) == 0)
            {
                value = v_strnicmp(string, "true", 4) == 0;
            }
            else if (v_strnicmp(type, "color", 5) == 0)
            {
                // Original bug kept: the parse never advances through a non-empty value (it loops for ever); an
                // empty one is black, and the red and green parts would have been dropped anyway
                color c;
                c.red = 0;
                c.green = 0;
                c.alpha = 0xFF;
                char part[32] = {};
                unsigned int len = 0;
                int index = 0;
                if (string[0] != '\0')
                {
                    for (;;)
                    {
                        if (string[0] != ',')
                        {
                            part[len++] = string[0];
                            continue;
                        }
                        if (index == 0 || index == 1)
                        {
                            atof(part);
                        }
                        ++index;
                        len = 0;
                        memset(part, 0, sizeof(part));
                    }
                }
                // x87 truncation
                c.blue = (unsigned char)(int)((double)(float)atof(part) * 255.0);
                value = c;
            }
            else if (v_strnicmp(type, "vector2f", 8) == 0)
            {
                // "x,y": the first part and the last
                vector2 v;
                v.x = 0.0f;
                char part[132] = {};
                int part_len = 0;
                int index = 0;
                unsigned int len = (unsigned int)strlen(string);
                for (unsigned int i = 0; i < len; ++i)
                {
                    if (string[i] == ',')
                    {
                        if (index == 0)
                        {
                            v.x = (float)atof(part);
                        }
                        ++index;
                        part_len = 0;
                        memset(part, 0, sizeof(part));
                    }
                    else
                    {
                        part[part_len++] = string[i];
                    }
                }
                v.y = (float)atof(part);
                value = v;
            }
            if (object_name != nullptr)
            {
                if (vint_object_base* target = vint_object_base::find_object_by_name_crc(crc_stri(object_name, 0), this))
                {
                    target->set_property_value(prop_name, value, 0);
                }
            }
        }
    }
}

// 0x004816C0 (:2319)
void vint_document::parse_xml_elements(xml_element* _root)
{
    if (_root == nullptr)
    {
        return;
    }
    xml_element* elements = xml_find(_root, "elements");
    if (elements == nullptr)
    {
        return;
    }
    // Original: the next object is searched under the document root, not under elements
    for (xml_element* e = xml_find(elements, "object"); e != nullptr; e = xml_find_next(_root, e, "object"))
    {
        xml_element* name = xml_find(e, "name");
        const char* name_string = name != nullptr ? xml_get_string(name, nullptr) : nullptr;
        xml_element* type = name_string != nullptr ? xml_find(e, "type") : nullptr;
        const char* type_string = type != nullptr ? xml_get_string(type, nullptr) : nullptr;
        if (type_string == nullptr)
        {
            continue;
        }
        // Original: the locked flag is looked up and ignored
        xml_find(e, "locked");
        vint_object_base* obj = vint_object_base::create_new(m_root_element, name_string, type_string);
        if (obj == nullptr)
        {
            continue;
        }
        if (obj->is_type(vint_element_base::Object_type))
        {
            obj->xml_parse_children(e);
        }
        else
        {
            obj->destroy();
        }
    }
}

// 0x004817C0 (:2391)
void vint_document::parse_xml_objects(xml_element* _root)
{
    if (_root == nullptr)
    {
        return;
    }
    xml_element* animations = xml_find(_root, "animations");
    if (animations == nullptr)
    {
        return;
    }
    // Original: the next object is searched under the document root
    for (xml_element* e = xml_find(animations, "object"); e != nullptr; e = xml_find_next(_root, e, "object"))
    {
        xml_element* name = xml_find(e, "name");
        const char* name_string = name != nullptr ? xml_get_string(name, nullptr) : nullptr;
        xml_element* type = name_string != nullptr ? xml_find(e, "type") : nullptr;
        const char* type_string = type != nullptr ? xml_get_string(type, nullptr) : nullptr;
        if (type_string == nullptr)
        {
            continue;
        }
        xml_find(e, "locked");
        if (vint_object_base* obj = vint_object_base::create_new(m_root_object, name_string, type_string))
        {
            obj->xml_parse_children(e);
        }
    }
}

// 0x00481EC0 (:2457)
bool vint_document::parse_xml_doc_version_1(xml_element* _root)
{
    parse_xml_metadata(_root);
    parse_xml_critical_resources(_root);
    parse_xml_elements(_root);
    parse_xml_objects(_root);
    parse_xml_properties(_root, false);
    parse_xml_properties(_root, true);
    return true;
}

// 0x0047F220 (:2607)
bool vint_document::reload_overrides_from_binary(vint_load_buffer& _buffer, mempool_base* _mem_pool)
{
    // the binary layout again, applying only the current resolution's overrides
    if (m_root_element == nullptr || m_root_object == nullptr)
    {
        return false;
    }
    if (_mem_pool == nullptr)
    {
        _mem_pool = Vint_temp_mempool;
    }
    vint_bin_file_header header;
    if (!vint_document_read_bin_header(_buffer, header))
    {
        return false;
    }
    vint_load_buffer strings_buffer(_buffer);
    strings_buffer.set_pos(header.strings_offset);
    vint_bin_file_strings strings;
    if (!strings.load_from_buffer(strings_buffer, _mem_pool))
    {
        keen::debug::nativePrintArgs("Error loading string table");
        return false;
    }
    for (unsigned int i = header.num_crit_resources; i != 0; --i)
    {
        unsigned char type;
        unsigned int name;
        _buffer.get_data(type);
        _buffer.get_data(name);
    }
    if (Doc_load_resource_callback != nullptr)
    {
        Doc_load_resource_callback(this);
    }
    for (unsigned int i = header.num_metadata_entries; i != 0; --i)
    {
        unsigned int name;
        unsigned int value;
        _buffer.get_data(name);
        _buffer.get_data(value);
    }
    for (unsigned int i = header.num_root_element_children; i != 0; --i)
    {
        vint_object_base::reload_overrides_from_binary(_buffer, m_root_element, strings);
    }
    for (unsigned int i = header.num_root_object_children; i != 0; --i)
    {
        vint_object_base::reload_overrides_from_binary(_buffer, m_root_object, strings);
    }
    return true;
}

// 0x0047F4E0 (:2686)
bool vint_document::reload_overrides_from_buffer(const char* _filename, mempool_base* _temp_pool)
{
    // only a buffered file (Original: the temp pool is not restored when there is none)
    unsigned int mark = _temp_pool->mark();
    vint_load_buffer* buffer = vint_document_find_data_buffer(crc_stri(_filename, 0));
    if (buffer == nullptr)
    {
        return false;
    }
    bool result = reload_overrides_from_binary(*buffer, _temp_pool);
    _temp_pool->restore_to_mark(mark);
    return result;
}

// 0x0047F570 (:2717)
bool vint_document::reload_overrides_from_file(const char* _filename, mempool_base* _temp_pool)
{
    if (_filename == nullptr)
    {
        return false;
    }
    if (_temp_pool == nullptr)
    {
        _temp_pool = Vint_temp_mempool;
    }
    unsigned int mark = _temp_pool->mark();
    bool result = reload_overrides_from_buffer(_filename, _temp_pool);
    _temp_pool->restore_to_mark(mark);
    return result;
}

// 0x004818A0 (:2744)
void vint_document::reload_overrides_all(mempool_base* temp_pool)
{
    if (temp_pool == nullptr)
    {
        return;
    }
    vint_document* doc = Active_docs;
    while (doc != nullptr)
    {
        doc->reload_overrides(temp_pool);
        doc = doc->document_next;
        if (doc == Active_docs)
        {
            break;
        }
    }
}

// 0x0047F5C0 (:2755)
bool vint_document::reload_overrides(mempool_base* temp_pool)
{
    if (temp_pool == nullptr)
    {
        return false;
    }
    const char* filename = vint_project_get_doc_filename(m_name);
    if (filename == nullptr)
    {
        return false;
    }
    unsigned int mark = temp_pool->mark();
    bool result = reload_overrides_from_buffer(filename, temp_pool);
    temp_pool->restore_to_mark(mark);
    return result;
}
