// Original: volition/rfg/code/objects/object.h
// Implementation: volition/rfg/code/objects/object.cpp
//
// object, the root of every world object (decompiled where noted), and the class registry.
// - object: position and orientation, the attachment hierarchy (child_ptr/child_next/child_prev siblings, parent
//   handle, attach_info from Attach_info_pool), the Havok body handle (havok_handle) and broadphase object (bp_handle,
//   havok_bpo), flags, and its slots in World's lists (all_index, type_index, subtype_index). handle is the world
//   handle (handle_manager, upper bits = zone). update_streaming_box refreshes last_known_bmin/bmax from get_bbox and
//   moves the broadphase object (havok_bpo_update). destroy_on_stream marks the object no_save/destroy_on_stream and
//   destroys it at once when it is not streamed. Concrete classes implement get_classname and pool_free (return to the
//   class pool) and declare get_type_static/get_subtype_static/get_bpo_index_static for their instantiator.
// - object_stream_resource<T>: resource-dependent data (renderable, Havok shape...) plus a loaded flag; set_loaded is
//   called from the stream callbacks of the owning class.
// - object_flags: flagged_list_state, the stream state (streaming/streamed/stub/persistent/original), save control
//   (no_save, force_full_save, serialize_protected), bpo_index and render_flags.
// - object_system_init initializes the human system and registers object_update_render_all as a render update task;
//   object_do_frame_postsim re-applies the transforms of attached children whose attach_info was not updated this frame
//   (walks up to the root parent, then adjust_child_transforms).
// - obj_class_instantiator (decompiled): every object class has one global instance (<class>_instantiator, for example
//   district_instantiator) whose constructor registers the class's alloc, type, bpo index and classname functions in
//   static tables indexed by str_hash_string(name) % hash_size (762, buckets of 768-entry tables). this_hash is the
//   class name hash that zone files store (zone_object::classname_hash). alloc_object(hash) allocates from the class
//   pool and sets obj_type/sub_type through the type function.
// - obj_prop_block: an rfg_prop_block with the position, orientation and name properties preset (second constructor).
// - object_properties: position and orientation (placement inferred).
// Placement of object_flags, remote_object_flags and object_contact_info inferred.
#pragma once

#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

struct attach_info_data;
class constraint;
struct contact_node;
class havok_bpo;
class hkpWorldObject;
class human;
struct lod_info;
class object;
class rl_renderable_mesh;
class rl_renderer;
struct weapon_fire_info;
struct weapon_info;

// size sizeof(T) + 4, aligned as T
template<typename T>
class object_stream_resource
{
public:
    void set_loaded(bool loaded); // <character_instance::resource_dependent_data> 0x007FA440 (.h:209)
    bool is_loaded() const; // <character_instance::resource_dependent_data> 0x005F5A20 (.h:214)
    T* get_data(); // <character_instance::resource_dependent_data> 0x006C9660 (.h:232)
    const T* get_data() const;

private:
    T m_data; // 0x00
    bool m_loaded; // sizeof(T)
};

// size 0x30
struct object_properties
{
    object_properties(); // 0x0091D030

    vector pos; // 0x00
    matrix orient; // 0x0C
};

// size 0xC
class obj_prop_block : public rfg_prop_block
{
public:
    obj_prop_block(); // 0x00A14100 (:231)
    obj_prop_block(const vector& pos, const matrix& orient, const char* name); // 0x00A38B00 (:212)
    ~obj_prop_block(); // 0x005DBB40

};

// size 0x4
class obj_class_instantiator
{
public:
    obj_class_instantiator(const char* name, object* (*af)(), void (*tf)(signed char&, signed char&), int (*bpf)(), const char* (*nf)(), int (*rf)(), int _pool_usage, int _max_objects); // 0x00710CE0 (object.inl:53)
    static int get_class_index(unsigned int str_hash); // 0x00919D40 (object.inl:119)
    static object* alloc_object(unsigned int str_hash); // 0x00919D60 (object.inl:134)
    static int bpo_index(unsigned int str_hash); // 0x00919DB0 (object.inl:160)
    static const char* get_classname(unsigned int);
    static object_type get_type(unsigned int str_hash); // 0x00A05490 (object.inl:175)
    static const char* get_classname_by_class_index(int);
    static int get_num_registered_classes();
    static void get_pool_usage_by_class_index(int cindex, int& total_bytes, int& _max_objects); // 0x00919DE0 (object.inl:197)
    static int get_class_index_by_types(object_type obj_type, object_sub_type sub_type); // 0x00919E10 (object.inl:218)
    static int get_class_index_by_type(object_type);
    static int get_class_index_by_subtype(object_sub_type sub_type); // 0x00919E80 (object.inl:252)

private:
    // world_serialize.cpp and world.cpp read the tables directly (inlined lookups); the friendship is inferred.
    friend class world;
    friend object_type object_type_by_hash(unsigned int hash);

    static bool li_init; // 0x0302F11D
    static int hash_size; // 0x01694538
    static object* (*alloc_funcs[768])(); // 0x0301AFD0
    static void (*type_funcs[768])(signed char&, signed char&); // 0x0301BBD0
    static int (*bpo_index_funcs[768])(); // 0x0302AD70
    static const char* (*classname_funcs[768])(); // 0x03025AA0
    static int pool_usage[768]; // 0x0302A168
    static int max_objects[768]; // 0x03024E98
    static int class_index[768]; // 0x0301A240
    static int num_registered_classes; // 0x0302F120
public:
    unsigned int this_hash; // 0x00
};

// Relative placement of an attached child (from Attach_info_pool, file static in human.cpp). Placement inferred.
// size 0x40
struct attach_info_data
{
    attach_info_data(); // 0x00A26480

    int parent_handle; // 0x00
    int parent_prop_point; // 0x04
    int child_prop_point; // 0x08
    matrix43 relative_transform; // 0x0C
    unsigned int use_relative_transform : 1; // 0x3C:0
    unsigned int update_physics : 1; // 0x3C:1
    unsigned int updated : 1; // 0x3C:2
};

// size 0x4
struct object_flags
{
    unsigned char flagged_list_state : 3; // 0x00:0
    unsigned char lighting_set_once : 1; // 0x00:3
    unsigned char destroyed : 1; // 0x00:4
    unsigned char no_save : 1; // 0x00:5
    unsigned char force_full_save : 1; // 0x00:6
    unsigned char destroy_on_stream : 1; // 0x00:7
    unsigned char created_by_mission_or_activity : 1; // 0x01:0
    unsigned char dont_transform : 1; // 0x01:1
    unsigned char world_free : 1; // 0x01:2
    unsigned char streaming : 1; // 0x01:3
    unsigned char streamed : 1; // 0x01:4
    unsigned char persistent : 1; // 0x01:5
    unsigned char original : 1; // 0x01:6
    unsigned char stub : 1; // 0x01:7
    unsigned char preserve_handle : 1; // 0x02:0
    unsigned char bpo_index : 2; // 0x02:1
    unsigned char is_dependent : 1; // 0x02:3
    unsigned char visited : 1; // 0x02:4
    unsigned char special_lifetime : 1; // 0x02:5
    unsigned char serialize_protected : 1; // 0x02:6
    unsigned char _dont_use_me : 1; // 0x02:7
    unsigned char streaming_fixed : 1; // 0x03:0
    unsigned char render_flags : 4; // 0x03:1
};

// size 0x1
struct remote_object_flags
{
    unsigned char remote_object : 1; // 0x00:0
    unsigned char client_sync : 1; // 0x00:1
    unsigned char client_destroy_permission : 1; // 0x00:2
    unsigned char no_destroy_packet : 1; // 0x00:3
    unsigned char common_create : 1; // 0x00:4
};

// size 0x4
struct object_contact_info
{
    contact_node* m_contact_list; // 0x00
};

// size 0x9C
class object
{
public:
    object(); // 0x00710C10 (.h:297)
    virtual ~object(); // vtbl+0x00 0x00710C30 (.h:298)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A14110 (:241)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A14150 (:247)
    virtual const char* get_classname() const = 0; // vtbl+0x0C
    virtual bool is_killed() const; // vtbl+0x10 0x00A14C90 (:781)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA2EA0 (:795)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A14CA0 (:846)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A14CB0 (:858)
    virtual bool serialize_restore_if_missing_in_save_file(obj_prop_block& pblock); // vtbl+0x20 0x00A14CD0 (:880)
    virtual bool serialize_restore_if_resetting_destruction(obj_prop_block& pblock); // vtbl+0x24 0x00A14CE0 (:887)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A8C2A0 (:905)
public:
    virtual void destroy_notify(); // vtbl+0x2C 0x00A14CF0 (:937)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00A8C310 (:955)
    void update_streaming_box(); // 0x00A14D00 (:974)
    virtual int get_tag_index_from_name(const char* tag_name) const; // vtbl+0x34 0x00A14D40 (:988)
    virtual bool get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const; // vtbl+0x38 0x00A14D50 (:994)
    virtual bool get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const; // vtbl+0x3C 0x00A14DA0 (:1005)
    virtual const rl_renderable_mesh* get_static_mesh() const; // vtbl+0x40 0x00710C40 (.h:344)
    void set_is_visible(bool is_visible); // 0x00A8C3E0 (:1015)
    bool get_is_visible() const; // 0x006C3970 (.h:348)
    void set_render_flags(int flags); // 0x00A5FCA0 (:1021)
    virtual void update_render(rl_renderer*); // vtbl+0x44 0x00A14DC0 (:1044)
    virtual bool enable_object_outline(int layer); // vtbl+0x48 0x00710C50 (.h:352)
    virtual bool disable_object_outline(); // vtbl+0x4C 0x00710C60 (.h:353)
    virtual int attach_index(const vector& _pos, unsigned int parent_objp_body_handle); // vtbl+0x50 0x00710C70 (.h:357)
    virtual bool attach_index_is_valid(int index); // vtbl+0x54 0x00710C80 (.h:358)
    virtual float get_mass() const; // vtbl+0x58 0x00A14AE0 (:630)
    virtual unsigned char get_physical_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x5C 0x00A14B00 (:645)
    virtual unsigned char get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x60 0x00A14B40 (:664)
    virtual float get_energy_scale(); // vtbl+0x64 0x00A14B80 (:681)
    virtual void process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info); // vtbl+0x68 0x00710C90 (.h:371)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info); // vtbl+0x6C 0x00710CA0 (.h:374)
    void remove_constraints(); // 0x00A14B90 (:687)
    void destroy_on_stream(); // 0x00A14BF0 (:702)
    void destroy_on_next_stream(); // 0x00705380 (.h:384)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A14290 (:273)
    bool set_child(object* child_objp, int parent_prop_point, int child_prop_point, bool maintain_transform, bool update_physics); // 0x00AA2C40 (:319)
    bool set_child(object* child_objp); // 0x00AA2BB0 (:278)
    bool remove_child(object* child_objp); // 0x00A8C1F0 (:375)
    bool remove_from_parent(); // 0x00A8C270 (:402)
    // <object_guard_node> 0x0064A430 (:273): the children of one type.
    template<typename T>
    void collect_children(base_array<T*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data) const
    {
        if (obj_list.full() || child_ptr == nullptr)
        {
            return;
        }
        object* child = child_ptr;
        do
        {
            if (child->get_obj_type() == T::get_type_static() && (cb_func == nullptr || cb_func(child, 0xFFFFFFFF, cb_data)))
            {
                obj_list.add(static_cast<T*>(child));
                if (obj_list.full())
                {
                    return;
                }
            }
            child = child->child_next;
        } while (child != child_ptr && child != nullptr);
    }
    void collect_children(base_array<object*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data) const; // 0x00A5FC10 (:418)
    bool has_child(); // 0x00A14280 (:267)
    void adjust_child_transforms(bool parent_disabled, bool move_all); // 0x00A142A0 (:495)
    void adjust_transforms(); // 0x00A146E0 (:568)
    int get_lod_by_distance(float distance, const lod_info* lod_infos, unsigned int num_lods); // 0x00A2C4D0 (.h:585)
    virtual bool ready_to_stream_out(); // vtbl+0x74 0x00710CB0 (.h:424)
    const char* get_name() const; // 0x00A14250 (:253)
    void set_name(const char* _name); // 0x00A14260 (:259)
    unsigned int get_handle() const; // 0x005DB950 (.h:432)
    unsigned int get_parent() const; // 0x005DB960 (.h:433)
    object_type get_obj_type() const; // 0x005DB970 (.h:435)
    object_sub_type get_sub_type() const; // 0x005DB980 (.h:436)
    bool streamed() const; // 0x005DB990 (.h:438)
    unsigned int get_srid() const; // 0x00A05480 (.h:439)
    void set_persistent(bool set); // 0x00A14DD0 (:1048)
    bool is_persistent() const; // 0x00919D10 (.h:442)
    bool is_original() const; // 0x00919D20 (.h:443)
    void set_streaming_dependent(bool streaming_dependent); // 0x00A14E30 (:1070)
    virtual unsigned int get_alt_hk_body_handle(unsigned int index) const; // vtbl+0x78 0x00A14E80 (:1116)
    virtual unsigned int get_alt_hk_body_handle_index(unsigned int alt_handle) const; // vtbl+0x7C 0x00A14EB0 (:1137)
    havok_bpo* get_bpo_handle(); // 0x00919D30 (.h:482)
protected:
    bool request_streaming_resource(unsigned int _srid, stream2_priority priority); // 0x00A14C10 (:718)
private:
    // world manages the handle, the lists and the flagged state of every object (inferred).
    friend class world;
    friend class obj_class_instantiator; // alloc_object sets obj_type and sub_type (inferred)
    friend class general_mover; // serialize updates bp_handle (inferred)
    friend class projectile; // update_pos_and_orient updates bp_handle (inferred)
    friend class trigger_region; // update_pos_and_orient updates bp_handle (inferred)
    friend class object_bounding_box; // update_pos_and_orient updates bp_handle (inferred)
    friend class marauder_ambush_region; // update_pos_and_orient updates bp_handle (inferred)
    void reset(); // 0x00A14C50 (:741)
    virtual void pool_free() = 0; // vtbl+0x80

    // 0x00 vfptr
public:
    vector pos; // 0x04
    matrix orient; // 0x10
    object* child_ptr; // 0x34
    object* child_next; // 0x38
    object* child_prev; // 0x3C
    constraint* child_constraint_ptr; // 0x40
    constraint* host_constraint_ptr; // 0x44
    attach_info_data* attach_info; // 0x48
    unsigned int havok_handle; // 0x4C
    object_contact_info contact_info; // 0x50
    object_flags obj_flags; // 0x54
    remote_object_flags remote_obj_flags; // 0x58
    unsigned char mp_dcmo_index; // 0x59
protected:
    int checking_reset; // 0x5C
private:
    unsigned short name_index; // 0x60
    object* flagged_next; // 0x64
    object* flagged_prev; // 0x68
    unsigned int handle; // 0x6C
    unsigned int parent; // 0x70
    havok_bpo* bp_handle; // 0x74
    unsigned short all_index; // 0x78
    unsigned short type_index; // 0x7A
    unsigned short subtype_index; // 0x7C
    signed char obj_type; // 0x7E
    signed char sub_type; // 0x7F
public:
    vector last_known_bmin; // 0x80
    vector last_known_bmax; // 0x8C
private:
    unsigned int srid; // 0x98
};

extern float Shard_fade_dist0_sqr; // 0x0169453C
extern float Shard_fade_dist1_sqr; // 0x01694540
extern float RFG_object_energy_scale; // 0x01694544

void unused_dcf_shard_fade_dist0(); // 0x00A13EF0 (:129)
void unused_dcf_shard_fade_dist1(); // 0x00A13F80 (:137)
void unused_dcf_lod_level(); // 0x00A14010 (:147)
void object_system_init(); // 0x00AB9DE0 (:1194)
void object_level_init_pre(); // 0x00A38C40 (:1208)
void object_level_init_post(); // 0x00AE05C0 (:1220)
void object_level_shutdown(); // 0x00AB3A50 (:1241)
void object_render_debug(); // 0x00A14EE0 (:1279)
void object_do_frame_postsim(); // 0x00A5FCF0 (:1286)
void object_set_visibility_plus_children(object* objp, bool render); // 0x00A8C440 (:1316)
bool object_is_player(unsigned int handle); // 0x00A14EF0 (:1327)
bool object_is_player(const object* objp); // 0x00A14F20 (:1333)
bool object_is_local_player(unsigned int handle); // 0x00A14F40 (:1338)
bool object_is_local_player(const object* objp); // 0x00A14F60 (:1348)
object_type object_type_by_hash(unsigned int hash); // 0x00A14F80 (:1353)

#include "volition/rfg/code/objects/object.inl"
