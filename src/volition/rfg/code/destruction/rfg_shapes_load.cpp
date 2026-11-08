// Original: volition/rfg/code/destruction/rfg_shapes_load.cpp
//
// Loading a destroyable from chunk data in place (decompiled). Layout after the 16-aligned rfg_base_destroyable_shape:
// 128-aligned subpiece bases (0x40 each), their extra data (0xC each), each piece's link index list (u16), then
// 4-aligned links (0x10 each), dlods (0x3C each), the rbb tree (a 0x14-byte header then the nodes), the instance data
// header (0x14) followed by the template instance buffer, and the 16-aligned transform buffer (its first int is its
// size). Compiled in rfg_destruction_unity.cpp.
//
// Pieces whose physical material index is 0xFF name their material by checksum in the extra data's h field; it is
// resolved here (unknown names get the default material) and h is cleared. Material flags: 1 shatter (piece flags
// 0x42), 0x20 structural (otherwise piece flag 8, not counted in the structural mass). Masses above 0.004 are scaled by
// the material density.

#include "volition/rfg/code/destruction/rfg_shapes.h"

#include "volition/rfg/code/destruction/rfg_rbb_tree.h"

// Next multiple of `alignment` (the original rounds with a signed remainder).
static int shapes_align(int offset, int alignment)
{
    const int r = offset % alignment;
    return r == 0 ? offset : offset + (alignment - r);
}

// 0x00737240 (:58)
rfg_base_destroyable_shape* rfg_base_destroyable_shape_load(unsigned char* data, int& offset, c_chunk_base* chunk_base)
{
    offset = shapes_align(offset, 0x10);
    rfg_base_destroyable_shape* shape = (rfg_base_destroyable_shape*)(data + offset);
    offset = shapes_align(offset + (int)sizeof(rfg_base_destroyable_shape), 0x80);
    shape->base_objects.from_pointer((rfg_subpiece_base*)(data + offset));
    offset += shape->num_objects * (int)sizeof(rfg_subpiece_base);
    shape->base_extra_data.from_pointer((rfg_subpiece_base_extra_data*)(data + offset));
    offset += shape->num_objects * (int)sizeof(rfg_subpiece_base_extra_data);
    shape->base_structural_mass = 0.0f;

    rfg_subpiece_base* objects = shape->base_objects.to_pointer();
    rfg_subpiece_base_extra_data* extra = shape->base_extra_data.to_pointer();
    for (int i = 0; i < shape->num_objects; i++)
    {
        rfg_subpiece_base& sp = objects[i];
        extra[i].shape.set_offset(-1);
        if (sp.physical_material_index == 0xFF)
        {
            int index = 0;
            for (; index < Num_rfg_physical_materials; index++)
            {
                if (extra[i].h == RFG_physical_materials[index].name_checksum)
                {
                    break;
                }
            }
            if (index >= Num_rfg_physical_materials)
            {
                index = Rfg_physical_material_default_index;
            }
            sp.physical_material_index = (unsigned char)index;
            extra[i].h = 0;
        }
        const unsigned char mat_index = sp.physical_material_index;
        const rfg_physical_material* material = Rfg_physical_material_default;
        if (mat_index != 0xFF && mat_index < Num_rfg_physical_materials)
        {
            material = &RFG_physical_materials[mat_index];
        }
        if (material != nullptr)
        {
            if ((material->flags & 1) != 0)
            {
                sp.flags |= 0x42;
            }
            if (0.004f < sp.mass)
            {
                sp.mass = material->density * sp.mass;
            }
            if ((material->flags & 0x20) == 0)
            {
                sp.flags |= 8;
            }
            if ((sp.flags & 0x28) == 0)
            {
                shape->base_structural_mass += sp.mass;
            }
        }
        sp.links.from_pointer((unsigned short*)(data + offset));
        offset += sp.num_links * 2;
    }

    offset = shapes_align(offset, 4);
    shape->base_links.from_pointer((rfg_link_base*)(data + offset));
    offset += shape->num_links * (int)sizeof(rfg_link_base);
    offset = shapes_align(offset, 4);
    shape->base_dlods.from_pointer((rfg_dlod_base*)(data + offset));
    offset += shape->num_dlods * (int)sizeof(rfg_dlod_base);
    offset = shapes_align(offset, 4);
    rfg_rbb_node* tree = (rfg_rbb_node*)(data + offset);
    offset += 0x14;
    rfg_rbb_read_nodes(tree, data, offset);
    shape->aabb_tree.from_pointer(tree);
    offset = shapes_align(offset, 4);

    shape->reset_link_strengths();
    // Links between a piece with base flag 0x40 and one without get flag 4.
    rfg_link_base* links = shape->base_links.to_pointer();
    for (int i = 0; i < shape->num_links; i++)
    {
        if (((objects[links[i].obj[1]].flags ^ objects[links[i].obj[0]].flags) & 0x40) != 0)
        {
            links[i].flags |= 4;
        }
    }

    // Instance data and its template buffer, whose link strengths start at the links' maximum.
    shape->inst_data.from_pointer((rfg_destroyable_instance_data*)(data + offset));
    offset += (int)sizeof(rfg_destroyable_instance_data);
    rfg_destroyable_instance_data* inst = shape->inst_data.to_pointer();
    inst->buffer.from_pointer(data + offset);
    offset += inst->data_size;
    rfg_link* template_links = (rfg_link*)(inst->buffer.to_pointer() + inst->links_offset);
    for (int i = 0; i < shape->num_links; i++)
    {
        template_links[i].yield_limit = links[i].yield_max;
    }

    offset = shapes_align(offset, 0x10);
    shape->transform_buffer.from_pointer(data + offset);
    offset += *(int*)(data + offset);
    offset = shapes_align(offset, 0x10);
    return shape;
}

// 0x007283A0 (:186)
void rfg_shape_delete_base(rfg_base_destroyable_shape* base)
{
    if (base != nullptr)
    {
        base->delete_me();
    }
}
