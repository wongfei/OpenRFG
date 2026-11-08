// Original: volition/rfg/code/geom/mesh_tags.h
// Implementation: volition/rfg/code/geom/mesh_tags.cpp
//
// Named attachment points of a mesh (prop points, exhausts, seats): each mesh_tag has a name checksum, a transform
// relative to a bone and the bone index. Read from mesh files (read_tag_data); looked up by name.
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/util/endian_type.h"

// size 0x34
struct mesh_tag
{
    unsigned int m_name_crc; // 0x00
    matrix43 m_transform; // 0x04
};

// size 0x10
class mesh_tags
{
public:
    mesh_tags(); // 0x007F47F0 (:18)
    int get_tag(const char* tag_name) const; // 0x00808740 (:30)
    int get_nth_tag(const char* tag_name, int num_tag) const; // 0x00808780 (:52)
    void get_tag_transform(matrix43& tag_transform, int tag_index) const; // 0x008087D0 (:78)
    void get_tag_world_transform(matrix43& tag_transform, int tag_index, const matrix43 base_transform) const; // 0x00808840 (:94)
    const mesh_tag* get_tags(int& num_tags) const; // 0x00808A80 (:104)
    int get_num_tags() const; // 0x00B62A10 (.h:64)
    void read_tag_data(unsigned char* data, int& offset); // 0x00808A90 (:114)

private:
    int m_num_tags; // 0x00
    et_sized_native_pointer<mesh_tag> m_tags; // 0x08
};

// 0x00B62A10 (.h:64)
inline int mesh_tags::get_num_tags() const
{
    return m_num_tags;
}
