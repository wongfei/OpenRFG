// Original: volition/render_lib/code/rl_visibility_manager.h
// Implementation: volition/render_lib/code/rl_visibility_manager.cpp
//
// rl_visibility_manager, runs the visibility passes of a scene: walks the partition with each pass frustum, occluders
// and LOD rules and fills the visible lists (vis_queue_entry). Placement of vis_queue_entry inferred.
#pragma once

#include <stdint.h>
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_frustum.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class rl_3d_entity;
class rl_bvolume;
class rl_camera;
class rl_class_dynamic_info;
class rl_light;
class rl_occluder;
class rl_partition_sector;
class rl_renderer;
class rl_scene;
class rl_scene_entity;
class vector2;

namespace keen
{
    struct AxisAlignedRectangle;
}

// size 0x1D3D0
class rl_visibility_manager : public rl_base_object
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
public:
    enum visibility_lists
    {
        LIST_0 = 0,
        LIST_1 = 1,
        LIST_2 = 2,
        LIST_3 = 3,
        LIST_4 = 4,
        LIST_5 = 5,
        LIST_6 = 6,
        LIST_7 = 7,
        VISIBILITY_LIST_COUNT = 8,
    };

    // size 0x14
    struct visible_sector_entry
    {
        rl_partition_sector* sector_p; // 0x00
        unsigned int pass_fully_visible_flags; // 0x04
        unsigned int occluder_use_flags; // 0x08
        unsigned int pass_visibility_flags; // 0x0C
        unsigned int pass_occlusion_flags; // 0x10
    };

    // size 0x240
    struct visibility_pass_info
    {
        visibility_pass_info(); // 0x00504FD0
        ~visibility_pass_info(); // 0x004D6220

        unsigned int pass_index; // 0x00
        unsigned int pass_index_shifted; // 0x04
        unsigned int dependent_pass_index; // 0x08
        unsigned int vis_pass_flags; // 0x0C
        rl_sphere bounding_sphere; // 0x10
        unsigned int se_culling_flags; // 0x20
        unsigned int ri_pass_culling_flags; // 0x24
        rl_frustum culling_frustum; // 0x30
    };

    // size 0x1
    class Debug
    {
    public:
        // size 0x1C
        struct Context
        {
            Context(); // 0x004D4BA0

            visibility_pass_info* pPass; // 0x00
            vector mainCameraPosition; // 0x04
            rl_view_frustum* pDefaultViewFrustum; // 0x10
            rl_view_frustum* pLowLodViewFrustum; // 0x14
            rl_view_frustum* pLandmarkViewFrustum; // 0x18
        };

        static void clipToRect(keen::AxisAlignedRectangle& innerRectangle, const keen::AxisAlignedRectangle& outerRectangle); // 0x004D4820 (:63)
        static void begin(const Context& context); // 0x004D4940 (:78)
        static int debugTransform(float value, float worldToCamera, float cameraToScreen, float screenOffset, float cameraOffset); // 0x004D4AB0 (:121)
        static void drawVisibilityLists(base_array<pool_list<rl_scene_entity*>*>& lists); // 0x005349A0 (:126)
        static void drawVisibilityList(pool_list<rl_scene_entity*>& list, unsigned int color); // 0x0051C8A0 (:151)
        static void drawVisibleSectors(base_array<visible_sector_entry>& sectors); // 0x0051C8E0 (:182)
        static void drawOccluders(base_array<rl_occluder*>& occluders); // 0x0051C910 (:224)
        static void drawFrustum(const vector* corners, unsigned int frustumColor); // 0x004D4B00 (:242)
        static void drawGenericFrustum(const rl_frustum& frustum, unsigned int frustumColor); // 0x004D4B10 (:333)
        static void drawViewFrustum(const rl_view_frustum& frustum, unsigned int frustumColor); // 0x004D4B30 (:371)
        static void end(); // 0x004D4B90 (:401)

        static Context ms_context; // 0x01C226AC
        static vector2 ms_screenOffset; // 0x01C20BC4
        static vector ms_cameraOffset; // 0x01C27390
        static float ms_worldToCamera; // 0x01B6AAF0
        static float ms_cameraToScreen; // 0x01B6EAF8
    };

protected:
    rl_visibility_manager(); // 0x00534B70 (:453)
    virtual ~rl_visibility_manager(); // vtbl+0x00 0x0051C940 (:464)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_visibility_manager* create(); // 0x00547360 (:39)
    static bool can_create(); // 0x004D47F0 (:39)
    virtual void release(); // vtbl+0x04 0x004D4800 (:39)
    void init(); // 0x00534D60 (:469)
    void perform_visibility_pass(rl_renderer& renderer, rl_camera& camera, rl_scene* scene_p); // 0x00561370 (:1033)
    void set_view_frustum(const rl_view_frustum& view_frustum); // 0x004BCDB0 (.h:220)
    void set_low_lod_view_frustum(const rl_view_frustum& view_frustum); // 0x004BCDD0 (.h:225)
    void set_landmark_lod_view_frustum(const rl_view_frustum& view_frustum); // 0x004BCDF0 (.h:230)
    void set_viewer(rl_3d_entity* viewer_p); // 0x004BCE10 (.h:235)
    void set_lod_off_enabled(bool enabled); // 0x007C1EB0 (.h:240)
    void set_visibility_list(visibility_lists index, pool_list<rl_scene_entity*>* visibility_list_p); // 0x004E3970 (.h:245)
    void set_visible_light_list(pool_list<rl_light*>* visibility_list_p); // 0x004BCE20 (.h:250)
    const pool_list<rl_occluder*>& get_potential_occluders() const;
    const pool_list<rl_occluder*>& get_actual_occluders() const;
    unsigned int get_num_entities_in_view_frustum() const;
    unsigned int get_num_entities_occluded() const;
    unsigned int get_num_entities_lod_offed() const;
    unsigned int get_base_pass_vis_flags(); // 0x004BCCA0 (.h:87)
    unsigned int get_clb_near_vis_flags(); // 0x004BCCB0 (.h:88)
    unsigned int get_clb_mid_vis_flags(); // 0x004BCCC0 (.h:89)
    unsigned int get_clb_far_vis_flags(); // 0x004BCCD0 (.h:90)
    unsigned int get_clb_terrain_vis_flags(); // 0x004BCCF0 (.h:91)
    unsigned int get_drop_shadow_vis_flags(); // 0x004BCD10 (.h:92)
    unsigned int get_distortion_vis_flags(); // 0x004BCD20 (.h:93)
    unsigned int get_base_shadow_vis_flags();
    unsigned int get_outline_vis_flags(int pass); // 0x004BCD30 (.h:95)
    unsigned int get_xray_pass_vis_flags(); // 0x004BCD50 (.h:96)
    unsigned int get_xray_terrain_vis_flags(); // 0x004BCD60 (.h:97)
    unsigned int get_non_shadow_vis_flags(); // 0x004BCD70 (.h:99)
    unsigned int get_shadow_vis_flags(); // 0x004BCD80 (.h:100)
    float get_shadow_min_y(); // 0x004BCD90 (.h:102)
    float get_shadow_max_y(); // 0x004BCDA0 (.h:103)
private:
    void perform_sector_visibility(rl_renderer& renderer, rl_camera& camera, rl_partition_sector* sector_p, unsigned int pass_fully_visible_flags, unsigned int pass_use_flags, unsigned int parent_occluder_use_flags); // 0x0051CD40 (:854)
    void perform_sector_visibility_threaded(rl_partition_sector* sector_p, unsigned int pass_fully_visible_flags, unsigned int pass_use_flags, unsigned int parent_occluder_use_flags); // 0x0051D220 (:966)
    void select_occluders(rl_scene* scene_p); // 0x0055DD50 (:1345)
    void determine_visible_lights(rl_renderer& renderer, rl_camera& camera, rl_scene* scene_p); // 0x0051D340 (:1451)
    void determine_visible_low_lod(rl_renderer& renderer, rl_camera& camera, rl_scene* scene_p); // 0x005473A0 (:1620)
    void determine_visible_landmark_lod(rl_renderer& renderer, rl_camera& camera, rl_scene* scene_p); // 0x0051D8F0 (:1540)
    void create_visibility_passes(rl_scene* scene_p, rl_camera& camera); // 0x00534DE0 (:1716)
    bool bounding_volume_is_visible_in_frustum(const rl_bvolume& bvolume) const; // 0x0051C9D0 (:477)
    bool entity_is_visible_in_frustum(rl_scene_entity* scene_entity_p) const; // 0x0051C9F0 (:491)
    bool sector_is_visible_in_frustum(rl_partition_sector*) const;
    bool entity_is_visible_in_occlusion(rl_scene_entity* scene_entity_p, unsigned int occluder_use_flags) const; // 0x00501E60 (:2009)
    bool sector_is_visible_in_occlusion(rl_partition_sector* sector_p, unsigned int& occluder_use_flags) const; // 0x00501EC0 (:2026)
    unsigned int bounding_volume_is_visible_for_passes(const rl_bvolume& bvolume, unsigned int se_flags, unsigned int ri_pass_flags, unsigned int passes_to_test, unsigned int already_visible_passes) const; // 0x0051CA20 (:552)
    unsigned int sector_is_visible_for_passes(rl_partition_sector* sector_p, unsigned int passes_to_test, unsigned int already_visible_passes) const; // 0x0051CB00 (:619)
    unsigned int sector_is_occluded_for_passes(rl_partition_sector* sector_p, unsigned int passes_to_test, unsigned int& occluder_use_flags) const; // 0x00501B30 (:631)
    unsigned int sector_is_fully_visible_for_passes(rl_partition_sector* sector_p, unsigned int passes_to_test) const; // 0x00501C50 (:678)
    unsigned int entity_is_visible_for_passes(rl_scene_entity* scene_entity_p, unsigned int passes_to_test, unsigned int already_visible_passes) const; // 0x0051CB20 (:757)
    unsigned int entity_is_occluded_for_passes(rl_renderer& renderer, rl_camera& camera, rl_scene_entity* scene_entity_p, unsigned int passes_to_test, unsigned int occluder_use_flags) const; // 0x0051CB60 (:773)
    static bool sector_sort_predicate(const visible_sector_entry&, const visible_sector_entry&);
    // Not in the binary: loop bodies the original inlines in several functions (rl_visibility_manager.cpp).
    void test_collected_entity(rl_renderer& renderer, rl_camera& camera, const visible_sector_entry& sector, rl_scene_entity* entity, unsigned int& visible, unsigned int& occluded) const;
    void add_collected_entities(rl_renderer& renderer, rl_camera& camera);

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01516400
private:
    rl_view_frustum m_view_frustum; // 0x10
    rl_view_frustum m_low_lod_view_frustum; // 0x230
    rl_view_frustum m_landmark_lod_view_frustum; // 0x450
    rl_3d_entity* m_viewer_p; // 0x670
    bool m_lod_off_enabled; // 0x674
    pool_list<rl_occluder*> m_actual_occluders; // 0x678
    pool_list<rl_occluder*> m_potential_occluders; // 0x690
    rl_occluder* m_merged_occluder_p; // 0x6A8
    int m_last_visibility_pass; // 0x6AC
    uint64_t m_render_category_filter; // 0x6B0
    unsigned int m_num_entities_in_frustum; // 0x6B8
    unsigned int m_num_entities_occluded; // 0x6BC
    unsigned int m_num_entities_lod_offed; // 0x6C0
    farray<visibility_pass_info,31> m_visibility_passes; // 0x6D0
    farray<visible_sector_entry,5000> m_visible_sectors; // 0x4CA0
    farray<pool_list<rl_scene_entity*>*,8> m_visibility_lists; // 0x1D350
    pool_list<rl_light*>* m_visible_light_list; // 0x1D380
    unsigned int m_base_pass_vis_index; // 0x1D384
    unsigned int m_clb_base_vis_index; // 0x1D388
    unsigned int m_drop_shadow_vis_index; // 0x1D38C
    unsigned int m_distortion_vis_index; // 0x1D390
    unsigned int m_base_shadow_vis_index; // 0x1D394
    unsigned int m_outline_vis_index[4]; // 0x1D398
    unsigned int m_xray_pass_vis_index; // 0x1D3A8
    unsigned int m_xray_terrain_vis_index; // 0x1D3AC
    unsigned int m_non_shadow_vis_flags; // 0x1D3B0
    unsigned int m_shadow_vis_flags; // 0x1D3B4
    unsigned int m_shadow_min_max_flags; // 0x1D3B8
    float m_shadow_min_y; // 0x1D3BC
    float m_shadow_max_y; // 0x1D3C0
};

// size 0x58
struct vis_queue_entry
{
    vis_queue_entry(); // 0x005075C0

    int m_pixels_drawn; // 0x00
    rl_scene_entity* m_entity; // 0x04
    unsigned int m_flags : 2; // 0x08:0
    unsigned char m_query_index; // 0x0C
    char m_delay; // 0x0D
    rl_stamped_handle m_query_handle[3]; // 0x10
    rl_aabb m_eye_check_aabb; // 0x28
    vector m_offset; // 0x40
    vector m_scale; // 0x4C
};

// 0x005075C0
// Only the query handles are initialized (to invalid, by their constructors).
inline vis_queue_entry::vis_queue_entry()
{
}
