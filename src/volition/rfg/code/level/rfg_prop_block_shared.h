// Original: volition/rfg/code/level/rfg_prop_block_shared.h
// Implementation: volition/rfg/code/level/rfg_prop_block_shared.cpp
//
// Property block: the serialized form of an object's properties in zone files and saves (decompiled).
// prop_list holds num_props records back to back, each an obj_prop header (type, size, name_crc) followed by size
// bytes of data padded to 4. load(uint8*) points into a loaded image ({u16 num_props, u16 size} then the records) and
// does not own it (owner == 0, read-only); set_internal on an empty block allocates the buffer from Prop_data_pool
// (page_mempool, file static in rfg_prop_block_shared.cpp) and grows it with expand_buffer. find_prop is a linear search
// by name_crc. Typed access goes through the get<T>/set<T> templates (data copied by size); get_str_as_enum and
// get_str_as_flags map strings through a str_data_hash_table. The non-optional getters assert when the property is
// missing.
#pragma once

#include "volition/rfg/code/level/rfg_prop_name_shared.h"
#include "volition/vlib/code/util/hash_table.h"

#include <string.h>

struct cfile;

#pragma warning(push)
#pragma warning(disable: 4200) // warning C4200: nonstandard extension used: zero-sized array in struct/union
// size 0x8
struct obj_prop
{
    unsigned short type; // 0x00
    unsigned short size; // 0x02
    unsigned int name_crc; // 0x04
    unsigned char data[0]; // 0x08
};
#pragma warning(pop)

// size 0xC
class rfg_prop_block
{
public:
    rfg_prop_block(); // 0x0091A550 (:60)
    ~rfg_prop_block(); // 0x00921950 (:69)
    void clear(); // 0x0091A560 (:76)
    bool load(cfile* f); // 0x0091A7C0 (:296)
    bool load(unsigned char* data); // 0x0091A790 (:282)
    bool save(cfile* f) const; // 0x0091A7F0 (:302)
    void get(const prop_name& name, char* str) const; // 0x00921980 (:316)
    bool get_optional(const prop_name& name, char* str) const; // 0x0091A830 (:326)
    void set(const prop_name& name, char* str); // 0x0091A8C0 (:356)
    void set(const prop_name& name, const char* str); // 0x0091A870 (:343)
    int get_buf(const prop_name& name, unsigned char*& buf) const; // 0x009219B0 (:362)
    int get_buf_optional(const prop_name& name, unsigned char*& buf) const; // 0x0091A910 (:375)
    int get_buf_copy(const prop_name& name, unsigned char* buf, int max_size) const; // 0x009219E0 (:392)
    int get_buf_copy_optional(const prop_name& name, unsigned char* buf, int max_size) const; // 0x0091A940 (:404)
    void set_buf(const prop_name& name, const unsigned char* buf, int size); // 0x0091A990 (:416)
    const char* get_str_ref(const prop_name& name) const; // 0x00921A30 (:429)
    const char* get_str_ref_optional(const prop_name& name) const; // 0x0091A9C0 (:441)
    void get_str_as_flags(const prop_name& name, int& flags, str_data_hash_table<unsigned int,32,32,unsigned char>& table) const; // 0x009309D0 (:455)
    bool get_str_as_flags_optional(const prop_name& name, int& flags, str_data_hash_table<unsigned int,32,32,unsigned char>& table) const; // 0x00928CF0 (:465)
    void set_flags_as_str(const prop_name& name, int flags, str_data_hash_table<unsigned int,32,32,unsigned char>& table); // 0x00928E00 (:489)
    int get_num_props() const;
    template<typename T>
    void get(const prop_name& name, T& objp) const; // <float> 0x0077CF60 (.h:177)
    template<typename T>
    bool get_optional(const prop_name& name, T& objp) const; // <cover_node_flags> 0x00A250A0 (.h:187)
    template<typename T>
    void set(const prop_name& name, const T& objp); // <rfg_move_type> 0x00718E30 (.h:210)
    template<typename T>
    void get_str_as_enum(const prop_name& name, T& en, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table) const; // <signed char> 0x00A78220 (.h:233)
    template<typename T>
    bool get_str_as_enum_optional(const prop_name& name, T& en, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table) const; // <bounding_box_types> 0x00A4AA40 (.h:241)
    void destructive_copy(rfg_prop_block& src); // 0x0091A590 (:90)
protected:
    obj_prop* find_prop(const prop_name& name, const unsigned int name_crc) const; // 0x0091A5D0 (:126)
    bool get_internal(obj_prop*& prop_ref, const prop_name& name, int type) const; // 0x0091A620 (:173)
    obj_prop* set_internal(const prop_name& name, int size, int type); // 0x0091A6E0 (:229)
    bool expand_buffer(int req); // 0x0091A650 (:192)

    // The mover serializers and property blocks read typed properties through get_internal (inferred).
    friend class object_mover;
    friend class general_mover;
    friend class gm_prop_block;
    friend class rfg_mover;
    friend class obj_rfgm_props;
    friend class district;
    friend class obj_light;

    unsigned short num_props; // 0x00
    unsigned short owner; // 0x02
    unsigned short buf_pos; // 0x04
    unsigned short buf_size; // 0x06
    unsigned char* prop_list; // 0x08
};

void rfg_prop_level_init_pre(); // 0x0091A9F0 (:526)
void rfg_prop_set_thread_ownership(); // 0x0091AA00 (:530)

// ---- template members (inline in the original header)

// <float> 0x0077CF60 (.h:177)
template<typename T>
void rfg_prop_block::get(const prop_name& name, T& objp) const
{
    obj_prop* prop;
    if (get_internal(prop, name, 5) && prop->size == sizeof(T))
    {
        memcpy(&objp, prop->data, sizeof(T));
    }
}

// <object_properties> 0x00920F10 (.h:187)
template<typename T>
bool rfg_prop_block::get_optional(const prop_name& name, T& objp) const
{
    obj_prop* prop = find_prop(name, name.crc);
    if (prop != nullptr && prop->type == 5 && prop->size == sizeof(T))
    {
        memcpy(&objp, prop->data, sizeof(T));
        return true;
    }
    return false;
}

// <float> 0x00718D30 (.h:210)
template<typename T>
void rfg_prop_block::set(const prop_name& name, const T& objp)
{
    obj_prop* prop = set_internal(name, sizeof(T), 5);
    if (prop != nullptr)
    {
        memcpy(prop->data, &objp, sizeof(T));
    }
}

// <signed char> 0x00A78220 (.h:233)
template<typename T>
void rfg_prop_block::get_str_as_enum(const prop_name& name, T& en, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table) const
{
    // A missing property only formats "Could not find object parameter : %s" into the thread's scratch buffer.
    get_str_as_enum_optional(name, en, table);
}

// <signed char> 0x00A50750 (.h:241)
template<typename T>
bool rfg_prop_block::get_str_as_enum_optional(const prop_name& name, T& en, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table) const
{
    char str[4092];
    if (!get_optional(name, &str[0]))
    {
        return false;
    }
    // An unknown name leaves en unchanged but still counts as found.
    unsigned int value;
    if (table.get(str, value))
    {
        en = (T)value;
    }
    return true;
}
