// Original: volition/rfg/code/objects/object_bounding_box.h
// Implementation: volition/rfg/code/objects/object_bounding_box.cpp
//
// Bounding box objects (OT_BOUNDING_BOX): a placed box region, used as a GPS target region or a rally point.
#pragma once

#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/bbox.h"

class matrix;
class vector;

enum bounding_box_types
{
    BBT_INVALID = -1,
    BBT_NONE = 0,
    BBT_GPS_TARGET = 1,
    NUM_BOUNDING_BOX_TYPES = 2,
};

// size 0xD0
class object_bounding_box : public object
{
public:
    object_bounding_box(); // 0x00A15CF0 (:33)
    static const object_type get_type_static() { return OT_BOUNDING_BOX; } // 0x005E9EC0 (.h:36)
    static const object_sub_type get_subtype_static() { return OT_SUB_UNDEFINED; } // 0x005E9ED0 (.h:36)
    static const char* get_classname_static() { return "object_bounding_box"; } // 0x00A15C60 (.h:36)
    static int get_bpo_index_static() { return 1; } // 0x00A15C70 (.h:36)
    virtual const char* get_classname() const { return "object_bounding_box"; } // vtbl+0x0C 0x00A15D10 (.h:36)
    static unsigned int get_classname_hash(); // 0x00A15CE0 (:32)
    static int pool_remaining(); // 0x00A395D0 (:32)
    virtual void pool_free(); // vtbl+0x80 0x00A8DD60 (:32)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A8DD70 (:123)
public:
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA4E80 (:45)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A15D30 (:89)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A15D50 (:129)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A15D90 (:136)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00A15DD0 (:146)
    virtual bool is_killed() const; // vtbl+0x10 0x00A15E70 (:161)
    virtual ~object_bounding_box(); // vtbl+0x00 0x00A15D20

private:
    friend class cartographer; // the GPS finds its regions by type (inferred)
    bbox bb; // 0x9C
    bbox local_bb; // 0xB4
public:
    bounding_box_types type; // 0xCC
};

const char* classname_object_bounding_box(); // 0x00A15CD0 (:32)
object* alloc_object_bounding_box(); // 0x00A615B0 (:32)
int bpo_index_object_bounding_box(); // 0x00A15CC0 (:32)
void types_object_bounding_box(signed char& type, signed char& subtype); // 0x00A15CB0 (:32)
bool object_bbox_get_rally_point(const object_bounding_box* bbp, vector& rally_point); // 0x00A39600 (:177)
const object_bounding_box* object_bbox_find_gps(const vector& point); // 0x00A615C0 (:199)
