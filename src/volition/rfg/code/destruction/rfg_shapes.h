// Original: volition/rfg/code/destruction/rfg_shapes.h
// Implementation: volition/rfg/code/destruction/rfg_shapes.cpp
//
// Destructible shapes (decompiled from rfg_base_destroyable_shape_load, rfg_destroyable_shape's constructor and the
// accessors in rfg_shapes.inl).
// - rfg_base_destroyable_shape: the read-only part loaded from a chunk (rfg_shapes_load.cpp): one rfg_subpiece_base per
//   piece (local bounds, mass, links into the link table, physical material, shape type), one rfg_link_base per link
//   between two pieces (yield strength, contact area), rfg_dlod_base damage LODs (render subpieces replaced as pieces
//   break), and rfg_destroyable_instance_data, the size and offsets of the per-instance buffer.
// - rfg_destroyable_shape: one instance in the world, a Havok bv-tree shape (THIS_SHAPE_TYPE BV_TREE) whose child
//   shapes are the pieces that still exist. Its buffer (allocate_buffer, from Level_memmgr) holds the rfg_subpiece,
//   rfg_link and rfg_dlod arrays laid out by inst_data. Pieces are addressed by shape key (index into objects); the
//   rbb tree (rfg_rbb_node, aabb_tree, or override_tree after recalc_rbb_tree) answers the Havok queries.
//   Instances are found by shape_uid in Destroyable_uid_hash.
// - rfg_subpiece: runtime state of a piece: its links (indices into the link array), the owning shape
//   (my_owner_gm_handle), list links for the mover's piece list, shardify queue and stress evaluation, and flags.
// - rfg_dynamic_link: a link between two movers created at run time (CHUNK_FLAG_DYNAMIC_LINK), saved with the world.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/endian_type.h"
#include "volition/vlib/code/util/hash_table.h"

struct cfile;
class object;
class object_mover;
class rfg_mover;
struct rfg_rbb_node;
struct c_chunk_base;
class hkSphere;
class hkAabb;
class hkTransformf;
class hkVector4f;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;
class hkpCdBody;
class hkpRayHitCollector;
struct hkpShapeBufferStorage;
class rfg_destroyable_shape;

// size 0x18
struct shard_impulse_info
{
    void reset(); // 0x00710930 (.h:360)
    shard_impulse_info(); // 0x00717600

    vector impulse_pos; // 0x00
    vector impulse_dir; // 0x0C
};

// size 0x40
struct rfg_subpiece_base
{
    float get_max_dim() const; // 0x007109A0 (rfg_shapes.inl:50)
    bool is_structural(); // 0x00710A40 (rfg_shapes.inl:58)
    material_shape_type get_override_shape() const; // 0x00710A50 (rfg_shapes.inl:65)
    void set_override_shape(material_shape_type type); // 0x00710A60 (rfg_shapes.inl:70)

    vector prel_bmin; // 0x00
    vector prel_bmax; // 0x0C
    vector pos; // 0x18
    vector center_of_mass; // 0x24
    float mass; // 0x30
    unsigned int dlod_key; // 0x34
    et_ptr_offset<unsigned short,0> links; // 0x38
    unsigned char physical_material_index; // 0x3C
    signed char shape_type; // 0x3D
    unsigned char num_links; // 0x3E
    unsigned char flags; // 0x3F
};

// size 0xC
struct rfg_subpiece_base_extra_data
{
    et_ptr_offset<hkpShape,1> shape; // 0x00
    unsigned short collision_model; // 0x04
    unsigned short render_subpiece; // 0x06
    unsigned int h; // 0x08
};

// size 0x10
struct rfg_link_base
{
    int yield_max; // 0x00
    float area; // 0x04
    short obj[2]; // 0x08
    unsigned char flags; // 0x0C
    unsigned char padding[3]; // 0x0D
};

// size 0x4
struct rfg_link
{
    int yield_limit; // 0x00
};

// size 0x3C
struct rfg_dlod_base
{
    unsigned int name_hash; // 0x00
    vector pos; // 0x04
    matrix orient; // 0x10
    unsigned short render_subpiece; // 0x34
    unsigned short first_piece; // 0x36
    unsigned char max_pieces; // 0x38
    unsigned char padding[3]; // 0x39
};

// size 0x14
struct rfg_destroyable_instance_data
{
    unsigned int objects_offset; // 0x00
    unsigned int links_offset; // 0x04
    unsigned int dlods_offset; // 0x08
    unsigned int data_size; // 0x0C
    et_ptr_offset<unsigned char,0> buffer; // 0x10
};

// size 0x24
struct rfg_subpiece
{
    bool is_modified(); // 0x00710A70 (rfg_shapes.inl:95)
    void set_modified(bool modified); // 0x00710A80 (rfg_shapes.inl:101)
    void remove_link(int index); // 0x007235B0 (rfg_shapes.inl:119)
    void restore_link(int index); // 0x007235F0 (rfg_shapes.inl:134)
    void add_impulse(const vector& impulse, const vector& impulse_pos); // 0x00710AA0 (rfg_shapes.inl:151)
    void sort_links(); // 0x00727950 (:930)
    object_mover* get_owner() const; // 0x0071FFF0 (:953)
    void set_owner(object_mover* om); // 0x00736710 (:859)
    void set_destroyable(rfg_destroyable_shape* shape); // 0x00736790 (:967)
    rfg_destroyable_shape* get_destroyable() const; // 0x0071AC40 (rfg_shapes.inl:86)

    int anchor_yield; // 0x00
    et_ptr_offset<unsigned short,0> links; // 0x04
private:
    et_ptr_offset<rfg_destroyable_shape,1> my_owner_gm_handle; // 0x08
public:
    unsigned short next; // 0x0C
    unsigned short prev; // 0x0E
    unsigned short shard_next; // 0x10
    unsigned short shard_prev; // 0x12
    unsigned short stress_next; // 0x14
    unsigned short stress_prev; // 0x16
    unsigned int flags; // 0x18
    unsigned short dynamic_link_index; // 0x1C
    unsigned short eval_info_index; // 0x1E
    unsigned short impulse_info_index; // 0x20
    unsigned char modified_framecount; // 0x22
    unsigned char num_links; // 0x23
};

// size 0xC
struct rfg_dlod
{
    rfg_destroyable_shape* get_owner(); // 0x00736420 (:517)
    void get_subpiece_indices(int& num_indices, short* indices); // 0x007276E0 (:551)
    bool should_render(); // 0x00727770 (:564)

private:
    et_ptr_offset<rfg_destroyable_shape,1> owner; // 0x00
public:
    et_ptr_offset<rfg_subpiece,0> subpieces; // 0x04
    unsigned char active_pieces; // 0x08
    unsigned char flags; // 0x09
    unsigned char max_pieces; // 0x0A
    unsigned char padding[1]; // 0x0B
};

// size 0x2C
class rfg_base_destroyable_shape
{
public:
    rfg_base_destroyable_shape(); // 0x00736510 (:592)
    void save(cfile* f); // 0x00717610 (:607)
    void init_subpiece_shapes(); // 0x00736560 (:714)
    void reset_link_strengths(); // 0x007277C0 (:762)
    void get_subpiece_orient_local(const rfg_subpiece_base* sp_base, matrix& orient); // 0x00723550 (rfg_shapes.inl:38)
    void delete_me(); // 0x00727860 (:785)

    et_ptr_offset<rfg_rbb_node,0> aabb_tree; // 0x00
    et_ptr_offset<rfg_subpiece_base,0> base_objects; // 0x04
    et_ptr_offset<rfg_subpiece_base_extra_data,0> base_extra_data; // 0x08
    int num_objects; // 0x0C
    et_ptr_offset<rfg_link_base,0> base_links; // 0x10
    int num_links; // 0x14
    et_ptr_offset<rfg_dlod_base,0> base_dlods; // 0x18
    int num_dlods; // 0x1C
    et_ptr_offset<rfg_destroyable_instance_data,0> inst_data; // 0x20
    et_ptr_offset<unsigned char,0> transform_buffer; // 0x24
    float base_structural_mass; // 0x28
};

// size 0x94
class rfg_destroyable_shape : public hkpBvTreeShape, public hkpShapeContainer
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 22,
    };

    struct MustEndWithSemiColon; // no definition in PDB

    // HK_DECLARE_CLASS_ALLOCATOR (the PDB records the operators without their names)
    static void* operator new(unsigned int, void*);
    static void* operator new(unsigned int nbytes); // 0x0071AC20 (.h:485)
    static void operator delete(void*, void*);
    static void operator delete(void* p, unsigned int nbytes); // 0x00736A90 (.h:485)
    static void* operator new[](unsigned int, void*);
    static void operator delete[](void*, void*);
private:
    unsigned char* allocate_buffer(); // 0x007279C0 (:1140)
    void deallocate_buffer(); // 0x007176B0 (:1157)
public:
    rfg_destroyable_shape(rfg_base_destroyable_shape* base, unsigned int _chunk_uid, const vector& pos, const matrix& orient, unsigned int sibling_uid); // 0x007367C0 (:1041)
    void set_shape_state(int index, bool state); // 0x00737CA0 (rfg_shapes.inl:178)
    void set_shape_state(rfg_subpiece* sp, bool state); // 0x00736B20 (:1175)
    bool get_shape_state(int index) const; // 0x0071ACA0 (rfg_shapes.inl:207)
    bool get_shape_state(const rfg_subpiece* sp) const; // 0x0071AC60 (rfg_shapes.inl:186)
    unsigned int get_subpiece_index(const rfg_subpiece* sp) const; // 0x00710B80 (rfg_shapes.inl:213)
    rfg_subpiece* get_subpiece(unsigned int key) const; // 0x00710BA0 (rfg_shapes.inl:234)
    rfg_subpiece_base* get_subpiece_base(const rfg_subpiece* sp) const; // 0x00723650 (rfg_shapes.inl:282)
    rfg_subpiece_base* get_subpiece_base(unsigned int key) const; // 0x00723630 (rfg_shapes.inl:254)
    rfg_subpiece_base_extra_data* get_subpiece_base_extra_data(const rfg_subpiece* sp) const; // 0x007236A0 (rfg_shapes.inl:314)
    rfg_subpiece_base_extra_data* get_subpiece_base_extra_data(unsigned int key) const; // 0x00723680 (rfg_shapes.inl:287)
    rfg_mover* get_mover() const; // 0x00710940 (.h:563)
    void set_mover(rfg_mover* _mover_p); // 0x00710950 (.h:564)
    const rfg_rbb_node* get_rbb_tree() const; // 0x00710BB0 (rfg_shapes.inl:320)
    void get_subpiece_pos_orient_uncached(const rfg_subpiece* sp, vector& pos, matrix& orient); // 0x00727CA0 (:1581)
    int get_dlod_index(const rfg_dlod* dlod) const; // 0x00710BC0 (rfg_shapes.inl:391)
    rfg_dlod* get_dlod(int index) const; // 0x00710BE0 (rfg_shapes.inl:400)
    rfg_dlod_base* get_dlod_base(const rfg_dlod* dlod) const; // 0x007236F0 (rfg_shapes.inl:416)
    rfg_dlod_base* get_dlod_base(int index) const; // 0x007236D0 (rfg_shapes.inl:408)
    void get_dlod_pos_orient_uncached(const rfg_dlod* dlod, vector& pos, matrix& orient); // 0x00727E40 (:1601)
    int get_link_index(const rfg_link* link) const; // 0x00710BF0 (rfg_shapes.inl:421)
    rfg_link* get_link(const rfg_subpiece* sp, int index) const; // 0x00723770 (rfg_shapes.inl:451)
    rfg_link* get_link(int index) const; // 0x00710C00 (rfg_shapes.inl:430)
    rfg_link_base* get_link_base(const rfg_subpiece* sp, int index) const; // 0x00723790 (rfg_shapes.inl:459)
    rfg_link_base* get_link_base(const rfg_link* l) const; // 0x00723750 (rfg_shapes.inl:446)
    rfg_link_base* get_link_base(int index) const; // 0x00723730 (rfg_shapes.inl:438)
    rfg_subpiece* get_shardify_anchor() const; // 0x0071ACE0 (rfg_shapes.inl:469)
    bool is_valid() const; // 0x00710960 (.h:590)
    void create_world_anchor(rfg_mover* rm, rfg_subpiece* sp); // 0x0072FBF0 (:1269)
    void delete_subpiece(rfg_subpiece* sp); // 0x0072FD40 (:1310)
    bool fade_subpiece(rfg_subpiece* sp, bool fade_in); // 0x0072FE10 (:1389)
    void remove_link(rfg_link* l); // 0x007279F0 (:1490)
    void damage_link(rfg_link* l, int damage); // 0x007176D0 (:1518)
    void repair_link(rfg_link* l, int repair); // 0x00727B10 (:1529)
    void restore_link(rfg_link* l); // 0x00727B60 (:1549)
    unsigned int get_dlod_index_from_name(const char* name); // 0x00727F90 (:1621)
    bool dlod_damaged_this_frame(int dlod_index); // 0x007176F0 (:1638)
    float dlod_percent_damaged(int dlod_index); // 0x00717710 (:1644)
    void recalc_shape_properties(); // 0x00727FE0 (:1669)
    void recalc_rbb_tree(); // 0x0073F4C0 (:1745)
    void get_tensor(matrix& tensor); // 0x00717750 (:1652)
    void add_subpiece_to_shardify_q(rfg_subpiece* sp); // 0x00720050 (:1841)
    void remove_subpiece_from_shardify_q(rfg_subpiece* sp); // 0x00720240 (:1887)
    rfg_subpiece* get_object_buffer() const; // 0x005DB4A0 (rfg_shapes.inl:475)
    void delete_me(); // 0x00736F40 (:1923)
    void querySphere(const vector& pos, const float radius, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // 0x00730190 (:2039)
    virtual void querySphere(const hkSphere& sphere, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // vtbl+0x54 0x00730150 (:2026)
    void queryAabb(const vector& bmin, const vector& bmax, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // 0x00730210 (:2057)
    virtual unsigned int queryAabb(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const; // vtbl+0x58 0x007375E0 (rfg_shapes_spu.cpp:24)
    virtual void queryAabb(const hkAabb& aabb, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // vtbl+0x48 0x007301C0 (:2045)
    void queryObb(const vector& pos, const matrix& orient, const vector& extents, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // 0x00730290 (:2081)
    virtual void queryObb(const hkTransformf& obbTransform, const hkVector4f& obbExtent, float tolerance, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const; // vtbl+0x5C 0x00730230 (:2063)
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x007203F0 (rfg_shapes_spu.cpp:43)
    virtual unsigned int queryAabbImpl(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const; // vtbl+0x50 0x00717E50 (rfg_shapes_spu.cpp:38)
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00728250 (:2088)
    virtual void castRayWithCollector(const hkpShapeRayCastInput& in, const hkpCdBody& body, hkpRayHitCollector& out) const; // vtbl+0x1C 0x00728290 (:2094)
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00736A10 (.h:664)
    virtual const hkpShape* getChildShapeImpl(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x60 0x007283B0 (rfg_shapes_spu.cpp:58)
    virtual const hkpShape* getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x14 0x007178D0 (:2123)
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x00736A20 (rfg_shapes.inl:330)
    virtual unsigned int getNextKey(unsigned int oldKey) const; // vtbl+0x0C 0x00736A40 (rfg_shapes.inl:346)
    virtual int getNumChildShapes() const; // vtbl+0x04 0x00736A60 (rfg_shapes.inl:371)
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x007282B0 (:2099)
private:
    void release_rbb_tree(); // 0x00728210 (:1906)
public:
    virtual ~rfg_destroyable_shape(); // vtbl+0x00 0x00736A70

private:
    vector center_of_mass_accum; // 0x18
    friend vector pick_building_target_pos(rfg_mover* building); // patrol_actions.cpp walks the child list (inferred)
    rfg_subpiece* objects; // 0x24
    rfg_link* links; // 0x28
    rfg_dlod* dlods; // 0x2C
    rfg_mover* mover_p; // 0x30
public:
    const rfg_rbb_node* aabb_tree; // 0x34
    rfg_rbb_node* override_tree; // 0x38
    rfg_base_destroyable_shape* parent; // 0x3C
    unsigned short child_obj_list; // 0x40
    unsigned short transfer_child_obj_list; // 0x42
    unsigned short shardify_q; // 0x44
    unsigned short anchor_depth; // 0x46
    int num_existing_child_shapes; // 0x48
    int calc_rbb_child_count; // 0x4C
    float structural_mass; // 0x50
    float base_structural_mass; // 0x54
    int flags; // 0x58
    vector center_of_mass; // 0x5C
    vector local_bmin; // 0x68
    vector local_bmax; // 0x74
    float mass; // 0x80
    unsigned int shape_uid; // 0x84
    rfg_destroyable_shape* sib_next; // 0x88
    rfg_destroyable_shape* sib_prev; // 0x8C
    timestamp destruction_mode_timer; // 0x90
};

// size 0x14
struct rfg_dynamic_link
{
    void reset(); // 0x00717620 (:975)
    void copy(const rfg_dynamic_link* rdl); // 0x00717640 (:988)
    bool should_save(unsigned int handle) const; // 0x00717660 (:1006)

    unsigned short shape_key[2]; // 0x00
    unsigned short anchor_depth[2]; // 0x04
    unsigned int mover_handle[2]; // 0x08
    int yield_limit; // 0x10
};

extern data_hash_table<unsigned int,rfg_destroyable_shape*,&hash_uint,9216,unsigned short> Destroyable_uid_hash; // 0x01EF8310
extern float Use_parent_rbb_pct; // 0x0165047C

void rfg_shape_system_init(); // 0x007202F0 (:2147)
void rfg_shape_level_init_pre(); // 0x007178E0 (:2164)
void rfg_shape_level_shutdown(); // 0x007178F0 (:2182)
void rfg_shape_set_thread_ownership(); // 0x00717900 (:2200)
int rfg_shape_calc_link_strength(unsigned char mat_0_index, rfg_subpiece_base* sp_0, unsigned char mat_1_index, rfg_subpiece_base* sp_1, float yield_multiplier); // 0x00720320 (:2205)
unsigned int rfg_shape_get_shape_uid_from_object(const object* objp); // 0x00717910 (:2237)
rfg_destroyable_shape* rfg_shape_get_shape_from_uid(const unsigned int shape_uid); // 0x00728370 (:2252)
rfg_destroyable_shape* rfg_shape_new(rfg_base_destroyable_shape* base, unsigned int chunk_uid, const vector& pos, const matrix& orient, unsigned int sibling_uid); // 0x0073CD70 (:2457)
void rfg_shape_delete(rfg_destroyable_shape* shape); // 0x00737220 (:2484)
float rfg_shape_calc_overlap_area(const vector& pos_0, const vector& bmin_0, const vector& bmax_0, const vector& pos_1, const vector& bmin_1, const vector& bmax_1); // 0x00717930 (:2862)
void rfg_shape_shard_impulse_info_clear(); // 0x00710970 (rfg_shapes.inl:17)
shard_impulse_info* rfg_shape_get_shard_impulse_info(rfg_subpiece* sp); // 0x00710980 (rfg_shapes.inl:23)
rfg_base_destroyable_shape* rfg_base_destroyable_shape_load(unsigned char* data, int& offset, c_chunk_base* chunk_base); // 0x00737240 (rfg_shapes_load.cpp:58)
void rfg_shape_delete_base(rfg_base_destroyable_shape* base); // 0x007283A0 (rfg_shapes_load.cpp:186)
