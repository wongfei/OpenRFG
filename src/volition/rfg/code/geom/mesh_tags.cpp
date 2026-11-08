// Original: volition/rfg/code/geom/mesh_tags.cpp
//
// read_tag_data (static meshes read their tags with it) and the tag lookups by name (get_tag, get_nth_tag) and index
// (get_tag_transform, get_tag_world_transform; player_minimal.md P4).

#include "volition/rfg/code/geom/mesh_tags.h"

#include "volition/vlib/code/math/crc.h"

// 0x007F47F0 (:18): nothing to construct.
mesh_tags::mesh_tags()
{
}

// 0x00808740 (:30): the index of the first tag with the name's checksum (case-insensitive), -1 without one.
int mesh_tags::get_tag(const char* tag_name) const
{
    const unsigned int crc = crc_stri(tag_name, 0);
    const mesh_tag* tags = m_tags.to_pointer();
    for (int i = 0; i < m_num_tags; i++)
    {
        if (tags[i].m_name_crc == crc)
        {
            return i;
        }
    }
    return -1;
}

// 0x00808780 (:52): the index of the num_tag-th tag with the name's checksum (the first is 1), -1 without one.
int mesh_tags::get_nth_tag(const char* tag_name, int num_tag) const
{
    const unsigned int crc = crc_stri(tag_name, 0);
    const mesh_tag* tags = m_tags.to_pointer();
    int found = 1;
    for (int i = 0; i < m_num_tags; i++)
    {
        if (tags[i].m_name_crc == crc)
        {
            if (found == num_tag)
            {
                return i;
            }
            found++;
        }
    }
    return -1;
}

// 0x008087D0 (:78): the tag's transform; identity for an invalid index.
void mesh_tags::get_tag_transform(matrix43& tag_transform, int tag_index) const
{
    if (tag_index > -1 && tag_index < m_num_tags)
    {
        tag_transform = m_tags.to_pointer()[tag_index].m_transform;
        return;
    }
    tag_transform.make_identity();
}

// 0x00808840 (:94): the tag's transform in the space of base_transform. Each product and sum is rounded to float
// (r*x + u*y, + f*z, then the base translation).
void mesh_tags::get_tag_world_transform(matrix43& tag_transform, int tag_index, const matrix43 base_transform) const
{
    get_tag_transform(tag_transform, tag_index);
    const vector& t = tag_transform.m_translation;
    const matrix& b = base_transform.m_rotation;
    vector pos;
    pos.x = base_transform.m_translation.x + (b.rvec.x * t.x + b.uvec.x * t.y + b.fvec.x * t.z);
    pos.y = base_transform.m_translation.y + (t.x * b.rvec.y + b.uvec.y * t.y + b.fvec.y * t.z);
    pos.z = base_transform.m_translation.z + (t.x * b.rvec.z + b.uvec.z * t.y + b.fvec.z * t.z);
    const matrix orient = tag_transform.m_rotation * b;
    tag_transform = matrix43(pos, orient);
}

// 0x00808A80 (:104)
const mesh_tag* mesh_tags::get_tags(int& num_tags) const
{
    num_tags = m_num_tags;
    return m_tags.to_pointer();
}

// 0x00808A90 (:114): the tag count, then the tags (0x34 bytes each) in place.
void mesh_tags::read_tag_data(unsigned char* data, int& offset)
{
    if (data == nullptr)
    {
        return;
    }
    m_num_tags = *(int*)(data + offset);
    offset += 4;
    m_tags.from_pointer((mesh_tag*)(data + offset));
    offset += m_num_tags * (int)sizeof(mesh_tag);
}
