// Original: volition/rfg/code/vehicle/vehicle_class.cpp
//
// The vehicle object: its lights, seats, damage, the fire and corpse states, tags, the stream callback and the two
// per-frame passes. Compiled in the vehicle unity TU: the statics below are shared with its other files.

#include "volition/rfg/code/vehicle/vehicle_class.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_vehicle_engine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/render_lib/code/rl_deformable_mesh_instance.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/geom/mesh_deform.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_filter_shape_collection.h"
#include "volition/rfg/code/interface/ui_document_hud_satellite.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_respawn.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/spawn/spawn_placement.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_cover.h"
#include "volition/rfg/code/vehicle/vehicle_effects.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_subinfo.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/rfg/code/vehicle_ai/intersection_controller.h"
#include "volition/rfg/code/havok/havok_game_filter.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/render_lib/code/rl_morph_instance.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpaabbphantom.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/cutscene/cutscene_internal.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/util/bitarray.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/rfg/code/gameplay/activities/activity_car_bomb.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/havok/havok_raycast_collectors.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_deformation.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_render.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/math/spline.h"

// ---- class statics
const int vehicle::c_lod_distance_max = 4; // 0x012C70AC
lod_info vehicle::ms_lod_distances[4] = {{10.0f}, {20.0f}, {40.0f}, {100.0f}}; // 0x016BA4B8

// ---- file-static globals
static const float LOW_SWERVE_DISTANCE = 8.0f; // 0x01336B5C
static const float HIGH_SWERVE_DISTANCE = 20.0f; // 0x01336CDC
static const float VEHICLE_DESTROY_COVER_DIST_SQR = 0.0625f; // 0x01336CE0
static float Damage_multiplier_npc_vehicle = 0.1f; // 0x016BA4C8
static tweak_table_item Tweak_table_item_Damage_multiplier_npc_vehicle("Damage_multiplier_npc_vehicle", &Damage_multiplier_npc_vehicle); // 0x037700FC
static float Vehicle_vs_building_damage_multiplier = 1.0f; // 0x016BA4CC
static float Vehicle_vs_building_max_damage_per_second = 300.0f; // 0x016BA4D0
static bool Vehicle_interpolate_havok; // 0x03770030
static float Vehicle_player_damage_factor; // 0x0376F634, from the tweak table
static tweak_table_item Tweak_table_item_Vehicle_player_damage_factor("Vehicle_player_damage_factor", &Vehicle_player_damage_factor);
static float flyer_explosion_linear_impulse_scalar; // 0x0376F668, from the tweak table
static float flyer_explosion_angular_impulse_scalar; // 0x0376F7AC, from the tweak table
static tweak_table_item Tweak_table_item_flyer_explosion_linear_impulse_scalar("flyer_explosion_linear_impulse_scalar", &flyer_explosion_linear_impulse_scalar);
static tweak_table_item Tweak_table_item_flyer_explosion_angular_impulse_scalar("flyer_explosion_angular_impulse_scalar", &flyer_explosion_angular_impulse_scalar);
extern bool Vehicle_system_avoid_despawn; // 0x03770045 (vehicle_system.cpp, later in the unity TU)
extern const vehicle_info* Nav_cell_detour_veh_info_mar_raider_1; // 0x035B6080
static float Vehicle_body_friction = 0.03f; // 0x016BA4D4
static float vertical_max_speed = 15.0f; // 0x016EB694
static bool Processed_this_frame; // 0x03770031
static bool Checked_this_frame; // 0x03770032
static int Num_vehicles_checking_cover; // 0x0377003C
static int Num_vehicles_checking_cover_last_frame; // 0x03770040
static unsigned char Vehicle_physical_material_index = 0xFF; // 0x016BA48A
static unsigned char Vehicle_effect_material_index = 0xFF; // 0x016BA48B
// Dynamic initializer in rfg.exe: a vehicle leaning under 20 degrees still gives cover.
static const float MIN_VEHICLE_COVER_ORIENT_VERTICAL = (float)cos(0.3490599989891052); // 0x03770B78
static float RFG_vehicle_energy_scale = 1.0f; // 0x016BA4D8
static int Vehicle_headlights_time_on = 18; // 0x016BA4FC
static int Vehicle_headlights_time_off = 6; // 0x016BA500
static float Vehicle_emergency_lights_blink_time = 0.5f; // 0x016BA508

// vehicle_component::get_flags() bits (rfg.pdb has no names; meaning from their uses)
static const unsigned int VCF_SHAPE_IN_PARENT = 0x20;
static const unsigned int VCF_GLASS = 0x40;
static const unsigned int VCF_NAV_DETOUR = 0x200;
static const unsigned int VCF_UNLATCHED = 0x10000;
static const unsigned int VCF_DETACHED = 0x20000;
static const unsigned int VCF_ACTIVE_IN_CHASSIS_MESH = 0x40000;
static const unsigned int VCF_NOT_RENDERED = 0x80000;
static const unsigned int VCF_NO_CHASSIS_MESH = 0x200000;

// Object types in the handle lookups below.
static human* vehicle_human_from_handle(unsigned int handle)
{
    return handle != 0xFFFFFFFF ? World.object_from_handle<human>(handle) : NULL;
}

static object_effect* vehicle_effect_from_handle(unsigned int handle)
{
    return handle != 0xFFFFFFFF ? World.object_from_handle<object_effect>(handle) : NULL;
}

// A vehicle from a handle; rfg.exe's checks inlined everywhere. Original bug kept by the callers that do not test it.
static vehicle* vehicle_from_handle(unsigned int v_handle)
{
    return v_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(v_handle) : NULL;
}

// Inlined in vehicle::set_infinite_mass and vehicle::check_for_vehicle_intersection: the Havok instance has the flag too.
static void automobile_set_havok_infinite_mass(vehicle* vp, bool set)
{
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO && ((automobile*)vp)->a_rdd.is_loaded()) {
        ((automobile*)vp)->a_rdd.get_data()->m_hk_vehicle_instance->set_is_infinite_mass(set);
    }
}

// Inlined in vehicle::clean_up_debris: only debris still belonging to this vehicle goes.
static void vehicle_clean_up_debris_object(vehicle* vp, unsigned int& handle)
{
    if (handle != 0xFFFFFFFF) {
        object_debris* debris = World.object_from_handle<object_debris>(handle);
        if (debris != NULL && debris->m_create_info.m_vehicle_handle == vp->get_handle()) {
            World.destroy_object(debris, false);
        }
        handle = 0xFFFFFFFF;
    }
}

// Compiler generated in rfg.exe, as are the constructors below (the members do the work).
// 0x00B9C110
vehicle::~vehicle()
{
}

// 0x00B64680
vehicle_antigravity::~vehicle_antigravity()
{
}

// 0x00B76850
vehicle::resource_dependent_data::resource_dependent_data()
{
}

// 0x00B645D0
vehicle::vehicle_navcell_info::vehicle_navcell_info()
{
}

// 0x00B645C0
vehicle_damage_event::vehicle_damage_event()
{
}

// 0x00B62E50 (global_event_data.h:95)
vehicle_destroyed_event::vehicle_destroyed_event()
    : event_data(EVENT_VEHICLE_DESTROYED)
{
}

// 0x00B62E60 (global_event_data.h:182)
player_kills_vehicle_event::player_kills_vehicle_event()
    : event_data(EVENT_PLAYER_KILLS_VEHICLE)
{
}

// 0x00B62E70 (global_event_data.h:201)
player_kills_vehicle_from_vehicle_event::player_kills_vehicle_from_vehicle_event()
    : event_data(EVENT_PLAYER_KILLS_VEHICLE_FROM_VEHICLE)
{
}

// 0x00B62E80 (global_event_data.h:207)
vehicle_takes_damage_event::vehicle_takes_damage_event()
    : event_data(EVENT_VEHICLE_DAMAGE)
{
}

// 0x00B64590 (:199)
void vehicle_light::reset()
{
    flags.is_on = false;
    flags.dirty = false;
    flags.broken = false;
    flags.is_broken = false;
    tag_index = -1;
    type = VLT_INVALID;
    parent_idx = -1;
    mesh_light = NULL;
    mesh_index_off = -1;
    mesh_index_on = -1;
    component_mesh_index_off = -1;
    component_mesh_index_on = -1;
    coronas[0] = 0xFFFFFFFF;
    coronas[1] = 0xFFFFFFFF;
    coronas[2] = 0xFFFFFFFF;
    corona_effect_info_handle = 0xFFFFFFFF;
}

// 0x00B76670 (:227)
void vehicle_light::start_coronas(unsigned int info_one, unsigned int info_two, vehicle* vp)
{
    corona_effect_info_handle = info_one;
    if (info_one == 0xFFFFFFFF || mesh_light == NULL) {
        return;
    }
    int num_tags = mesh_light->on_mesh_tags.get_num_tags();
    if (3 < num_tags) {
        num_tags = 3;
    }
    int tag_data_offset = 0;
    const mesh_tag* tag = mesh_light->on_mesh_tags.get_tags(tag_data_offset);
    const unsigned int secondary_glare = crc_stri("HeadlightSecondary_Glar", 0);
    const vehicle::resource_dependent_data* rdd = vp->rdd.is_loaded() ? vp->rdd.get_data() : NULL;
    for (int i = 0; i < num_tags; i++, tag++) {
        if (coronas[i] != 0xFFFFFFFF) {
            continue;
        }
        const unsigned int info = (type == VLT_HEAD_LIGHT && tag->m_name_crc == secondary_glare) ? info_two : info_one;
        // a light tag: 0x05 in the high byte, then the light's index, then the tag within it
        const int light_index = (int)(this - rdd->m_vehicle_lights);
        const int tag_index_on_light = ((light_index | 0x500) << 16) | i;
        vector pos;
        matrix orient;
        vp->get_tag_world_coords(tag_index_on_light, pos, &orient, NULL, NULL);
        coronas[i] = effects_play(info, vp->get_handle(), tag_index_on_light, true, 0xFFFFFFFF, 1.0f, false);
    }
}

// 0x00B767D0 (:278)
void vehicle_light::stop_all_effects()
{
    object_effect* effect = vehicle_effect_from_handle(effect_handle);
    effect_handle = 0xFFFFFFFF;
    if (effect != NULL) {
        effect->stop(object_effect::SS_STOP_EMITTING);
    }
    for (int i = 0; i < 3; i++) {
        effect = vehicle_effect_from_handle(coronas[i]);
        coronas[i] = 0xFFFFFFFF;
        if (effect != NULL) {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
    }
}

// 0x00B96F40 (:440)
void unused_dcf_vehicle_bomb()
{
    if (Console_command == 0) {
        return;
    }
    const unsigned int handle = vehicle_get_closest_vehicle(player_get_local_player()->pos, -1.0f, NULL, false);
    vehicle* vp = handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(handle) : NULL;
    if (vp != NULL) {
        vp->m_bomb_status = VEHICLE_BOMB_ARMED;
        activity_car_bomb_start(NULL);
        console_output("Car bomb attached to the nearest vehicle\n", NULL, false);
        return;
    }
    console_output("No vehicles found\n", NULL, false);
}

// 0x00B9D9A0 (:536)
vehicle::vehicle()
    : damage_events_cs(0)
    , component_rinstance_cs(0)
{
    engine_inst = -1;
    engine_started_inst = -1;
}

// 0x00B64640 (:558)
vehicle_antigravity::vehicle_antigravity(hkpRigidBody* rb, const vector& acceleration)
    : hkpUnaryAction(rb, 0)
{
    acceleration_due_to_force.set(acceleration.x, acceleration.y, acceleration.z, 0.0f);
}

// 0x00B768E0 (:548)
hkpAction* vehicle_antigravity::clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& entities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantoms) const
{
    vector acceleration;
    havok_to_game_vector(acceleration, acceleration_due_to_force);
    return new vehicle_antigravity((hkpRigidBody*)entities[0], acceleration);
}

// 0x00B769B0 (:563)
void vehicle_antigravity::applyAction(const hkStepInfo& stepInfo)
{
    hkpEntity* entity = m_entity;
    hkVector4f impulse = acceleration_due_to_force;
    impulse.mul(entity->getMotion()->getMass());
    entity->activate();
    entity->getMotion()->applyForce(stepInfo.m_deltaTime, impulse);
}

// 0x00B76A50 (:584) (LTCG: vp in EAX)
static int find_closest_vehicle_component(vehicle* vp, const vector& pos)
{
    if (vp == NULL) {
        return -1;
    }
    const vehicle::resource_dependent_data* rdd = vp->rdd.is_loaded() ? vp->rdd.get_data() : NULL;
    for (unsigned int i = 0; i < rdd->m_num_components; i++) {
        const vehicle_component& component = rdd->m_components[i];
        if ((component.get_flags() & (VCF_DETACHED | VCF_NOT_RENDERED)) != 0) {
            continue;
        }
        vector local_pos(pos.x - component.get_chassis_space_position().x, pos.y - component.get_chassis_space_position().y, pos.z - component.get_chassis_space_position().z);
        local_pos.rotate(component.get_chassis_space_rotation());
        vector bmin;
        vector bmax;
        ((rl_mesh*)rdd->sub_info->m_vehicle_mesh)->get_subpiece_local_bb(component.get_data()->m_submesh_id, bmin, bmax);
        vector closest;
        if (bbox_find_closest_point(closest, bmin, bmax, local_pos) < 0.1f) {
            return (int)i;
        }
    }
    return -1;
}

// 0x00BB3700 (:997)
void vehicle_update_passengers_pos_orient(vehicle* vp)
{
    if (vp->info->vi_info == NULL || vp->obj_flags.destroyed) {
        return;
    }
    int remaining = vp->vii.num_humans;
    for (int i = 0; 0 < remaining && i < 8; i++) {
        if (vp->vii.humans[i].handle != 0xFFFFFFFF) {
            remaining--;
            vi_update_human_pos(&vp->vii, i);
        }
    }
}

// The lights of one tag name; continues the vehicle's list. Original quirk: 20 lights at most.
static int vehicle_init_light_tags(vehicle* vp, vehicle::resource_dependent_data* rdd, const char* tag_name, vehicle_light_type type, int count)
{
    int tag = vp->get_tag_index_from_name(tag_name);
    int nth = 1;
    while (tag != -1 && count < 20) {
        rdd->m_vehicle_lights[count].tag_index = tag;
        rdd->m_vehicle_lights[count].type = type;
        count++;
        nth++;
        tag = vp->get_nth_tag_index_from_name(tag_name, nth);
    }
    return count;
}

// 0x00B8B350 (:1008) (LTCG: vp in EDI)
static void vehicle_init_tag_ids(vehicle* vp)
{
    vehicle::resource_dependent_data* rdd = vp->rdd.is_loaded() ? vp->rdd.get_data() : NULL;
    const int type = vehicle_info_get_type(vp->info);
    for (int i = 0; i < 4; i++) {
        vehicle_init_exhaust_tag(type, i, vp->get_nth_tag_index_from_name("exhaust", i + 1));
    }
    int count = vehicle_init_light_tags(vp, rdd, "Taillight_Point_Glare", VLT_TAIL_LIGHT, 0);
    count = vehicle_init_light_tags(vp, rdd, "Brakelight_Point_Glare", VLT_BRAKE_LIGHT, count);
    count = vehicle_init_light_tags(vp, rdd, "Reverselight_Point_Glare", VLT_REVERSE_LIGHT, count);
    rdd->m_num_vehicle_lights = count;
}

// 0x00B64690 (:1104) (LTCG: vp in ECX)
static void vehicle_init_seat_info(vehicle* vp)
{
    for (int i = 0; i < 11; i++) {
        vp->m_seat_info[i].m_current_occupant_handle = 0xFFFFFFFF;
    }
}

// 0x00BB3710 (:1172)
static void vehicle_kill_passengers(vehicle* vp, human* attacker, damaged_by_types dbt, const weapon_info* orig_weapon)
{
    int remaining = vp->vii.num_humans;
    for (int i = 0; 0 < remaining && i < 8; i++) {
        const unsigned int handle = vp->vii.humans[i].handle;
        if (handle == 0xFFFFFFFF) {
            continue;
        }
        remaining--;
        human* hp = World.object_from_handle<human>(handle);
        if (hp == NULL) {
            continue;
        }
        if (hp->get_sub_type() == OT_SUB_HUMAN_NPC) {
            ((npc*)hp)->npc_flags.tied_up = 0;
        }
        bool ragdoll = false;
        if (hp->is_invulnerable()) {
            vp->m_flags.no_ragdoll = 0;
            ragdoll = true;
        }
        else if (!hp->char_inst->rdd.is_loaded()) {
            World.destroy_object(hp, false);
            vp->vii.remove_human(i);
        }
        else {
            human_damage(hp, attacker, -1.0f, dbt, orig_weapon, NULL, true, false);
            ragdoll = !hp->is_killed();
        }
        if (ragdoll) {
            vehicle_interaction_ragdoll(hp, 0xFFFFFFFF);
        }
        if (object_is_local_player(hp)) {
            if (Rfg_camera.mode == CAMERA_TURRET_MODE) {
                camera_stop_turret_mode(true);
            }
            camera_free_set_target(hp);
        }
    }
}

// 0x00BA8850 (:1222)
void vehicle_turret_turn_off_components(vehicle* vp, vehicle_component* vcp)
{
    vehicle::resource_dependent_data* rdd = vp->rdd.is_loaded() ? vp->rdd.get_data() : NULL;
    const vehicle_component_data* data = vcp->get_data();
    for (unsigned int i = 0; i < data->m_num_children; i++) {
        vehicle_turret_turn_off_components(vp, &rdd->m_components[data->m_children_indices[i]]);
    }
    if ((vcp->get_flags() & VCF_SHAPE_IN_PARENT) != 0) {
        const unsigned int num_pieces = data->m_parent_num_sub_pieces;
        hkArray<unsigned int,hkContainerHeapAllocator> filters;
        // walk up to the body that holds this component's shapes, summing the piece offsets
        int start = 0;
        vehicle_component* cur = vcp;
        hkpEntity* entity = NULL;
        while ((cur->get_flags() & VCF_SHAPE_IN_PARENT) != 0 && (cur->get_flags() & (VCF_DETACHED | VCF_NOT_RENDERED)) == 0) {
            const unsigned int parent = cur->get_data()->m_parent_index;
            entity = havok_body_get_pointer(parent == 0xFFFFFFFF ? vp->havok_handle : rdd->m_components[parent].havok_handle);
            // Deviation from rfg.exe: with no body at parent -1 it goes on and reads the component before the first; the port stops.
            if (entity != NULL || parent == 0xFFFFFFFF) {
                break;
            }
            start += cur->get_data()->m_parent_sub_piece_start_index;
            cur = &rdd->m_components[parent];
        }
        if (entity != NULL) {
            havok_filter_shape_collection* collection = havok_filter_shape_collection_get_from_rigid_body((hkpRigidBody*)entity);
            if (collection != NULL) {
                start += cur->get_data()->m_parent_sub_piece_start_index;
                collection->get_collision_filter(filters, start, num_pieces);
                bool all_off = true;
                for (int i = 0; i < (int)num_pieces; i++) {
                    all_off = all_off && filters[i] == COLLIDE_NO_COLLISION;
                }
                if (0 < (int)num_pieces && !all_off) {
                    collection->set_collision_filter(start, num_pieces, COLLIDE_NO_COLLISION);
                    havok_entity_update_collision_filter(entity, true, true);
                }
            }
        }
    }
    vcp->set_flags(VCF_DETACHED | VCF_NO_CHASSIS_MESH);
}

// 0x00BA8A70 (:1246)
void vehicle_build_turrets(vehicle* vp)
{
    vehicle::resource_dependent_data* rdd = vp->rdd.is_loaded() ? vp->rdd.get_data() : NULL;
    // the mesh's turret components, and whether a mount below uses each. Original bug kept: 6 at most, no bound.
    unsigned int turret_components[6];
    int component_used[6];
    unsigned int num_turret_components = 0;
    for (unsigned int i = 0; i < rdd->m_num_components; i++) {
        if (rdd->m_components[i].get_data()->m_type == VCT_TURRET) {
            turret_components[num_turret_components] = i;
            component_used[num_turret_components] = 0;
            num_turret_components++;
        }
    }

    vp->m_num_turret_mounts = 0;
    for (int m = 0; m < vp->info->num_turret_mounts; m++) {
        const vehicle_info::turret_mount_info* mount_info = &vp->info->turret_mounts[m];
        const int tag = vp->get_tag_index_from_name(mount_info->prop_point);
        if (tag == -1) {
            char message[256];
            sprintf(message, "Warning: Can't find prop point %s for vehicle %s.\n", mount_info->prop_point, vp->info->name);
            console_output(message, NULL, false);
            continue;
        }
        vector pos;
        matrix orient;
        vp->get_tag_world_coords(tag, pos, &orient, &vp->pos, &vp->orient);
        obj_turret_props props(mount_info->attached_turret_info, pos, orient, tag, 0xFFFFFFFF);
        turret* new_turret = World.create_object<turret>(props, vp->get_handle());
        if (new_turret == NULL) {
            continue;
        }
        if (vp->m_num_turret_mounts == 0 && 0 < vp->m_turret_autofire_ms) {
            new_turret->automatic_firing_pattern_start(vp->m_turret_autofire_ms);
        }
        vp->m_turret_mounts[m].info = mount_info;
        vp->m_turret_mounts[m].attached_turret = new_turret->get_handle();
        vp->m_num_turret_mounts++;
        // two turrets on one seat are linked in a chain
        turret* first = vehicle_get_seat_turret(vp, mount_info->seat);
        if (first != new_turret) {
            turret* last = first;
            while (last->get_linked_turret() != 0xFFFFFFFF) {
                last = World.object_from_handle<turret>(last->get_linked_turret());
            }
            last->set_linked_turret(new_turret->get_handle());
        }
        const char* base_name = mount_info->attached_turret_info->components[1].vehicle_component_name;
        const char* barrel_name = mount_info->attached_turret_info->components[2].vehicle_component_name;
        for (unsigned int c = 0; c < num_turret_components; c++) {
            const char* bone_name = rdd->sub_info->m_components[turret_components[c]].bone_name;
            if (base_name != NULL && v_stricmp(bone_name, base_name) == 0) {
                component_used[c] = 1;
            }
            if (barrel_name != NULL && v_stricmp(bone_name, barrel_name) == 0) {
                component_used[c] = 1;
            }
        }
        if (vp->m_flags.disable_turrets_for_npcs != 0) {
            new_turret->turret_flags.locked_turret = true;
        }
    }
    // the mesh's turret parts no mount uses
    for (unsigned int c = 0; c < num_turret_components; c++) {
        if (component_used[c] == 0) {
            vehicle_turret_turn_off_components(vp, &rdd->m_components[turret_components[c]]);
        }
    }
}

// 0x00B76B70 (:1349)
void vehicle_destroy_turrets(vehicle* vp)
{
    for (int i = 0; i < vp->m_num_turret_mounts; i++) {
        if (vp->m_turret_mounts[i].attached_turret != 0xFFFFFFFF) {
            turret* tp = World.object_from_handle<turret>(vp->m_turret_mounts[i].attached_turret);
            if (tp != NULL) {
                World.destroy_object(tp, false);
            }
        }
    }
    vp->m_num_turret_mounts = 0;
}

// 0x00B76BE0 (:1365)
void vehicle_setup_havok_for_corpse(vehicle* vp)
{
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        automobile_setup_havok_for_corpse((automobile*)vp);
    }
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    if (body == NULL) {
        return;
    }
    body->activate();
    body->setFriction(0.5f);
    if (vp->rdd.is_loaded()) {
        const vehicle::resource_dependent_data* rdd = vp->rdd.get_data();
        hkVector4f center_of_mass;
        game_to_havok_vector(center_of_mass, rdd->sub_info->m_cm_dynamics_info.m_center_of_mass);
        body->setCenterOfMassLocal(center_of_mass);
        if (rdd->m_chassis_collision_component != 0xFFFFFFFF && vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
            const unsigned int body_filter = havok_body_get_collision_type(vp->havok_handle);
            hkpListShape* list = havok_filter_shape_collection_get_from_rigid_body(body);
            if (list != NULL) {
                for (unsigned int key = list->getFirstKey(); key != 0xFFFFFFFF; key = list->getNextKey(key)) { // HK_INVALID_SHAPE_KEY
                    // 0xB: the chassis collision group
                    if ((list->getCollisionFilterInfo(key) & 0xFFFF) == 0xB) {
                        list->setCollisionFilterInfo(key, body_filter);
                    }
                }
            }
        }
    }
    havok_entity_update_collision_filter(body, true, true);
    havok_body_set_auto_remove_level(body, HAVOK_AUTO_REMOVE_THIRD);
}

// 0x00B8B4F0 (:1411) (LTCG: attacker in EAX)
static human* get_human_attacker(object* attacker)
{
    if (attacker == NULL) {
        return NULL;
    }
    if (attacker->get_obj_type() == OT_HUMAN) {
        return (human*)attacker;
    }
    if (attacker->get_obj_type() == OT_VEHICLE) {
        const vehicle* vp = (const vehicle*)attacker;
        if (!vp->most_recent_driver_exit_time.valid() && vp->most_recent_driver != 0xFFFFFFFF) {
            return World.object_from_handle<human>(vp->most_recent_driver);
        }
    }
    return NULL;
}

// 0x00BB6900 (:1440)
void vehicle_blow_up(vehicle* vp, human* attacker, bool was_vehicle, const weapon_info* orig_weapon, bool explode)
{
    if (vp->m_flags.m_corpse != 0) {
        return;
    }
    vehicle_ai_free_resources(&vp->vai);
    if (explode || vp->get_sub_type() == OT_SUB_VEHICLE_FLYER) {
        vehicle_kill_passengers(vp, attacker, DBT_EXPLOSION, orig_weapon);
    }
    else {
        int remaining = vp->vii.num_humans;
        for (int i = 0; 0 < remaining && i < 8; i++) {
            const unsigned int handle = vp->vii.humans[i].handle;
            if (handle == 0xFFFFFFFF) {
                continue;
            }
            remaining--;
            object* objp = World.object_from_handle(handle);
            if (objp != NULL && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC) {
                ((npc*)objp)->get_ai()->notify_vehicle_disabled();
            }
        }
    }

    // a vehicle carried by this one goes with it
    object* first_child = vp->child_ptr;
    for (object* child = first_child; child != NULL;) {
        if (child->get_obj_type() == OT_VEHICLE) {
            vp->remove_child(child);
            vehicle_blow_up((vehicle*)child, attacker, was_vehicle, orig_weapon, true);
            break;
        }
        child = child->child_next;
        if (child == first_child) {
            break;
        }
    }

    vp->engine_stop(false, false);
    vehicle::resource_dependent_data* rdd = vp->rdd.is_loaded() ? vp->rdd.get_data() : NULL;
    if (rdd != NULL) {
        for (unsigned int i = 0; i < rdd->m_num_vehicle_lights; i++) {
            rdd->m_vehicle_lights[i].stop_all_effects();
            rdd->m_vehicle_lights[i].flags.broken = true;
            rdd->m_vehicle_lights[i].flags.dirty = true;
        }
        vp->set_render_flags(8);
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        automobile_effects_stop_wheel_effects((automobile*)vp);
    }
    vp->m_flags.m_corpse = 1;

    human* human_attacker = get_human_attacker(attacker);
    if (!Multiplayer) {
        if (human_attacker != NULL) {
            alm_report_vehicle_death(vp, human_attacker);
        }
    }
    if (Multiplayer && multi_is_server()) {
        multi_game_respawn_request_vehicle_respawn(vp, 5000);
    }

    vp->m_flags.m_on_fire = 0;
    vp->m_flags.m_nano_sim_fire = 0;
    vp->on_fire_timestamp.invalidate();
    for (int i = 0; i < 2; i++) {
        unsigned int& instance = vp->m_damage_areas[0].m_effect_instances[i];
        if (instance != 0xFFFFFFFF) {
            object_effect* effect = World.object_from_handle<object_effect>(instance);
            instance = 0xFFFFFFFF;
            if (effect != NULL) {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
        }
    }
    vp->m_flags.m_nano_sim_smoke = 0;
    vp->m_flags.m_engine_smoking = 0;
    vp->m_flags.m_needs_repair = 0;

    // the 12 largest components other than wheels stay on as debris pieces; the rest shatter or stay attached
    unsigned int largest_storage[3];
    bit_array largest;
    largest.init((unsigned char*)largest_storage, 66);
    largest.clear(false);
    vehicle_detach_turrets(vp);
    float sizes[12];
    unsigned int indices[12];
    int num_largest = 0;
    for (unsigned int i = 0; i < rdd->m_num_components; i++) {
        const vehicle_component& component = rdd->m_components[i];
        if ((component.get_flags() & (VCF_DETACHED | VCF_GLASS)) != 0) {
            continue;
        }
        const vehicle_component_data* data = component.get_data();
        if (vehicle_component_is_wheel(data) || data->m_durability == 6) {
            continue;
        }
        vector bmin;
        vector bmax;
        ((rl_mesh*)rdd->sub_info->m_vehicle_mesh)->get_subpiece_local_bb(data->m_submesh_id, bmin, bmax);
        const float size = (bmax.x - bmin.x) * (bmax.x - bmin.x) + (bmax.y - bmin.y) * (bmax.y - bmin.y) + (bmax.z - bmin.z) * (bmax.z - bmin.z);
        if (num_largest == 12) {
            int smallest = 0;
            for (int s = 1; s < 12; s++) {
                if (sizes[s] < sizes[smallest]) {
                    smallest = s;
                }
            }
            if (sizes[smallest] < size) {
                largest.set(indices[smallest], 0);
                sizes[smallest] = size;
                indices[smallest] = i;
                largest.set(i, 1);
            }
        }
        else {
            sizes[num_largest] = size;
            indices[num_largest] = i;
            largest.set(i, 1);
            num_largest++;
        }
    }
    for (unsigned int i = 0; i < rdd->m_num_components; i++) {
        if (rdd->m_components[i].m_constraint != NULL) {
            havok_constraint_release(rdd->m_components[i].m_constraint);
            rdd->m_components[i].m_constraint = NULL;
        }
    }
    const bool from_nano = orig_weapon != NULL && orig_weapon->weapon_class == WEAPON_CLASS_NANO_RIFLE;
    for (unsigned int i = 0; i < rdd->m_num_components; i++) {
        vehicle_component* component = &rdd->m_components[i];
        const vehicle_component_data* data = component->get_data();
        if (data->m_type == VCT_WHEEL_COLLISION || data->m_type == VCT_WHEEL_BUMPER || data->m_type == VCT_CHASSIS_COLLISION) {
            vehicle_component_set_hierarchy_collision(component, vp, COLLIDE_NO_COLLISION);
        }
        else if (data->m_durability == 6) {
            component->destroy_renderable_instance_absolute(vp);
        }
        else if ((component->get_flags() & VCF_DETACHED) == 0) {
            if ((component->get_flags() & VCF_GLASS) == 0) {
                const bool use_impact_sound = largest.get(i) != 0 || vehicle_component_is_wheel(data);
                // explode, not was_vehicle: rfg.exe reads [ESP+0x124] (arg 5) at 0x00BB6F08
                vp->component_detach(component, use_impact_sound, 750, DBT_EXPLOSION, explode, from_nano);
            }
            else {
                vehicle_component_shatter_glass(component, vp, &vp->pos, NULL, NULL);
            }
        }
    }

    const object_sub_type sub_type = vp->get_sub_type();
    if (sub_type == OT_SUB_VEHICLE_AUTO) {
        automobile_blow_up((automobile*)vp);
        const vehicle_info* info = vp->info;
        if (info->vehicle_classification == BVT_TANK || info->vehicle_classification == BVT_ART_TANK) {
            stats_increment_by_one(STATS_VEHICLES_DESTROYED_TANKS);
        }
        else if (info != NULL) {
            if (info->default_team == HUMAN_TEAM_EDF) {
                stats_increment_by_one(STATS_VEHICLES_DESTROYED_EDF_WHEELED);
            }
            else if (info->default_team == HUMAN_TEAM_GUERILLA || vp->team_of_most_recent_driver == HUMAN_TEAM_CIVILIAN) {
                stats_increment_by_one(STATS_VEHICLES_DESTROYED_CIVILIAN_WHEELED);
            }
        }
    }
    else if (sub_type == OT_SUB_VEHICLE_FLYER) {
        flyer_blow_up((flyer*)vp);
    }
    else if (sub_type == OT_SUB_VEHICLE_WALKER) {
        walker_blow_up((walker*)vp);
        stats_increment_by_one(STATS_VEHICLES_DESTROYED_CIVILIAN_WALKERS);
    }
    float cost = activity_register_damage_get_cost(vp);
    stats_increment(STATS_DESTRUCTION_COST_VEHICLES, &cost);

    vector flat_fvec = vp->orient.fvec.copy_flatten_normalize();
    matrix explosion_orient;
    explosion_orient.make(flat_fvec);
    vector impulse;
    impulse.rand_around_dir(UP_VECTOR, 0.8f, 1.0f);
    impulse.x *= vp->info->chassis_mass;
    impulse.y *= vp->info->chassis_mass;
    impulse.z *= vp->info->chassis_mass;
    vector explosion_pos;
    const int gas_tank = vp->get_tag_index_from_name("gas_tank");
    if (gas_tank == -1) {
        explosion_pos = vp->pos;
    }
    else {
        vp->get_tag_world_coords(gas_tank, explosion_pos, NULL, NULL, NULL);
    }
    // The explosion is gated on explode (rfg.exe 0x00BB70C9 reads [ESP+0x124], arg 5). Ghidra's decompile names it
    // was_vehicle: the vtbl+0x90 engine_stop(false, false) call above shows as the no-argument __vftable[1].get_classname,
    // which shifts every later stack argument by one slot.
    if (explode) {
        const vehicle_info* info = vp->info;
        if (vp->m_bomb_status == VEHICLE_BOMB_NONE) {
            if (info->m_explosion_info != NULL) {
                vector up = UP_VECTOR;
                explosion_create(info->m_explosion_info, vp, attacker, explosion_pos, explosion_orient, &up, orig_weapon, false);
                const float strength = fl_rand_range(0.5f, 1.0f) * 9.8f; // gravity times a random fraction
                impulse.x *= strength;
                impulse.y *= strength;
                impulse.z *= strength;
                havok_body_apply_point_impulse(vp->havok_handle, impulse, explosion_pos);
                vp->m_corpse_timer.set(30000);
                vp->m_corpse_absolute_longest_timer.set_rand(210000, 240000);
                vector engine_pos;
                const int engine = vp->get_tag_index_from_name("engine");
                if (engine == -1) {
                    engine_pos = vp->pos;
                }
                else {
                    vp->get_tag_world_coords(engine, engine_pos, NULL, NULL, NULL);
                }
                vp->m_corpse_effect = effects_play(info->m_effect_corpse_smoke, engine_pos, explosion_orient, true, vp->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            }
        }
        else {
            explosion_create(info->m_bomb_explosion_info, vp, NULL, explosion_pos, explosion_orient, NULL, NULL, false);
            const float strength = fl_rand_range(0.8f, 1.3f) * 9.8f;
            impulse.x *= strength;
            impulse.y *= strength;
            impulse.z *= strength;
            havok_body_apply_point_impulse(vp->havok_handle, impulse, explosion_pos);
            vp->m_corpse_timer.set(0);
            vp->m_corpse_absolute_longest_timer.set(0);
        }
    }
    vehicle_setup_havok_for_corpse(vp);
    activity_register_death(vp, attacker);
    vp->m_sound_delay_after_explosion.set(750);
    if (vp->m_mesh_deform_sphere_list != NULL) {
        mesh_deform_free_deform_spheres(&vp->m_mesh_deform_sphere_list);
        vp->m_num_mesh_deform_spheres = 0;
    }
    if (vp->m_mesh_deform_pending_list != NULL) {
        mesh_deform_free_deform_spheres(&vp->m_mesh_deform_pending_list);
    }
    if (attacker == (human*)player_get_local_player()) {
        player_kills_vehicle_event destroyed_event;
        destroyed_event.handle = vp->get_handle();
        destroyed_event.team = vp->team_of_most_recent_driver != HUMAN_TEAM_NONE ? vp->team_of_most_recent_driver : vp->info->default_team;
        gem_post_event(destroyed_event);
        vehicle* attacker_vehicle = human_in_vehicle(attacker);
        if (attacker_vehicle != NULL) {
            player_kills_vehicle_from_vehicle_event in_vehicle_event;
            in_vehicle_event.handle = attacker_vehicle->get_handle();
            in_vehicle_event.team = destroyed_event.team;
            gem_post_event(in_vehicle_event);
        }
    }
    vehicle_destroyed_event destroyed;
    destroyed.vehicle_handle = vp->get_handle();
    gem_post_event(destroyed);
    if (from_nano) {
        vp->start_nano_rifle_fade_out(true, attacker, ZERO_VECTOR);
    }
}

// 0x00B76D20 (:1895)
void vehicle_apply_hit_points(vehicle_damage_area* area, int hit_points)
{
    area->m_hit_points -= hit_points < area->m_hit_points ? hit_points : area->m_hit_points;
    if (area->m_max_hit_points < area->m_hit_points) {
        area->m_hit_points = area->m_max_hit_points;
    }
}

// 0x00B8B540 (:1909) (LTCG: vp in EAX)
static void vehicle_update_smoke_value(vehicle* vp)
{
    object_effect* effect = vehicle_effect_from_handle(vp->m_damage_areas[0].m_effect_instances[0]);
    if (effect != NULL) {
        effect->set_expression_variable(EFFECTS_VAR_VEHICLE_SMOKE_PERCENT, vehicle_get_smoke_amount(vp->m_damage_areas));
    }
}

// Fire and smoke thresholds of the engine hit points (rfg.exe compares against these literally).
static const int VEHICLE_ENGINE_FIRE_HIT_POINTS = 0x2EF;
static const int VEHICLE_ENGINE_MIN_SMOKE_HIT_POINTS = 0x6D6;

// 0x00B96FC0 (:1937) (LTCG: vp in ESI, area in EAX)
static bool vehicle_apply_damage_to_engine(vehicle* vp, object* attacker, bool was_vehicle, const weapon_info* attacker_weapon, vehicle_damage_area* area, int hit_points, bool engine_fire)
{
    if (Multiplayer && !multi_is_server()) {
        return false;
    }
    if (!engine_fire && vp->m_flags.m_only_fire_damage != 0) {
        return true;
    }
    vehicle_apply_hit_points(area, hit_points); // inlined in rfg.exe
    human* human_attacker = get_human_attacker(attacker);
    human* notified_attacker = (human*)attacker;
    if (!Multiplayer) {
        if (human_attacker != NULL) {
            alm_report_vehicle_damage(vp, human_attacker);
        }
        if (camera_get_mode() == CAMERA_SATELLITE_MODE) {
            player* local = player_get_local_player();
            human* driver = vehicle_get_first_passenger(vp, VSI_DRIVER);
            if (local != NULL && driver != NULL && human_teams_are_friendly(local, driver)) {
                hud_ui_satellite_report_fire_taken(vp->get_handle());
                // the satellite's own rounds count as the player's
                if (attacker_weapon != NULL && (attacker_weapon->projectile_info.flags & 0x2000) != 0) {
                    human_attacker = player_get_local_player();
                    notified_attacker = player_get_local_player();
                }
            }
        }
    }
    if (was_vehicle) {
        vp->ramming_damage_taken += hit_points;
        if (vp->ramming_damage_taken < 0) {
            vp->ramming_damage_taken = 0;
        }
        else if (area->m_max_hit_points < vp->ramming_damage_taken) {
            vp->ramming_damage_taken = area->m_max_hit_points;
        }
    }
    float percent = (float)area->m_hit_points / (float)area->m_max_hit_points;
    if (percent - 0.0f < 0.0f) {
        percent = 0.0f;
    }
    if (0.0f <= percent - 1.0f) {
        percent = 1.0f;
    }

    vehicle_takes_damage_event damage_event;
    damage_event.handle = vp->get_handle();
    damage_event.attacker = notified_attacker != NULL ? notified_attacker->get_handle() : 0xFFFFFFFF;
    damage_event.health_level = percent;
    gem_post_event(damage_event);

    // 30 % of the maximum, at least 0x6D6 hit points
    int smoke_hit_points = (int)((float)area->m_max_hit_points * 0.3f);
    if (smoke_hit_points <= VEHICLE_ENGINE_MIN_SMOKE_HIT_POINTS) {
        smoke_hit_points = VEHICLE_ENGINE_MIN_SMOKE_HIT_POINTS;
    }
    if (smoke_hit_points < area->m_hit_points) {
        return false;
    }
    const bool from_nano = attacker_weapon != NULL && attacker_weapon->weapon_class == WEAPON_CLASS_NANO_RIFLE;
    if (area->m_hit_points < VEHICLE_ENGINE_FIRE_HIT_POINTS) {
        if (vp->m_flags.m_on_fire == 0) {
            vehicle_catch_fire(vp, from_nano);
            player* local = player_get_local_player();
            if (vp->engine_inst != -1 && vehicle_human_in_vehicle(vp, local)) {
                audiolib_vehicle_engine_stop(vp->engine_inst);
            }
            if (human_attacker != NULL) {
                vp->m_killer_handle = human_attacker->get_handle();
                vp->m_killer_weapon = attacker_weapon;
                if (was_vehicle) {
                    vp->m_flags.m_kill_by_vehicle = 1;
                }
            }
        }
        return true;
    }
    if (vp->m_flags.m_engine_smoking != 0 && (vp->m_flags.m_nano_sim_smoke != 0 || !from_nano)) {
        vehicle_update_smoke_value(vp);
        return true;
    }
    vehicle_set_smoking_and_fire(vp->get_handle(), true, false, from_nano);
    return true;
}

// 0x00BB7450 (:2087) (LTCG: vp in EAX, time_step in XMM1)
static bool vehicle_engine_on_fire_do_frame(vehicle* vp, float time_step)
{
    if (vp->m_flags.m_invulnerable != 0) {
        return false;
    }
    const float damage = vp->m_fire_damage_rate * time_step + vp->m_fire_fractional_damage;
    const int whole_damage = (int)floor((double)damage);
    vp->m_fire_fractional_damage = damage - (float)whole_damage;
    object* killer = World.object_from_handle(vp->m_killer_handle);
    vehicle_apply_damage_to_engine(vp, killer, vp->m_flags.m_kill_by_vehicle != 0, vp->m_killer_weapon, &vp->m_damage_areas[0], whole_damage, true);
    if (vp->m_damage_areas[0].m_hit_points == 0) {
        vehicle_blow_up(vp, (human*)killer, vp->m_flags.m_kill_by_vehicle != 0, vp->m_killer_weapon, vp->m_flags.m_nano_sim_fire == 0);
        return true;
    }
    player* local = player_get_local_player();
    if (local != NULL && human_in_vehicle(local, vp->get_handle())) {
        camera_shake_play("vehicle_onfire", 1.0f, false);
    }
    return false;
}

// 0x00B646E0 (:2139) (LTCG: vp in EAX, damage_areas in ECX)
static unsigned int vehicle_get_damage_areas(vehicle_damage_area** damage_areas, unsigned int num_vehicle_damage_areas, vehicle* vp, const vector* damage_position, float damage_radius)
{
    damage_areas[0] = &vp->m_damage_areas[0];
    return 1;
}

// The filter of one component's shapes: wheels off, proxies and hull on their own groups.
static unsigned int vehicle_component_collision_filter(unsigned int type, unsigned int chassis_filter, unsigned int wheel_proxy_filter, unsigned int hull_filter)
{
    if (type == VCT_FRONT_WHEEL || type == VCT_REAR_WHEEL || type == VCT_AXLE_3_WHEEL || type == VCT_AXLE_4_WHEEL || type == VCT_TREAD_WHEEL_GROUND) {
        return COLLIDE_NO_COLLISION;
    }
    if (type == VCT_WHEEL_COLLISION || type == VCT_TREAD_WHEEL_IDLER) {
        return wheel_proxy_filter;
    }
    if (type == VCT_CHASSIS_COLLISION) {
        return hull_filter;
    }
    {
        return chassis_filter;
    }
}

// 0x00B76D40 (:2163) (LTCG: p_component in EAX, v_rdd in EDX). rfg.exe has no caller left.
static void vehicle_setup_collision_filter_ascension(hkArray<unsigned int,hkContainerHeapAllocator>& initial_collision_filters, const vehicle_component* const p_component, const vehicle::resource_dependent_data* const v_rdd, const unsigned int chassis_collision_filter, const unsigned int wheel_proxy_collision_filter, const unsigned int hull_collision_filter)
{
    if ((p_component->get_flags() & VCF_SHAPE_IN_PARENT) == 0) {
        return;
    }
    int start = 0;
    const vehicle_component* cur = p_component;
    for (unsigned int parent = cur->get_data()->m_parent_index; parent != 0xFFFFFFFF; parent = cur->get_data()->m_parent_index) {
        cur = &v_rdd->m_components[parent];
        start += cur->get_data()->m_parent_sub_piece_start_index;
        if ((cur->get_flags() & VCF_SHAPE_IN_PARENT) == 0) {
            return;
        }
    }
    const vehicle_component_data* data = p_component->get_data();
    const unsigned int first = data->m_parent_sub_piece_start_index + start;
    const unsigned int end = data->m_parent_num_sub_pieces + first;
    const unsigned int filter = vehicle_component_collision_filter(data->m_type, chassis_collision_filter, wheel_proxy_collision_filter, hull_collision_filter);
    for (unsigned int i = first; i < end; i++) {
        initial_collision_filters[i] = filter;
    }
}

// 0x00B76E10 (:2247)
static void vehicle_setup_collision_filter_due_to_variant_cms(hkArray<unsigned int,hkContainerHeapAllocator>& initial_collision_filters, const vehicle::resource_dependent_data* const v_rdd, const unsigned int chassis_collision_filter, const unsigned int wheel_proxy_collision_filter, const unsigned int hull_collision_filter)
{
    // breadth first from the root components. Original bug kept: 66 entries, no bound.
    const vehicle_component* queue[66];
    unsigned int queue_size = 0;
    for (unsigned int i = 0; i < v_rdd->m_num_components; i++) {
        if (v_rdd->m_components[i].get_data()->m_parent_index == 0xFFFFFFFF) {
            queue[queue_size++] = &v_rdd->m_components[i];
        }
    }
    for (unsigned int q = 0; q < queue_size; q++) {
        const vehicle_component* component = queue[q];
        int start = 0;
        const vehicle_component* cur = component;
        bool in_parent = (cur->get_flags() & VCF_SHAPE_IN_PARENT) != 0;
        while (in_parent) {
            const unsigned int parent = cur->get_data()->m_parent_index;
            if (parent == 0xFFFFFFFF) {
                break;
            }
            cur = &v_rdd->m_components[parent];
            start += cur->get_data()->m_parent_sub_piece_start_index;
            in_parent = (cur->get_flags() & VCF_SHAPE_IN_PARENT) != 0;
        }
        if (in_parent) {
            const vehicle_component_data* data = component->get_data();
            const unsigned int first = data->m_parent_sub_piece_start_index + start;
            const unsigned int end = data->m_parent_num_sub_pieces + first;
            const unsigned int filter = vehicle_component_collision_filter(data->m_type, chassis_collision_filter, wheel_proxy_collision_filter, hull_collision_filter);
            for (unsigned int i = first; i < end; i++) {
                initial_collision_filters[i] = filter;
            }
        }
        const vehicle_component_data* data = component->get_data();
        for (unsigned int c = 0; c < data->m_num_children; c++) {
            queue[queue_size++] = &v_rdd->m_components[data->m_children_indices[c]];
        }
    }
}

// 0x00B646F0 (:2422) (LTCG: hp in ESI, vp on the stack)
static bool vehicle_can_pre_ragdoll_human(const human* hp, const vehicle* vp)
{
    if (hp == NULL || hp->is_killed() || human_in_vehicle(hp) != NULL || human_in_vehicle_interaction(hp, NULL)) {
        return false;
    }
    if (hp->ground_obj_handle == vp->get_handle()) {
        return false;
    }
    const human_ragdoll_state state = hp->ragdoll_state;
    if (state == HUMAN_RAGDOLL_STATE_NONE || state == HUMAN_RAGDOLL_STATE_FLINCH || (state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && hp->ragdoll_on_impact_time <= Frametime * 2.0f)) {
        return !human_is_immune_to_ragdoll(hp);
    }
    return false;
}

// 0x00B64780 (:2462) (LTCG: hp in EAX)
static void vehicle_pre_ragdoll_human(human* hp)
{
    human_ragdoll_on_impact(hp, 0.3f, 7.0f, false);
}

// 0x00B647B0 (:2469) (LTCG: vp in EAX, veh_velocity in ECX, hp in EDI)
static void vehicle_check_pre_ragdoll_human(const vehicle* vp, human* hp, const vector& veh_velocity, float velocity_mag, float max_bbox_size_sq)
{
    const vector& pos = hp->pos;
    const float dx = pos.x - vp->pos.x;
    const float dy = pos.y - vp->pos.y;
    const float dz = pos.z - vp->pos.z;
    const float dist_sqr = dx * dx + dy * dy + dz * dz;
    if (max_bbox_size_sq <= dist_sqr) {
        // more than 0.3 s away at the closing speed
        const float closing = hp->actual_velocity_mag + velocity_mag;
        if (0.3f <= dist_sqr / (closing * closing)) {
            return;
        }
        // where the vehicle is in 0.15 s, relative to the human
        const vector future(vp->pos.x + (veh_velocity.x - hp->actual_velocity.x) * 0.15f, vp->pos.y + (veh_velocity.y - hp->actual_velocity.y) * 0.15f, vp->pos.z + (veh_velocity.z - hp->actual_velocity.z) * 0.15f);
        if (0.0f <= (pos.y - future.y) * dy + (pos.x - future.x) * dx + (pos.z - future.z) * dz) {
            vector closest;
            ix_closest_point_on_line_segment(closest, pos, vp->pos, future);
            if (max_bbox_size_sq <= vec_dist_squared(pos, closest)) {
                return;
            }
        }
    }
    vehicle_pre_ragdoll_human(hp); // inlined in rfg.exe
}

// 0x00B8B580 (:2508) (LTCG: vp on the stack)
static void vehicle_preemptively_ragdoll_nearby_humans(const vehicle* vp)
{
    player* local = player_get_local_player();
    bool only_local_player = false;
    if (local->vehicle_handle == 0xFFFFFFFF) {
        if (!vehicle_can_pre_ragdoll_human(local, vp)) {
            return;
        }
        const float dx = vp->pos.x - local->pos.x;
        const float dy = vp->pos.y - local->pos.y;
        const float dz = vp->pos.z - local->pos.z;
        if (2500.0f < dz * dz + dy * dy + dx * dx) {
            return; // 50 m
        }
        only_local_player = true;
    }
    else if (local->vehicle_handle != vp->get_handle()) {
        return;
    }
    vector velocity;
    vehicle_get_velocity(vp, velocity);
    const float speed_sqr = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    if (speed_sqr <= 36.0f) {
        return; // 6 m/s
    }
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    const float min_size = bmin.x * bmin.x + bmin.y * bmin.y + bmin.z * bmin.z;
    const float max_size = bmax.x * bmax.x + bmax.y * bmax.y + bmax.z * bmax.z;
    const float bbox_size_sqr = 0.0f <= min_size - max_size ? min_size : max_size;
    const float speed = sqrtf(speed_sqr);
    if (only_local_player) {
        if (local->rdd.is_loaded()) {
            vehicle_check_pre_ragdoll_human(vp, local, velocity, speed, bbox_size_sqr);
        }
        return;
    }
    const int num_humans = World.get_num_objects<human>();
    for (int i = 0; i < num_humans; i++) {
        human* hp = World.get_object<human>(i);
        if (vehicle_can_pre_ragdoll_human(hp, vp)) {
            vehicle_check_pre_ragdoll_human(vp, hp, velocity, speed, bbox_size_sqr);
        }
    }
}

// 0x00B76F90 (:2580)
void vehicle_class_system_init()
{
    Vehicle_physical_material_index = rfg_get_physical_material_index("Steel-Normal");
    Vehicle_effect_material_index = rfg_get_effect_material_index(Vehicle_physical_material_index);
    automobile_tire_track_infos_read_table();
}

// 0x00B64A40 (:2600): empty in rfg.exe.
void vehicle_class_level_init()
{
}

// 0x00BB3850 (:2606)
void vehicle_destroy_with_passengers(vehicle* vp)
{
    if (!Multiplayer || multi_is_server()) {
        vp->vii.passengers_destroy();
        World.destroy_object(vp, false);
    }
}

// 0x00B64A50 (:2623)
float vehicle::get_ramming_damage_percentage() const
{
    const int max_hit_points = m_damage_areas[0].m_max_hit_points;
    if (0 < max_hit_points && -1 < ramming_damage_taken && ramming_damage_taken <= max_hit_points) {
        return (float)ramming_damage_taken / (float)max_hit_points;
    }
    return 0.0f;
}

// 0x00BB3890 (:2637)
void vehicle::process_electrical_damage(float damage, human* attacker)
{
    if (1.0f <= m_electrical_damage_percent || is_killed()) {
        return;
    }
    m_electrical_damage_percent += damage;
    if (1.0f <= m_electrical_damage_percent) {
        m_electrical_damage_percent = 1.0f;
        vehicle_set_smoking_and_fire(get_handle(), true, false, false);
        const weapon* equipped = attacker != NULL ? attacker->get_equipped_weapon() : NULL;
        vehicle_kill_passengers(this, attacker, DBT_EXPLOSION, equipped != NULL ? equipped->w_info : NULL);
        engine_stop(false, false);
    }
}

// 0x00B64A90 (:2670): empty in rfg.exe.
bool vehicle::initialize_on_create(vehicle_info* vip, const vector& pos_in, const matrix& orient_in, bool pos_at_ground)
{
    return true;
}

// 0x00B64AA0 (:2884)
bool vehicle::is_killed() const
{
    return m_flags.m_corpse != 0;
}

// 0x00B8B820 (:2890)
bool vehicle::is_ambient() const
{
    if (!most_recent_driver_exit_time.valid() && most_recent_driver != 0xFFFFFFFF) {
        object* driver = World.object_from_handle(most_recent_driver);
        if (driver != NULL && driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == OT_SUB_HUMAN_NPC) {
            return ((npc*)driver)->get_manager() == (npc_manager_systems)1; // an ambient spawn's driver
        }
    }
    return false;
}

// 0x00B76FC0 (:2912)
void vehicle::get_bbox(vector& bmin, vector& bmax) const
{
    if (!rdd.is_loaded()) {
        get_local_bbox(bmin, bmax);
        bmin.x += pos.x;
        bmin.y += pos.y;
        bmin.z += pos.z;
        bmax.x += pos.x;
        bmax.y += pos.y;
        bmax.z += pos.z;
        return;
    }
    const vehicle_subinfo* sub_info = rdd.get_data()->sub_info;
    vector local_bmin;
    vector local_bmax;
    ((rl_mesh*)sub_info->m_vehicle_mesh)->get_subpiece_local_bb(sub_info->m_chassis_submesh_id, local_bmin, local_bmax);
    bbox_instance(bmin, bmax, orient, pos, local_bmin, local_bmax);
}

// 0x00B770E0 (:2934)
void vehicle::get_local_bbox(vector& bmin, vector& bmax) const
{
    if (!rdd.is_loaded()) {
        bmin.set(-0.5f, -0.5f, -0.5f);
        bmax.set(0.5f, 0.5f, 0.5f);
        return;
    }
    const vehicle_subinfo* sub_info = rdd.get_data()->sub_info;
    ((rl_mesh*)sub_info->m_vehicle_mesh)->get_subpiece_local_bb(sub_info->m_chassis_submesh_id, bmin, bmax);
}

// 0x00B77140 (:2956)
void vehicle::get_local_bbox_slow(vector& bmin, vector& bmax) const
{
    if (!rdd.is_loaded()) {
        bmin = info->spawn_box.min;
        bmax = info->spawn_box.max;
        return;
    }
    const resource_dependent_data* data = rdd.get_data();
    rl_mesh* mesh = (rl_mesh*)data->sub_info->m_vehicle_mesh;
    mesh->get_subpiece_local_bb(data->sub_info->m_chassis_submesh_id, bmin, bmax);
    for (int i = (int)data->m_num_components - 1; -1 < i; i--) {
        const vehicle_component& component = data->m_components[i];
        if ((component.get_flags() & VCF_DETACHED) != 0) {
            continue;
        }
        vector part_min;
        vector part_max;
        mesh->get_subpiece_local_bb(component.get_data()->m_submesh_id, part_min, part_max);
        bbox_transform(part_min, part_max, component.get_chassis_space_rotation(), ZERO_VECTOR, part_min, part_max);
        const vector& p = component.get_chassis_space_position();
        const vector expand_max(part_max.x + p.x, p.y + part_max.y, part_max.z + p.z);
        const vector expand_min(part_min.x + p.x, part_min.y + p.y, part_min.z + p.z);
        bbox_expand(bmin, bmax, expand_min, expand_max);
    }
}

// 0x00B8B870 (:2993)
int vehicle::attach_index(const vector& posp, unsigned int parent_objp_body_handle)
{
    const int component = find_closest_vehicle_component(this, posp);
    if (component != -1) {
        return (int)rdd.get_data()->m_components[component].get_first_shape_key(this);
    }
    return -1;
}

// 0x00B64AB0 (:3024)
bool vehicle::attach_index_is_valid(int index)
{
    if (index < 0) {
        return false;
    }
    if (get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        return true;
    }
    hkpRigidBody* body = havok_body_get_pointer(havok_handle);
    if (body != NULL) {
        havok_filter_shape_collection* collection = havok_filter_shape_collection_get_from_rigid_body(body);
        // Original bug kept: "<=" lets one past the last child through.
        if (collection != NULL && index <= collection->getNumChildShapes() + (int)collection->m_numDisabledChildren) {
            return true;
        }
    }
    return false;
}

// 0x00B972C0 (:3062)
unsigned char vehicle::get_physical_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const
{
    if (!rdd.is_loaded() || hk_objp == NULL) {
        return Vehicle_physical_material_index;
    }
    const unsigned int alt_index = get_alt_hk_body_handle_index(havok_rigid_body_get_handle((const hkpRigidBody*)hk_objp));
    const unsigned int component = vehicle_get_component_index_from_shape_key(this, shape_key, alt_index);
    if (component == 0xFFFFFFFF) {
        return object::get_physical_material(shape_key, contact_point, hk_objp);
    }
    return rfg_get_physical_material_index_by_checksum(rdd.get_data()->m_components[component].get_data()->m_mass_type);
}

// 0x00B64B10 (:3090)
unsigned char vehicle::get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const
{
    return rfg_get_effect_material_index(get_physical_material(shape_key, contact_point, hk_objp));
}

// 0x00B64B40 (:3097)
float vehicle::get_energy_scale()
{
    return info->m_energy_scale * RFG_vehicle_energy_scale;
}

// 0x00B64B60 (:3103)
hkpRigidBody* vehicle::get_supporting_rigid_body() const
{
    return NULL;
}

// 0x00B8B8C0 (:3117)
bool vehicle::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load)) {
        return false;
    }
    if (!load) {
        pblock.set<float>(VEHICLE_INITIAL_SPEED, vai.initial_forward_speed);
        for (unsigned int i = 0; i < m_num_nav_cell_detour_component_request_handles; i++) {
            nav_cell_detour_remove_request(m_nav_cell_detour_component_request_handles[i]);
        }
        m_num_nav_cell_detour_component_request_handles = 0;
        nav_cell_detour_remove_request(m_nav_cell_detour_request_handle);
        m_nav_cell_detour_request_handle = 0xFFFFFFFF;
        intersection_controller_handle_vehicle_destroyed(this);
        return true;
    }

    obj_flags.no_save = 1;
    *(unsigned char*)&m_rdd_flags = 0;
    last_pos = pos;
    last_orient = orient;
    last_velocity = ZERO_VECTOR;
    last_angular_velocity = ZERO_VECTOR;
    last_forward_velocity = 0.0f;
    forward_velocity = 0.0f;
    spawn_priority = SPAWN_PRIORITY_LOW;
    last_damage_reported = 0.0f;
    m_walker_velocity_hack.set(0);
    m_walker_velocity_hack_followup.invalidate();
    m_walker_thrown_energy_scaled.set(0);
    extra_mass_value = 1.0f;
    spawn_node_handle = 0xFFFFFFFF;
    squad_handle = 0xFFFFFFFF;
    num_damage_events = 0;
    m_num_nav_cell_detour_component_request_handles = 0;
    m_nav_cell_detour_request_handle = 0xFFFFFFFF;
    infinite_mass = false;
    extra_mass = false;
    vehicle_cover_timestamp.invalidate();
    vehicle_cover_nodes.clear();
    vehicle_cover_index = 0;
    vehicle_cover_creation_pos = ZERO_VECTOR;
    engine_inst = -1;
    engine_started_inst = -1;
    engine_high_load_start_timer.invalidate();
    memset(&m_flags, 0, sizeof(m_flags));
    pblock.get_buf_copy_optional(VEHICLE_PROP_FLAGS, (unsigned char*)&m_flags, 8);

    bool at_ground = false;
    pblock.get<bool>(VEHICLE_PROP_AT_GROUND, at_ground);
    if (pblock.get_buf_copy_optional(VEHICLE_MOVE_AT_STREAM_FLAGS, (unsigned char*)&stream_placement_flags, 4) == 0) {
        memset(&stream_placement_flags, 0, sizeof(stream_placement_flags));
    }
    int type = 0;
    if (!pblock.get_optional(VEHICLE_PROP_TYPE, type)) {
        char type_name[256];
        pblock.get(VEHICLE_PROP_TYPE_NAME, type_name);
        type = vehicle_get_type(type_name);
    }
    if (type < 0) {
        return false;
    }
    info = &Vehicle_info_pool[type];
    if (info->srid == 0) {
        return false;
    }
    srm_register_vehicle(this);
    m_rdd_flags.m_pos_on_ground_for_create = at_ground;
    vehicle_init_seat_info(this);
    vehicle_init_damage_areas(this);
    m_fade_timer.invalidate();
    m_corpse_timer.invalidate();
    m_corpse_absolute_longest_timer.invalidate();
    m_corpse_effect = 0xFFFFFFFF;
    m_turret_autofire_ms = -1;
    for (int i = 0; i < 4; i++) {
        m_exhaust_effect_normal[i] = 0xFFFFFFFF;
        m_exhaust_effect_burst[i] = 0xFFFFFFFF;
    }
    m_killer_handle = 0xFFFFFFFF;
    m_killer_weapon = NULL;
    damage_sound_propagate_timer.set(0);
    m_bomb_status = VEHICLE_BOMB_NONE;
    m_num_sub_pieces = 0;
    reserved_by = 0xFFFFFFFF;
    m_sound_delay_after_explosion.invalidate();
    ramming_damage_taken = 0;
    m_electrical_damage_percent = 0.0f;
    damage_percent = 0.0f;
    if (v_stricmp(info->name, "MarsRover1") == 0) {
        m_flags.m_invulnerable = 1;
        m_flags.no_player_use = 1;
        m_flags.invisible_driver = 1;
    }
    vai.vehicle_handle = get_handle();
    vai.steer_info.wheelbase = 0.0f;
    vai.active_control_info.max_braking_decel = info->m_ai_max_braking_decel;
    vai.active_control_info.max_radial_accel = info->m_ai_max_radial_accel;
    vai.base_control_info.max_braking_decel = info->m_ai_max_braking_decel;
    vai.base_control_info.max_radial_accel = info->m_ai_max_radial_accel;
    m_navcell_info.last_visited_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    m_navcell_info.last_visited_nav_cell_update_timer.invalidate();
    vehicle_ai_init(this);
    vii.initialize(this);
    pf_failure_timeout.invalidate();
    disable_for_npc_drivers_timer.set(0);
    no_npc_entry_timer.set(0);
    pblock.get<float>(VEHICLE_INITIAL_SPEED, vai.initial_forward_speed);
    m_mesh_deform_sphere_list = NULL;
    m_num_mesh_deform_spheres = 0;
    m_mesh_deform_pending_list = NULL;
    most_recent_driver = 0xFFFFFFFF;
    team_of_most_recent_driver = HUMAN_TEAM_NONE;
    most_recent_driver_exit_time.invalidate();
    rdd.set_loaded(false);
    m_nav_cell_detour_request_handle = nav_cell_detour_add_request(get_handle(), -1);
    request_streaming_resource(info->srid, STREAM_PRIORITY_AUDIO_CACHE_LOAD);
    if (m_rdd_flags.m_hide_vehicle != 0) {
        vehicle_hide(get_handle(), true);
        m_rdd_flags.m_hide_vehicle = 0;
    }
    memset(m_tag_deform_weights, 0, sizeof(m_tag_deform_weights));
    on_fire_timestamp.invalidate();
    emergency_light_timer.invalidate();
    return true;
}

// 0x00B64B70 (:3361)
obj_serialize_mode vehicle::serialize_mode()
{
    return OSM_FULL;
}

// Which light type a mesh light is (inlined twice in vehicle::stream).
static vehicle_light_type vehicle_light_type_from_component_type(unsigned int type)
{
    if (type == VCT_HEADLIGHT) {
        return VLT_HEAD_LIGHT;
    }
    if (type == VCT_FOGLIGHT) {
        return VLT_FOG_LIGHT;
    }
    if (type == VCT_TAILLIGHT) {
        return VLT_TAIL_LIGHT;
    }
    if (type == VCT_BRAKELIGHT) {
        return VLT_BRAKE_LIGHT;
    }
    if (type == VCT_REVERSELIGHT) {
        return VLT_REVERSE_LIGHT;
    }
    if (type == VCT_EMERGENCYLIGHT) {
        return VLT_EMERGENCY_LIGHT;
    }
    if (type == VCT_STROBELIGHT) {
        return VLT_STROBE_LIGHT;
    }
    {
        return VLT_INVALID;
    }
}

// The vehicle's body, its filter collection and optional MOPP tree.
static void vehicle_stream_create_body(vehicle* vp, vehicle::resource_dependent_data* v_rdd)
{
    physics_create_info create_info;
    create_info.effect_material = Vehicle_effect_material_index;
    create_info.shape_type = HAVOK_SHAPE_TYPE_VEHICLE;
    create_info.physical_material = Vehicle_physical_material_index;
    create_info.obj_ptr = vp;
    // a new collision system group for this vehicle's parts
    const unsigned int group = Havok_filter->getNewSystemGroup() << 16; // inlined in rfg.exe
    // collision layers: 0xF flyer, 0xC walker, 9 automobile
    const object_sub_type sub_type = vp->get_sub_type();
    create_info.collision_type = group | (sub_type == OT_SUB_VEHICLE_FLYER ? 0xF : sub_type == OT_SUB_VEHICLE_WALKER ? 0xC : 9);
    v_rdd->m_saved_collision_filter = create_info.collision_type;

    const hkpShape* shape = havok_cm_manager_get(v_rdd->sub_info->m_cm_dynamics_info.m_cm_index);
    hkArray<unsigned int,hkContainerHeapAllocator> filters;
    if (shape->getType() == hkcdShapeType::LIST) {
        unsigned int chassis_filter = create_info.collision_type;
        unsigned int hull_filter = create_info.collision_type;
        if (sub_type == OT_SUB_VEHICLE_AUTO && v_rdd->m_chassis_collision_component != 0xFFFFFFFF) {
            chassis_filter = group | 0xB;
            hull_filter = group | 0xA;
        }
        const hkpListShape* list = (const hkpListShape*)shape;
        for (int i = list->getNumChildShapes() + list->m_numDisabledChildren; i != 0; i--) {
            filters.pushBack(chassis_filter);
        }
        vehicle_setup_collision_filter_due_to_variant_cms(filters, v_rdd, chassis_filter, group | 0x2F, hull_filter);
    }
    else {
        filters.pushBack(create_info.collision_type);
    }
    create_info.shape = havok_filter_shape_collection_create(shape, filters);
    const hkpMoppCode* mopp = v_rdd->sub_info->m_cm_dynamics_info.m_mopp_code;
    if (mopp != NULL) {
        hkpMoppBvTreeShape* tree = new hkpMoppBvTreeShape((hkpShapeCollection*)create_info.shape, mopp);
        create_info.shape->removeReference();
        create_info.shape = tree;
    }
    create_info.friction = Vehicle_body_friction;
    create_info.pos = vp->pos;
    create_info.orient = vp->orient;
    create_info.linear_damping = 0.01f;
    create_info.angular_damping = 0.05f;
    create_info.restitution = 0.0f;
    create_info.motion_type = 3;
    const vehicle_info* info = vp->info;
    const vehicle_cm_dynamics_info& dynamics = v_rdd->sub_info->m_cm_dynamics_info;
    create_info.mass = info->chassis_mass;
    create_info.precalc_center_of_mass.set(dynamics.m_center_of_mass.x, info->center_of_mass_y + dynamics.m_center_of_mass.y, info->center_of_mass_z + dynamics.m_center_of_mass.z);
    const float mass = info->chassis_mass;
    const matrix& tensor = dynamics.m_inertia_tensor;
    create_info.precalc_tensor.rvec.set(tensor.rvec.x * mass, tensor.rvec.y * mass, tensor.rvec.z * mass);
    create_info.precalc_tensor.uvec.set(tensor.uvec.x * mass, tensor.uvec.y * mass, tensor.uvec.z * mass);
    create_info.precalc_tensor.fvec.set(tensor.fvec.x * mass, tensor.fvec.y * mass, tensor.fvec.z * mass);
    create_info.flags |= 1;
    create_info.simulation_quality_type = HK_COLLIDABLE_QUALITY_DEBRIS;
    vp->havok_handle = havok_body_create(create_info);
}

// The chassis mesh instance: the chassis, each light's off and on pieces, each attached part. False: no instance.
static bool vehicle_stream_create_chassis_instance(vehicle* vp, vehicle::resource_dependent_data* v_rdd)
{
    v_rdd->m_renderable_instance_p = rl_deformable_mesh_instance::create();
    if (v_rdd->m_renderable_instance_p == NULL) {
        return false;
    }
    const int common_mark = Common_mempool.mark();
    unsigned int num_lights = v_rdd->sub_info->m_num_lights;
    for (unsigned int i = 0; i < v_rdd->m_num_components; i++) {
        num_lights += v_rdd->m_components[i].get_data()->m_num_lights;
    }
    rl_sub_instance_info* sub_infos = (rl_sub_instance_info*)Common_mempool.alloc((v_rdd->m_num_components + 1 + num_lights * 2) * sizeof(rl_sub_instance_info), 0);
    sub_infos[0].m_submesh_id = v_rdd->sub_info->m_chassis_submesh_id;
    sub_infos[0].m_transform = IDENTITY_MATRIX43;
    sub_infos[0].m_is_active = 1;
    short num_sub_instances = 1;

    // headlights between Vehicle_headlights_time_on and Vehicle_headlights_time_off
    const game_clock* clock = game_clock_get();
    vp->m_flags.m_headlights_on = !(Vehicle_headlights_time_off < (int)clock->m_hour && (int)clock->m_hour < Vehicle_headlights_time_on);
    vp->emergency_light_timer.set((int)(Vehicle_emergency_lights_blink_time * 1000.0f));

    for (unsigned int i = 0; i < v_rdd->sub_info->m_num_lights; i++) {
        vehicle_mesh_light* mesh_light = &v_rdd->sub_info->m_lights[i];
        vehicle_light& light = v_rdd->m_vehicle_lights[v_rdd->m_num_vehicle_lights];
        light.mesh_light = mesh_light;
        light.parent_idx = -1;
        light.type = vehicle_light_type_from_component_type(mesh_light->type);
        rl_sub_instance_info* off = &sub_infos[num_sub_instances];
        off->m_submesh_id = mesh_light->off_mesh_id;
        off->m_transform.set_translation(mesh_light->position);
        off->m_transform.set_orientation(mesh_light->rotation);
        off->m_is_active = 1;
        light.mesh_index_off = num_sub_instances;
        rl_sub_instance_info* on = &sub_infos[num_sub_instances + 1];
        on->m_submesh_id = mesh_light->on_mesh_id;
        on->m_transform.set_translation(mesh_light->position);
        on->m_transform.set_orientation(mesh_light->rotation);
        on->m_is_active = 0;
        light.mesh_index_on = num_sub_instances + 1;
        num_sub_instances += 2;
        v_rdd->m_num_vehicle_lights++;
    }

    for (unsigned int i = 0; i < v_rdd->m_num_components; i++) {
        vehicle_component& component = v_rdd->m_components[i];
        component.clear_flags(VCF_ACTIVE_IN_CHASSIS_MESH);
        const vehicle_component_data* data = component.get_data();
        const unsigned int type = data->m_type;
        // wheels and treads are their own instances
        if (vehicle_component_is_wheel(data) || (component.get_flags() & VCF_NO_CHASSIS_MESH) != 0 || type == VCT_TREAD_SEGMENT || type == VCT_TREAD_WHEEL_GROUND || type == VCT_TREAD_WHEEL_IDLER) {
            component.m_sub_instance_idx = -1;
            continue;
        }
        rl_sub_instance_info* sub = &sub_infos[num_sub_instances];
        sub->m_is_active = (component.get_flags() & (VCF_DETACHED | VCF_NOT_RENDERED)) == 0;
        sub->m_submesh_id = data->m_submesh_id;
        sub->m_transform.set_translation(component.get_chassis_space_position());
        sub->m_transform.set_orientation(component.get_chassis_space_rotation());
        if (sub->m_is_active != 0) {
            component.set_flags(VCF_ACTIVE_IN_CHASSIS_MESH);
        }
        component.m_sub_instance_idx = num_sub_instances;
        num_sub_instances++;
        for (unsigned int l = 0; l < data->m_num_lights; l++) {
            vehicle_mesh_light* mesh_light = &data->m_lights[l];
            vehicle_light& light = v_rdd->m_vehicle_lights[v_rdd->m_num_vehicle_lights];
            light.mesh_light = mesh_light;
            light.parent_idx = (int)i;
            light.type = vehicle_light_type_from_component_type(mesh_light->type);
            // the light's transform is local to its component
            vector light_pos = mesh_light->position;
            matrix light_orient(mesh_light->rotation);
            light_pos.unrotate(component.get_chassis_space_rotation());
            light_pos.x += component.get_chassis_space_position().x;
            light_pos.y += component.get_chassis_space_position().y;
            light_pos.z += component.get_chassis_space_position().z;
            light_orient.unrotate(component.get_chassis_space_rotation());
            rl_sub_instance_info* off = &sub_infos[num_sub_instances];
            off->m_submesh_id = mesh_light->off_mesh_id;
            off->m_transform.set_translation(light_pos);
            off->m_transform.set_orientation(light_orient);
            off->m_is_active = 1;
            light.mesh_index_off = num_sub_instances;
            rl_sub_instance_info* on = &sub_infos[num_sub_instances + 1];
            on->m_submesh_id = mesh_light->on_mesh_id;
            on->m_transform.set_translation(light_pos);
            on->m_transform.set_orientation(light_orient);
            on->m_is_active = 0;
            light.mesh_index_on = num_sub_instances + 1;
            num_sub_instances += 2;
            v_rdd->m_num_vehicle_lights++;
        }
    }

    rl_deformable_mesh_instance* instance = v_rdd->m_renderable_instance_p;
    instance->init((rl_mesh*)v_rdd->sub_info->m_vehicle_mesh, (rl_material_map*)v_rdd->sub_info->m_material_map, sub_infos, num_sub_instances);
    instance->calculate_local_aabb();
    if ((rl_morph_container*)v_rdd->sub_info->m_morph_target != NULL) {
        rl_morph_instance* morph = rl_morph_instance::create();
        morph->init(per_vertex_weight, (rl_morph_container*)v_rdd->sub_info->m_morph_target);
        instance->set_morph_instance(morph);
    }
    Common_mempool.restore_to_mark(common_mark);
    instance->set_position(vp->pos);
    instance->set_rotation(vp->orient);
    game_render_scene_update(instance);
    instance->set_flag(0x100, false);
    return true;
}

// 0x00BA8DE0 (:3373)
bool vehicle::stream(bool load)
{
    if (!object::stream(load)) {
        return false;
    }
    if (!load) {
        for (unsigned int i = 0; i < m_num_nav_cell_detour_component_request_handles; i++) {
            nav_cell_detour_remove_request(m_nav_cell_detour_component_request_handles[i]);
        }
        m_num_nav_cell_detour_component_request_handles = 0;
        vehicle_destroy_turrets(this);
        vehicle_effects_kill_lights(this);
        resource_dependent_data* data = rdd.is_loaded() ? rdd.get_data() : NULL;
        if (data != NULL) {
            if (data->spline_render_update != NULL) {
                vehicle_render_free_spline_render_update(data->spline_render_update); // inlined in rfg.exe
            }
            if (data->m_renderable_instance_p != NULL) {
                data->m_renderable_instance_p->release();
                data->m_renderable_instance_p = NULL;
            }
        }
        vehicle_cover_destroy_nodes();
        // Original bug kept: the fire effect's handle is cleared through the smoke's slot.
        const unsigned int smoke = m_damage_areas[0].m_effect_instances[0];
        if (smoke != 0xFFFFFFFF) {
            m_damage_areas[0].m_effect_instances[0] = 0xFFFFFFFF;
            object_effect* effect = World.object_from_handle<object_effect>(smoke);
            if (effect != NULL) {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
        }
        const unsigned int fire = m_damage_areas[0].m_effect_instances[1];
        if (fire != 0xFFFFFFFF) {
            m_damage_areas[0].m_effect_instances[0] = 0xFFFFFFFF;
            object_effect* effect = World.object_from_handle<object_effect>(fire);
            if (effect != NULL) {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
        }
        if (m_corpse_effect != 0xFFFFFFFF) {
            object_effect* effect = World.object_from_handle<object_effect>(m_corpse_effect);
            if (effect != NULL) {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
            m_corpse_effect = 0xFFFFFFFF;
        }
        for (int i = 0; i < 4; i++) {
            if (m_exhaust_effect_normal[i] != 0xFFFFFFFF) {
                object_effect* effect = World.object_from_handle<object_effect>(m_exhaust_effect_normal[i]);
                if (effect != NULL) {
                    effect->stop(object_effect::SS_STOP_EMITTING);
                }
                m_exhaust_effect_normal[i] = 0xFFFFFFFF;
            }
            if (m_exhaust_effect_burst[i] != 0xFFFFFFFF) {
                object_effect* effect = World.object_from_handle<object_effect>(m_exhaust_effect_burst[i]);
                if (effect != NULL) {
                    effect->stop(object_effect::SS_STOP_EMITTING);
                }
                m_exhaust_effect_burst[i] = 0xFFFFFFFF;
            }
        }
        // Original bug kept: without loaded data these read through null.
        for (unsigned int i = 0; i < data->m_num_components; i++) {
            if (data->m_components[i].m_renderable_instance_p != NULL) {
                data->m_components[i].m_renderable_instance_p->release();
                data->m_components[i].m_renderable_instance_p = NULL;
            }
        }
        vehicle_free_havok_data(this);
        if (m_mesh_deform_sphere_list != NULL) {
            mesh_deform_free_deform_spheres(&m_mesh_deform_sphere_list);
            m_num_mesh_deform_spheres = 0;
        }
        if (m_mesh_deform_pending_list != NULL) {
            mesh_deform_free_deform_spheres(&m_mesh_deform_pending_list);
        }
        for (unsigned int i = 0; i < data->m_num_components; i++) {
            if (data->m_components[i].m_mesh_deform_pending_list != NULL) {
                mesh_deform_free_deform_spheres(&data->m_components[i].m_mesh_deform_pending_list);
                data->m_components[i].m_mesh_deform_pending_list = NULL;
            }
        }
        if (data->rig != NULL) {
            vanim_delete(vp);
            vp = NULL;
            if (-1 < aim_handle) {
                animlib_delete_anim_instance(aim_handle);
            }
            aim_handle = -1;
        }
        clean_up_debris();
        rdd.set_loaded(false);
        engine_stop(false, false);
        if (is_killed() && vii.num_humans < 1 && (!mission_is_active() || !Vehicle_system_avoid_despawn || !object_is_local_player(most_recent_driver))) {
            World.destroy_object(this, false);
        }
        return true;
    }

    if (sp_should_move_streamed_vehicle(this)) {
        sp_move_streamed_vehicle(this);
    }
    else {
        sp_clean_stream_area(this);
    }
    rdd.set_loaded(true);
    resource_dependent_data* v_rdd = rdd.get_data();
    v_rdd->sub_info = vehicle_subinfo_find(info->mesh_name);
    pass_by_play_id = -1;
    pass_by_distance = 1e+06f;
    if (m_flags.immediate_spawn == 0) {
        vehicle_fade_in(this, 1000);
    }
    m_flags.immediate_spawn = 0;
    v_rdd->m_num_vehicle_lights = 0;
    for (int i = 0; i < 20; i++) {
        v_rdd->m_vehicle_lights[i].reset();
        v_rdd->m_vehicle_lights[i].effect_handle = 0xFFFFFFFF;
    }
    for (int i = 0; i < 4; i++) {
        v_rdd->spline_data[i].num_treads = -1;
        v_rdd->spline_data[i].starting_offset_percent = 0.0f;
    }
    v_rdd->spline_render_update = NULL;
    for (int i = 0; i < 80; i++) {
        v_rdd->spline_debris[i] = 0xFFFFFFFF;
    }
    v_rdd->m_num_components = v_rdd->sub_info->m_num_components;
    v_rdd->track_pad_component_id = -1;
    v_rdd->m_wheel_bumper_component = 0xFFFFFFFF;
    v_rdd->m_chassis_collision_component = 0xFFFFFFFF;
    for (unsigned int i = 0; i < v_rdd->sub_info->m_num_components; i++) {
        vehicle_component& component = v_rdd->m_components[i];
        component.init(v_rdd->sub_info->m_components[i], *v_rdd->sub_info, this, false);
        const unsigned int type = component.get_data()->m_type;
        if (type == VCT_TREAD_SEGMENT) {
            v_rdd->track_pad_component_id = (int)i;
        }
        if (type == VCT_WHEEL_BUMPER) {
            v_rdd->m_wheel_bumper_component = i;
        }
        if (type == VCT_CHASSIS_COLLISION) {
            v_rdd->m_chassis_collision_component = i;
        }
        if (m_num_nav_cell_detour_component_request_handles < 8) {
            bool detour = (component.get_flags() & VCF_NAV_DETOUR) != 0;
            // the raider's rear wheels detour too
            if (info == Nav_cell_detour_veh_info_mar_raider_1 && !detour) {
                detour = type == VCT_REAR_WHEEL;
            }
            if (detour) {
                m_nav_cell_detour_component_request_handles[m_num_nav_cell_detour_component_request_handles] = nav_cell_detour_add_request(get_handle(), (short)i);
                m_num_nav_cell_detour_component_request_handles++;
            }
        }
    }
    vehicle_calc_cs_pos_orient_for_components(this);

    // the interaction's component names resolve against the first mesh that streams in
    vehicle_interaction_info* vi_info = info->vi_info;
    if (vi_info != NULL && !vi_info->is_initialized) {
        vi_info->is_initialized = true;
        for (int t = 0; t < vi_info->num_waypoint_transitions; t++) {
            vehicle_interaction_waypoint_transition& transition = vi_info->transitions[t];
            if (transition.component_obstacle_name == NULL) {
                continue;
            }
            for (unsigned int c = 0; c < v_rdd->sub_info->m_num_components; c++) {
                if (v_stricmp(v_rdd->sub_info->m_components[c].bone_name, transition.component_obstacle_name) == 0) {
                    transition.component_obstacle_index = (signed char)c;
                    break;
                }
            }
        }
        for (int w = 0; w < vi_info->num_waypoints; w++) {
            vehicle_interaction_waypoint& waypoint = vi_info->waypoints[w];
            if (waypoint.parent_component_name == NULL) {
                waypoint.parent_component_index = -4; // the chassis
                continue;
            }
            waypoint.parent_component_index = -5; // a name no component has
            for (unsigned int c = 0; c < v_rdd->sub_info->m_num_components; c++) {
                if (v_stricmp(v_rdd->sub_info->m_components[c].bone_name, waypoint.parent_component_name) == 0) {
                    waypoint.parent_component_index = (int)c;
                    break;
                }
            }
        }
    }

    vehicle_stream_create_body(this, v_rdd);
    if (get_sub_type() != OT_SUB_VEHICLE_FLYER && v_rdd->m_chassis_collision_component != 0xFFFFFFFF && v_rdd->m_wheel_bumper_component != 0xFFFFFFFF) {
        vehicle_component_set_hierarchy_collision(&v_rdd->m_components[v_rdd->m_wheel_bumper_component], this, COLLIDE_NO_COLLISION);
        v_rdd->m_wheel_bumper_component = 0xFFFFFFFF;
    }
    vehicle_init_tag_ids(this);
    for (unsigned int i = 0; i < v_rdd->m_num_components; i++) {
        if (v_rdd->m_components[i].get_data()->m_parent_index == 0xFFFFFFFF) {
            vehicle_component_calc_ws_pos_orient_recursive(&v_rdd->m_components[i], this, &orient, &pos);
        }
    }
    if (!vehicle_stream_create_chassis_instance(this, v_rdd)) {
        World.destroy_object(this, false);
        return false;
    }
    for (unsigned int i = 0; i < v_rdd->m_num_components; i++) {
        vehicle_component& component = v_rdd->m_components[i];
        component.m_renderable_instance_p = NULL;
        component.m_renderable_instance_refs = 0;
        const unsigned int type = component.get_data()->m_type;
        const bool own_instance = vehicle_component_is_wheel(component.get_data()) || type == VCT_TREAD_WHEEL_GROUND || type == VCT_TREAD_WHEEL_IDLER || type == VCT_TREAD_SEGMENT;
        if ((component.get_flags() & (VCF_DETACHED | VCF_NOT_RENDERED | VCF_NO_CHASSIS_MESH)) == 0 && own_instance && !component.create_renderable_instance(this)) {
            World.destroy_object(this, false);
            return false;
        }
    }
    aim_handle = -1;
    if (info->rig_name != NULL) {
        vp = vanim_create(info->anim_set_name);
        v_rdd->rig = animlib_get_loaded_rig(info->rig_name);
        aim_handle = animlib_new_anim_instance_manager(v_rdd->rig, info->bones_used_info.unique_id);
        vp->aim_handle = aim_handle;
        vanim_change_state(vp, index_alias(1, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        for (unsigned int i = 0; i < v_rdd->m_num_components; i++) {
            v_rdd->component_bone_index[i] = animlib_find_bone_by_name(v_rdd->rig, v_rdd->m_components[i].get_data()->bone_name);
        }
    }
    m_vehicle_vs_building_damage.set(0);
    walker_vehicle_collisions = 0;

    // shape type 8 a list, 9 a MOPP tree over one, anything else a single piece
    const hkpShape* shape = havok_cm_manager_get(v_rdd->sub_info->m_cm_dynamics_info.m_cm_index);
    if (shape != NULL) {
        const unsigned char shape_type = (unsigned char)shape->getType();
        if (shape_type == 8 || shape_type == 9) {
            const hkpShape* list = shape_type == 9 ? (const hkpShape*)((const hkpMoppBvTreeShape*)shape)->getShapeCollection() : shape;
            if (list != NULL) {
                m_num_sub_pieces = ((const hkpListShape*)list)->getNumChildShapes();
            }
        }
        else {
            m_num_sub_pieces = 1;
        }
    }
    for (unsigned int i = 0; i < v_rdd->m_num_components; i++) {
        const vehicle_component_data* data = v_rdd->m_components[i].get_data();
        if (data->m_parent_index == 0xFFFFFFFF && (data->m_flags & VCF_SHAPE_IN_PARENT) != 0 && data->m_parent_sub_piece_start_index != 0 && data->m_parent_sub_piece_start_index < m_num_sub_pieces) {
            m_num_sub_pieces = data->m_parent_sub_piece_start_index;
        }
    }
    set_render_flags(8);
    if (get_sub_type() != OT_SUB_VEHICLE_WALKER) {
        vehicle_build_turrets(this);
    }
    if (v_stricmp(info->name, "MarsRover1") == 0) {
        infinite_mass = true;
        automobile_set_havok_infinite_mass(this, true);
    }
    return true;
}

// 0x00B64B80 (:4185)
void vehicle::check_for_vehicle_intersection()
{
    if (sp_should_move_streamed_vehicle(this)) {
        sp_move_streamed_vehicle(this);
    }
    else {
        sp_clean_stream_area(this);
    }
}

// 0x00B77310 (:4198)
void vehicle::clean_up_debris()
{
    if (!rdd.is_loaded()) {
        return;
    }
    resource_dependent_data* data = rdd.get_data();
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_clean_up_debris_object(this, data->m_components[i].m_debris_object);
    }
    for (int i = 0; i < 80; i++) {
        vehicle_clean_up_debris_object(this, data->spline_debris[i]);
    }
}

// 0x00BB3950 (:4229)
void vehicle::destroy_notify()
{
    object::destroy_notify();
    vii.passengers_destroy();
    m_flags.disable_for_npc_drivers = 1;
    // corpses lying on the vehicle go with it
    contact_node* first = contact_info.m_contact_list;
    for (contact_node* contact = first; contact != NULL;) {
        if (contact->m_contacted_object != 0xFFFFFFFF) {
            human* hp = World.object_from_handle<human>(contact->m_contacted_object);
            if (hp != NULL && hp->get_sub_type() == OT_SUB_HUMAN_NPC && hp->is_killed()) {
                hp->corpse_cleanup_timer.set(0);
                ((npc*)hp)->fade_out_and_despawn(0, false);
            }
        }
        contact = contact->next;
        if (contact == first) {
            break;
        }
    }
    clean_up_debris();
    dialogue_manager_cancel_vehicle(get_handle());
    engine_stop(false, false);
}

// 0x00BB3A00 (:4287)
void vehicle::free_this()
{
    if (0 < vehicle_cover_nodes.size()) {
        vehicle_cover_destroy_nodes();
    }
    engine_stop(false, false);
    srm_remove_vehicle(this);
    if (!Multiplayer || multi_is_server()) {
        vii.passengers_destroy();
    }
    vehicle_ai_free_resources(&vai);
    nav_cell_detour_remove_request(m_nav_cell_detour_request_handle);
    m_nav_cell_detour_request_handle = 0xFFFFFFFF;
    for (unsigned int i = 0; i < m_num_nav_cell_detour_component_request_handles; i++) {
        nav_cell_detour_remove_request(m_nav_cell_detour_component_request_handles[i]);
    }
    m_num_nav_cell_detour_component_request_handles = 0;
    intersection_controller_handle_vehicle_destroyed(this);
    object::free_this();
}

// 0x00B773D0 (:4353)
void vehicle::set_infinite_mass(bool set)
{
    infinite_mass = set;
    automobile_set_havok_infinite_mass(this, set);
}

// 0x00B77410 (:4442)
void vehicle::vehicle_cover_destroy_nodes()
{
    for (int i = 0; i < vehicle_cover_nodes.size(); i++) {
        World.destroy_object(vehicle_cover_nodes[i], false);
    }
    vehicle_cover_nodes.clear();
}

// 0x00B97350 (:4451)
void vehicle::vehicle_cover_process()
{
    const int num_definitions = info->cover_info != NULL ? info->cover_info->node_info.size() : 0;
    if (num_definitions < 1) {
        return;
    }
    bool keep_timer = false;
    if (!is_killed() && m_flags.m_on_fire == 0 && rdd.is_loaded()) {
        object* driver = (!most_recent_driver_exit_time.valid() && most_recent_driver != 0xFFFFFFFF) ? World.object_from_handle(most_recent_driver) : NULL;
        if (driver != NULL && driver->get_obj_type() != OT_HUMAN) {
            driver = NULL;
        }
        if (!object_is_player(driver)) {
            if (VEHICLE_DESTROY_COVER_DIST_SQR < vec_dist_squared(pos, vehicle_cover_creation_pos)) {
                // moved: start over from here
                vehicle_cover_creation_pos = pos;
                vehicle_cover_timestamp.set(1500);
                keep_timer = true;
            }
            else if (MIN_VEHICLE_COVER_ORIENT_VERTICAL <= orient.uvec.y) {
                if (!vehicle_cover_timestamp.valid()) {
                    // staggered by the number of automobiles
                    vehicle_cover_timestamp.set(World.get_num_objects<automobile>() * 600);
                }
                keep_timer = true;
            }
        }
    }
    if (!keep_timer) {
        vehicle_cover_timestamp.invalidate();
    }
    if (!vehicle_cover_timestamp.valid() || !vehicle_cover_timestamp.elapsed()) {
        if (0 < vehicle_cover_nodes.size()) {
            vehicle_cover_destroy_nodes();
        }
        vehicle_cover_index = 0;
        return;
    }
    if (vehicle_cover_index < num_definitions && !Processed_this_frame) {
        vehicle_cover_creation_pos = pos;
        const unsigned int node = vehicle_cover_create_node(this, info->cover_info->node_info[vehicle_cover_index]);
        if (node != 0xFFFFFFFF) {
            vehicle_cover_nodes.add(node);
        }
        vehicle_cover_index++;
        if (vehicle_cover_index != num_definitions) {
            return;
        }
        vehicle_check_cover_index = 0;
        vehicle_check_cover_timestamp.set(Num_vehicles_checking_cover_last_frame * 600);
    }
    if (vehicle_cover_index == num_definitions && vehicle_check_cover_index < num_definitions) {
        Num_vehicles_checking_cover++;
        if (!Checked_this_frame && vehicle_check_cover_timestamp.valid() && vehicle_check_cover_timestamp.elapsed()) {
            vehicle_cover_check_node(this, info->cover_info->node_info[vehicle_check_cover_index]);
            vehicle_check_cover_index++;
            if (vehicle_check_cover_index == num_definitions) {
                vehicle_check_cover_index = 0;
                vehicle_check_cover_timestamp.set(Num_vehicles_checking_cover_last_frame * 600);
            }
        }
    }
}

// 0x00B77460 (:4579)
unsigned int vehicle::get_alt_hk_body_handle(unsigned int index) const
{
    unsigned int obj_handle;
    if (index == 0) {
        obj_handle = havok_handle;
    }
    else {
        if (!rdd.is_loaded() || rdd.get_data()->m_num_components <= index - 1) {
            return 0xFFFFFFFF;
        }
        obj_handle = rdd.get_data()->m_components[index - 1].havok_handle;
    }
    if (obj_handle != 0xFFFFFFFF && havok_body_get_pointer(obj_handle) == NULL) {
        return 0xFFFFFFFF;
    }
    return obj_handle;
}

// 0x00B774B0 (:4614)
unsigned int vehicle::get_alt_hk_body_handle_index(unsigned int alt_handle) const
{
    if (alt_handle != 0xFFFFFFFF && havok_body_get_pointer(alt_handle) != NULL) {
        if (alt_handle == havok_handle) {
            return 0;
        }
        if (rdd.is_loaded()) {
            const resource_dependent_data* data = rdd.get_data();
            for (unsigned int i = 0; i < data->m_num_components; i++) {
                if (alt_handle == data->m_components[i].havok_handle) {
                    return i + 1;
                }
            }
        }
    }
    return 0xFFFFFFFF;
}

// 0x00B77510 (:4646)
unsigned int vehicle::get_component_idx_from_alt_body_index(unsigned int alt_index) const
{
    if (alt_index == 0xFFFFFFFF || alt_index == 0 || (rdd.is_loaded() && rdd.get_data()->m_num_components <= alt_index - 1)) {
        return 0xFFFFFFFF;
    }
    return alt_index - 1;
}

// 0x00B77540 (:4677)
void vehicle::engine_start(bool player)
{
    if (m_flags.m_engine_running != 0) {
        if (player && audiolib_vehicle_engine_is_playing(engine_inst)) {
            audiolib_vehicle_engine_audio_to_2d(engine_inst);
        }
        return;
    }
    vehicle_handle_enter_audio(this, player);
    if (info->m_effect_exhaust_normal != 0xFFFFFFFF && info->m_effect_exhaust_burst != 0xFFFFFFFF) {
        for (int i = 0; i < 4; i++) {
            const int tag = info->m_exhaust_tag_id[i];
            if (tag == -1) {
                continue;
            }
            vector tag_pos;
            matrix tag_orient;
            get_tag_world_coords(tag, tag_pos, &tag_orient, NULL, NULL);
            m_exhaust_effect_normal[i] = effects_play(info->m_effect_exhaust_normal, tag_pos, tag_orient, true, get_handle(), tag, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            effects_play(info->m_effect_exhaust_burst, tag_pos, tag_orient, false, get_handle(), tag, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
    }
    vehicle_camera_shake_play_engine_start(this);
    m_flags.m_engine_running = 1;
}

// 0x00B77690 (:4729)
void vehicle::engine_stop(bool player, bool streamout)
{
    if (this == NULL) { // the original checks
        return;
    }
    if (get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        automobile_release_horn(get_handle());
    }
    if (m_flags.m_engine_running == 0) {
        return;
    }
    vehicle_handle_exit_audio(this, true, player, streamout);
    for (int i = 0; i < 4; i++) {
        if (m_exhaust_effect_normal[i] != 0xFFFFFFFF) {
            object_effect* effect = World.object_from_handle<object_effect>(m_exhaust_effect_normal[i]);
            if (effect != NULL) {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
            m_exhaust_effect_normal[i] = 0xFFFFFFFF;
        }
        if (m_exhaust_effect_burst[i] != 0xFFFFFFFF) {
            object_effect* effect = World.object_from_handle<object_effect>(m_exhaust_effect_burst[i]);
            if (effect != NULL) {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
            m_exhaust_effect_burst[i] = 0xFFFFFFFF;
        }
    }
    m_flags.m_engine_running = 0;
}

// 0x00B64BB0 (:4787)
int vehicle_get_num_seats(const vehicle* vp)
{
    return vp->info->vi_info != NULL ? vp->info->vi_info->num_seats : 1;
}

// 0x00B77750 (:4811)
int vehicle_get_num_seats(unsigned int v_handle)
{
    return vehicle_get_num_seats(vehicle_from_handle(v_handle));
}

// 0x00B77790 (:4821)
void vehicle_get_angular_velocity(const vehicle* vp, vector& vel)
{
    if (!vp->rdd.is_loaded()) {
        vel = ZERO_VECTOR;
        return;
    }
    const object_sub_type sub_type = vp->get_sub_type();
    if (sub_type == OT_SUB_VEHICLE_AUTO || sub_type == OT_SUB_VEHICLE_FLYER) {
        havok_to_game_vector(vel, havok_body_get_pointer(vp->havok_handle)->getAngularVelocity());
    }
    else if (sub_type == OT_SUB_VEHICLE_WALKER) {
        keen::debug::nativePrintArgs("Walkers dont support this function!");
    }
}

// 0x00B64BD0 (:4838)
void vehicle_get_last_velocity(const vehicle* vp, vector& vel)
{
    vel = vp->last_velocity;
}

// 0x00B64BF0 (:4844)
void vehicle_get_last_angular_velocity(const vehicle* vp, vector& vel)
{
    vel = vp->last_angular_velocity;
}

// 0x00B77840 (:4855)
void vehicle_get_velocity(const vehicle* vp, vector& vel)
{
    if (!vp->rdd.is_loaded()) {
        vel = ZERO_VECTOR;
        return;
    }
    const object_sub_type sub_type = vp->get_sub_type();
    if (sub_type == OT_SUB_VEHICLE_AUTO || sub_type == OT_SUB_VEHICLE_FLYER) {
        havok_to_game_vector(vel, havok_body_get_pointer(vp->havok_handle)->getLinearVelocity());
    }
    else if (sub_type == OT_SUB_VEHICLE_WALKER) {
        walker_get_velocity((walker*)vp, vel);
    }
}

// 0x00B778F0 (:4877)
void vehicle_get_velocity(unsigned int v_handle, vector& vel)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp != NULL) {
        vehicle_get_velocity(vp, vel);
        return;
    }
    vel.zero();
}

// 0x00B64C10 (:4894)
unsigned int vehicle_get_hit_points(const vehicle* vp)
{
    return vp->m_damage_areas[0].m_hit_points;
}

// 0x00B64C20 (:4906)
unsigned int vehicle_get_max_hit_points(const vehicle* vp)
{
    return vp->m_damage_areas[0].m_max_hit_points;
}

// 0x00B77920 (:4920)
void vehicle_get_aligned_box(vector& bmin, vector& bmax, vector& pos, matrix& orient, const vehicle* vp)
{
    if (!vp->rdd.is_loaded()) {
        bmin.zero();
        bmax.zero();
        pos.zero();
        orient.make_identity();
        return;
    }
    const vehicle_subinfo* sub_info = vp->rdd.get_data()->sub_info;
    ((rl_mesh*)sub_info->m_vehicle_mesh)->get_subpiece_local_bb(sub_info->m_chassis_submesh_id, bmin, bmax);
    pos = vp->pos;
    orient = vp->orient;
}

// 0x00B64C30 (:4943)
void vehicle_get_last_pos(const vehicle* vp, vector& last_pos)
{
    last_pos = vp->last_pos;
}

// 0x00B64C50 (:4954)
void vehicle_get_curr_pos(const vehicle* vp, vector& pos)
{
    pos = vp->pos;
}

// 0x00B64C70 (:4995)
bool vehicle::not_allowed_to_ragdoll() const
{
    return m_flags.no_ragdoll != 0;
}

// 0x00B64C80 (:5001)
bool vehicle::in_vehicle(human* hp) const
{
    return get_handle() == hp->vehicle_handle;
}

// 0x00B64CA0 (:5007)
bool vehicle::is_offscreen() const
{
    vector bmin;
    vector bmax;
    get_local_bbox(bmin, bmax);
    // a quarter larger, so a vehicle is gone from view before it despawns
    bmin.x *= 1.25f;
    bmin.y *= 1.25f;
    bmin.z *= 1.25f;
    bmax.x *= 1.25f;
    bmax.y *= 1.25f;
    bmax.z *= 1.25f;
    return game_render_cull_oriented_bounding_box(bmin, bmax, pos, orient);
}

// 0x00B64D70 (:5024)
int vehicle_get_num_passengers(const vehicle* vp)
{
    return vp->vii.num_humans;
}

// 0x00B779C0 (:5047)
int vehicle_get_num_passengers(unsigned int v_handle)
{
    return vehicle_get_num_passengers(vehicle_from_handle(v_handle));
}

// 0x00B779F0 (:5062)
human* vehicle_passenger_in_seat(const vehicle* vp, vehicle_seat_index seat_index)
{
    if (seat_index == VSI_INVALID) {
        return NULL;
    }
    if (seat_index == VSI_DRIVER) {
        if (vp->most_recent_driver_exit_time.valid()) {
            return NULL;
        }
        return vehicle_human_from_handle(vp->most_recent_driver);
    }
    for (int i = 0; i < 8; i++) {
        human* hp = vehicle_human_from_handle(vp->vii.humans[i].handle);
        if (hp != NULL && hp->vehicle_seat_idx == seat_index) {
            return hp;
        }
    }
    return NULL;
}

// 0x00B77A80 (:5087)
bool vehicle_player_is_occupant(const vehicle* vp)
{
    for (int i = 0; i < 8; i++) {
        human* hp = vehicle_human_from_handle(vp->vii.humans[i].handle);
        if (hp != NULL && object_is_player(hp)) {
            return true;
        }
    }
    return false;
}

// 0x00B77AE0 (:5105)
bool vehicle_human_is_riding_with_non_exiting_player(const human* hp)
{
    if (hp == NULL || hp->vehicle_handle == 0xFFFFFFFF) {
        return false;
    }
    vehicle* vp = World.object_from_handle<vehicle>(hp->vehicle_handle);
    if (vp == NULL) {
        return false;
    }
    human* driver = vp->most_recent_driver_exit_time.valid() ? NULL : vehicle_human_from_handle(vp->most_recent_driver);
    if (!object_is_player(driver)) {
        return false;
    }
    player* local = player_get_local_player();
    if (local == NULL || human_in_vehicle_transition(local, vp, VI_INTERACTION_EXIT)) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        if (vehicle_human_from_handle(vp->vii.humans[i].handle) == hp) {
            return true;
        }
    }
    return false;
}

// 0x00B77BB0 (:5153)
human* vehicle_passenger_in_seat(unsigned int v_handle, vehicle_seat_index seat_index)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL ? vehicle_passenger_in_seat(vp, seat_index) : NULL;
}

// 0x00B77BE0 (:5166)
human* vehicle_passenger_entering_seat(const vehicle* vp, vehicle_seat_index seat_index)
{
    for (int i = 0; i < 8; i++) {
        const vi_human_manager& hm = vp->vii.humans[i];
        if (hm.handle != 0xFFFFFFFF && hm.interaction == VI_INTERACTION_ENTER && hm.enter_data.destination_seat == seat_index) {
            return vehicle_human_from_handle(hm.handle);
        }
    }
    return NULL;
}

// 0x00B77C40 (:5188)
human* vehicle_passenger_entering_seat(unsigned int v_handle, vehicle_seat_index seat_index)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL ? vehicle_passenger_entering_seat(vp, seat_index) : NULL;
}

// 0x00B77C70 (:5202)
human* vehicle_passenger_in_or_entering_seat(const vehicle* vp, vehicle_seat_index seat_index)
{
    for (int i = 0; i < 8; i++) {
        const vi_human_manager& hm = vp->vii.humans[i];
        if (hm.handle == 0xFFFFFFFF) {
            continue;
        }
        human* hp = vehicle_human_from_handle(hm.handle);
        if (hm.interaction == VI_INTERACTION_ENTER && hm.enter_data.destination_seat == seat_index) {
            return hp;
        }
        if (hp != NULL && hp->vehicle_seat_idx == seat_index) {
            return hp;
        }
    }
    return NULL;
}

// 0x00B77CE0 (:5219)
human* vehicle_passenger_in_or_entering_seat(unsigned int v_handle, vehicle_seat_index seat_index)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL ? vehicle_passenger_in_or_entering_seat(vp, seat_index) : NULL;
}

// 0x00B77D10 (:5226)
human* vehicle_passenger_in_entering_or_reserving_seat(const vehicle* vp, vehicle_seat_index seat_index)
{
    for (int i = 0; i < 8; i++) {
        const vi_human_manager& hm = vp->vii.humans[i];
        if (hm.handle == 0xFFFFFFFF) {
            continue;
        }
        human* hp = vehicle_human_from_handle(hm.handle);
        // interaction 1 reserves a seat, 2 (VI_INTERACTION_ENTER) enters it
        if ((hm.interaction == VI_INTERACTION_ENTER || (int)hm.interaction == 1) && hm.enter_data.destination_seat == seat_index) {
            return hp;
        }
        if (hp != NULL && hp->vehicle_seat_idx == seat_index) {
            return hp;
        }
    }
    return NULL;
}

// 0x00B77D80 (:5245)
human* vehicle_passenger_in_entering_or_reserving_seat(unsigned int v_handle, vehicle_seat_index seat_index)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL ? vehicle_passenger_in_entering_or_reserving_seat(vp, seat_index) : NULL;
}

// 0x00B64D80 (:5289)
vehicle_seat_index vehicle_passenger_get_seat(const vehicle* vp, const human* hp)
{
    return human_in_vehicle_interaction(hp, vp) ? hp->vehicle_seat_idx : VSI_INVALID;
}

// 0x00B64DB0 (:5299)
vehicle_seat_index vehicle_seat_name_to_index(const char* name)
{
    for (int i = 0; i < 11; i++) {
        if (v_stricmp(name, Vehicle_seat_tag_names[i]) == 0) {
            return (vehicle_seat_index)i;
        }
    }
    return VSI_INVALID;
}

// 0x00B77DB0 (:5325)
human* vehicle_get_first_passenger(const vehicle* vp, vehicle_seat_index index)
{
    const int num_seats = vehicle_get_num_seats(vp);
    for (int seat = index; seat < num_seats; seat++) {
        human* hp = vehicle_passenger_in_seat(vp, (vehicle_seat_index)seat);
        if (hp != NULL) {
            return hp;
        }
    }
    return NULL;
}

// 0x00B77DF0 (:5346)
human* vehicle_most_recent_driver(const vehicle* vp)
{
    return vehicle_human_from_handle(vp->most_recent_driver);
}

// 0x00B77E50 (:5362)
unsigned int vehicle_get_door_index_from_seat(const vehicle* vp, vehicle_seat_index seat_index)
{
    if (vp->rdd.is_loaded()) {
        const vehicle_subinfo::seat_to_door_link_info* links = vp->rdd.get_data()->sub_info->m_seat_to_door_links;
        if (links != NULL && (unsigned int)seat_index < links->m_num_seats) {
            return links->m_door_component_indices[seat_index];
        }
    }
    return 0xFFFFFFFF;
}

// 0x00B64DF0 (:5389)
const vehicle_turret_mount* vehicle_get_seat_turret_mount(const vehicle* vp, vehicle_seat_index seat_index)
{
    for (int i = 0; i < vp->m_num_turret_mounts; i++) {
        if (vp->m_turret_mounts[i].info != NULL && vp->m_turret_mounts[i].info->seat == seat_index) {
            return &vp->m_turret_mounts[i];
        }
    }
    return NULL;
}

// 0x00B64E40 (:5399)
vehicle_turret_mount* vehicle_get_seat_turret_mount(vehicle* vp, vehicle_seat_index seat_index)
{
    for (int i = 0; i < vp->m_num_turret_mounts; i++) {
        if (vp->m_turret_mounts[i].info != NULL && vp->m_turret_mounts[i].info->seat == seat_index) {
            return &vp->m_turret_mounts[i];
        }
    }
    return NULL;
}

// 0x00B64E90 (:5409)
vehicle_seat_index vehicle_get_seat_for_turret(const vehicle* vp, turret* tp)
{
    for (int i = 0; i < vp->m_num_turret_mounts; i++) {
        if (vp->m_turret_mounts[i].attached_turret == tp->get_handle()) {
            return vp->m_turret_mounts[i].info->seat;
        }
    }
    return VSI_INVALID;
}

// 0x00B8BE00 (:5421)
human* vehicle_get_human_in_gunner_seat(const vehicle* vp)
{
    for (int i = 0; i < 8; i++) {
        human* hp = vehicle_human_from_handle(vp->vii.humans[i].handle);
        if (hp == NULL) {
            continue;
        }
        const vehicle_turret_mount* mount = vehicle_get_seat_turret_mount(vp, vehicle_passenger_get_seat(vp, hp));
        if (mount != NULL && mount->attached_turret != 0xFFFFFFFF) {
            turret* tp = World.object_from_handle<turret>(mount->attached_turret);
            if (tp != NULL && tp->is_functional(false)) {
                return hp;
            }
        }
    }
    return NULL;
}

// 0x00B77E80 (:5444)
turret* vehicle_get_seat_turret(const vehicle* vp, vehicle_seat_index seat_index)
{
    const vehicle_turret_mount* mount = vehicle_get_seat_turret_mount(vp, seat_index);
    return (mount != NULL && mount->attached_turret != 0xFFFFFFFF) ? World.object_from_handle<turret>(mount->attached_turret) : NULL;
}

// 0x00B64ED0 (:5453)
vehicle_turret_mount* vehicle_get_turret_mount(vehicle* vp, int index)
{
    return (-1 < index && index < vp->m_num_turret_mounts) ? &vp->m_turret_mounts[index] : NULL;
}

// 0x00B77EC0 (:5463)
bool vehicle_all_turrets_locked(vehicle* vp)
{
    for (int i = 0; i < vp->m_num_turret_mounts; i++) {
        if (vp->m_turret_mounts[i].attached_turret != 0xFFFFFFFF) {
            turret* tp = World.object_from_handle<turret>(vp->m_turret_mounts[i].attached_turret);
            if (tp != NULL && !tp->turret_flags.locked_turret) {
                return false;
            }
        }
    }
    return true;
}

// 0x00BAA660 (:5545)
void vehicle_open_door(vehicle* vp, unsigned int door_index, float open_duration)
{
    if (vp->rdd.is_loaded() && door_index < vp->rdd.get_data()->m_num_components) {
        vehicle_component_open_door(&vp->rdd.get_data()->m_components[door_index], vp, open_duration);
    }
}

// 0x00BAA6A0 (:5568)
void vehicle_close_door(vehicle* vp, unsigned int door_index, float close_duration)
{
    if (vp->rdd.is_loaded() && door_index < vp->rdd.get_data()->m_num_components) {
        vehicle_component_close_door(&vp->rdd.get_data()->m_components[door_index], vp, close_duration);
    }
}

// 0x00B77F30 (:5591)
bool vehicle_door_is_fully_open(const vehicle* vp, unsigned int door_index)
{
    return vp->rdd.is_loaded() && door_index < vp->rdd.get_data()->m_num_components && 0.97f < vp->rdd.get_data()->m_components[door_index].m_type_attribute.door.m_t_actual;
}

// 0x00B77F80 (:5614)
bool vehicle_door_is_fully_closed(const vehicle* vp, unsigned int door_index)
{
    return vp->rdd.is_loaded() && door_index < vp->rdd.get_data()->m_num_components && vp->rdd.get_data()->m_components[door_index].m_type_attribute.door.m_t_actual < 0.03f;
}

// 0x00B77FD0 (:5639)
float vehicle_get_door_open_pct(const vehicle* vp, unsigned int door_index)
{
    if (vp->rdd.is_loaded() && door_index < vp->rdd.get_data()->m_num_components) {
        const vehicle_component& component = vp->rdd.get_data()->m_components[door_index];
        if (component.get_data()->m_type == VCT_DOOR) {
            return (component.get_flags() & VCF_DETACHED) != 0 ? 1.0f : component.m_type_attribute.door.m_t_actual;
        }
    }
    return 1.0f;
}

// 0x00B78040 (:5669)
void vehicle_check_door_for_seat(vehicle* vp, vehicle_seat_index seat_index, bool& door_exists, bool& door_is_attached)
{
    door_exists = false;
    door_is_attached = false;
    const unsigned int door = vehicle_get_door_index_from_seat(vp, seat_index); // inlined in rfg.exe
    if (door != 0xFFFFFFFF) {
        door_exists = true;
        const unsigned int flags = vp->rdd.get_data()->m_components[door].get_flags();
        // bit 4: it is a door that can latch
        door_is_attached = (flags & 4) != 0 && (flags & VCF_DETACHED) == 0;
    }
}

// 0x00B8BEE0 (:5700)
int vehicle_get_type(const vehicle* vehicle_ptr)
{
    return vehicle_info_get_type(vehicle_ptr->info);
}

// 0x00B8BF20 (:5711)
float vehicle_get_real_value(const vehicle* vehicle_ptr)
{
    const float health = (float)(unsigned int)vehicle_ptr->m_damage_areas[0].m_hit_points / (float)(unsigned int)vehicle_ptr->m_damage_areas[0].m_max_hit_points;
    return Vehicle_info_pool[vehicle_info_get_type(vehicle_ptr->info)].m_value * health;
}

// 0x00B64EF0 (:5721)
void vehicle_apply_linear_velocity(vehicle* target, const vector* linear_velocity)
{
    const float mass = target->get_mass();
    const vector scaled(mass * linear_velocity->x, linear_velocity->y * mass, mass * linear_velocity->z);
    hkVector4f impulse;
    game_to_havok_vector(impulse, scaled);
    hkpRigidBody* body = havok_body_get_pointer(target->havok_handle);
    body->activate();
    body->applyLinearImpulse(impulse);
}

// 0x00B64F90 (:5736)
void vehicle_apply_angular_impulse(vehicle* target, const vector* angular_impulse)
{
    const float mass = target->get_mass();
    const vector scaled(mass * angular_impulse->x, angular_impulse->y * mass, mass * angular_impulse->z);
    hkVector4f impulse;
    game_to_havok_vector(impulse, scaled);
    hkpRigidBody* body = havok_body_get_pointer(target->havok_handle);
    body->activate();
    body->applyAngularImpulse(impulse);
}

// 0x00B65030 (:5758)
void vehicle_apply_point_velocity(vehicle* vp, const vector* velocity, const vector* position)
{
    const float mass = vp->get_mass();
    const vector scaled(mass * velocity->x, velocity->y * mass, mass * velocity->z);
    hkVector4f impulse;
    hkVector4f point;
    game_to_havok_vector(impulse, scaled);
    game_to_havok_vector(point, *position);
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    body->activate();
    body->applyPointImpulse(impulse, point);
}

// 0x00BB6190 (:5777)
void vehicle::start_nano_rifle_fade_out(bool death_full_fade, human* attacker, vector world_hit_pos)
{
    const int effect_id = material_effects_get_nano_rifle_matfx_id(death_full_fade);
    if (!rdd.is_loaded()) {
        return;
    }
    resource_dependent_data* data = rdd.get_data();
    material_effect_parameters params = material_effects_get_parameters(1, 2000);
    params.world_pos = world_hit_pos;
    params.use_local_pos = true;
    material_effects_apply(data->m_renderable_instance_p, effect_id, &params);
    for (int i = 0; i < (int)data->m_num_components; i++) {
        material_effects_apply(data->m_components[i].m_renderable_instance_p, effect_id, &params);
    }
    for (int i = 0; i < 8; i++) {
        human* hp = vehicle_human_from_handle(vii.humans[i].handle);
        if (hp != NULL && death_full_fade) {
            if (human_can_quick_exit_vehicle(hp)) {
                vehicle_interaction_request_exit(hp, NULL);
            }
            else {
                hp->start_nano_rifle_fade_out(true, attacker, true, ZERO_VECTOR);
            }
        }
    }
    for (int i = 0; i < m_num_turret_mounts; i++) {
        if (m_turret_mounts[i].attached_turret == 0xFFFFFFFF) {
            continue;
        }
        turret* tp = World.object_from_handle<turret>(m_turret_mounts[i].attached_turret);
        if (tp == NULL) {
            continue;
        }
        weapon* wp = tp->get_weapon_inst();
        if (wp != NULL && wp->char_inst != NULL && wp->char_inst->rdd.is_loaded()) {
            material_effects_apply(wp->char_inst->rdd.get_data()->renderable_instance_p, effect_id, &params);
        }
        wp = tp->get_secondary_weapon_inst();
        if (wp != NULL && wp->char_inst != NULL && wp->char_inst->rdd.is_loaded()) {
            material_effects_apply(wp->char_inst->rdd.get_data()->renderable_instance_p, effect_id, &params);
        }
    }
    if (death_full_fade) {
        vehicle_fade_out(this, 2000, true, true);
        havok_body_set_collision_type(havok_body_get_pointer(havok_handle), COLLIDE_NO_COLLISION, true, false);
    }
}

// 0x00BC27A0 (:5834)
void vehicle::process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info)
{
    vector direction(fire_info.hit_pos.x - fire_info.start_pos.x, fire_info.hit_pos.y - fire_info.start_pos.y, fire_info.hit_pos.z - fire_info.start_pos.z);
    direction = direction.copy_normalize_safe();
    vector damage_velocity(direction.x * 100.0f, direction.y * 100.0f, direction.z * 100.0f);
    if (!Multiplayer || multi_is_server()) {
        vehicle_apply_damage(this, attacker, fire_info.w_info, DBT_BULLET, &fire_info.hit_pos, &fire_info.hit_normal, 0.2f, (int)damage, &damage_velocity, false);
    }
    if (fire_info.w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE) {
        start_nano_rifle_fade_out(is_killed(), attacker, fire_info.hit_pos);
    }
    player* local = player_get_local_player();
    if (local != NULL && human_in_vehicle(local, this)) {
        human_combat_process_hit_indicator(local, NULL, &direction, damage);
    }
}

// 0x00BC2920 (:5873)
bool vehicle::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info)
{
    if (!vehicle_apply_damage(this, attacker, &w_info, DBT_MELEE, &hit_pos, &hit_normal, 0.5f, fl_round(damage), NULL, false)) {
        return false;
    }
    const vector scaled(impulse.x * 10.0f, impulse.y * 10.0f, impulse.z * 10.0f);
    hkVector4f hk_impulse;
    hkVector4f hk_point;
    game_to_havok_vector(hk_impulse, scaled);
    game_to_havok_vector(hk_point, hit_pos);
    hkpRigidBody* body = havok_body_get_pointer(havok_handle);
    if (body != NULL) {
        body->activate();
        if (extra_mass) {
            hk_impulse.mul(1.0f / extra_mass_value);
        }
        if (!infinite_mass) {
            body->applyPointImpulse(hk_impulse, hk_point);
        }
        player* local = player_get_local_player();
        if (local != NULL && human_in_vehicle(local, this)) {
            human_combat_process_hit_indicator(local, NULL, &impulse, damage);
        }
    }
    return false; // the original always answers false
}

// 0x00BC2AD0 (:5916)
void vehicle_process_explosion_hit(vehicle* target, human* attacker, object* source, const vector& explosion_pos, float radius, unsigned int damage, const vector* linear_impulse, const vector* angular_impulse, const weapon_info* w_info, const explosion_info* p_explosion_info)
{
    // the player's convoy car does not take its own convoy's rockets in the rail shooter
    if (activity_vrail_is_riding_shotgun_running() && vehicle_player_is_occupant(target) && source != NULL && source->get_obj_type() == OT_VEHICLE) {
        return;
    }
    if (p_explosion_info != NULL && (p_explosion_info->flags & 2) != 0) {
        vehicle_blow_up(target, attacker, false, w_info, false);
        return;
    }
    hkVector4f hk_linear;
    hkVector4f hk_angular;
    game_to_havok_vector(hk_linear, *linear_impulse);
    game_to_havok_vector(hk_angular, *angular_impulse);
    // no more downward push than its weight (10 m/s^2)
    const float min_y = target->get_mass() * -10.0f;
    if (hk_linear(1) - min_y < 0.0f) {
        hk_linear(1) = min_y;
    }
    hkpRigidBody* body = havok_body_get_pointer(target->havok_handle);
    if (body == NULL) {
        return;
    }
    body->activate();
    if (target->get_sub_type() == OT_SUB_VEHICLE_FLYER) {
        hk_linear.mul(hkSimdFloat32::fromFloat(flyer_explosion_linear_impulse_scalar));
        hk_angular.mul(hkSimdFloat32::fromFloat(flyer_explosion_angular_impulse_scalar));
    }
    if (target->get_extra_mass()) {
        const float scale = 1.0f / target->get_extra_mass_value();
        hk_linear.mul(scale);
        hk_angular.mul(scale);
    }
    if (!target->get_infinite_mass()) {
        body->applyLinearImpulse(hk_linear);
        body->applyAngularImpulse(hk_angular);
    }
    const float mass = target->info->chassis_mass;
    if (mass <= 0.0f) {
        return;
    }
    vector damage_velocity(linear_impulse->x / mass, linear_impulse->y / mass, linear_impulse->z / mass);
    bool player_inside = false;
    int remaining = target->vii.num_humans;
    for (int i = 0; 0 < remaining && i < 8; i++) {
        const unsigned int handle = target->vii.humans[i].handle;
        if (handle == 0xFFFFFFFF) {
            continue;
        }
        remaining--;
        human* hp = World.object_from_handle<human>(handle);
        if (object_is_local_player(hp)) {
            human_combat_process_hit_indicator(hp, NULL, linear_impulse, (float)damage);
        }
        if (object_is_player(hp)) {
            player_inside = true;
        }
    }
    if (player_inside) {
        const float scale = p_explosion_info != NULL ? p_explosion_info->player_veh_damage_mult : 1.0f;
        damage = (unsigned int)(long long)((double)damage * (double)scale); // x87 product in rfg.exe, truncated
    }
    // The human attacker, not the source: the source is the projectile (usually freed by now) or the exploding object.
    vehicle_apply_damage(target, attacker, w_info, DBT_EXPLOSION, &explosion_pos, NULL, radius, damage, &damage_velocity, false);
}

// 0x00B650E0 (:6117)
bool vehicle_is_corpse(const vehicle* vp)
{
    return vp->is_killed();
}

// 0x00B650F0 (:6128)
bool vehicle_is_on_fire(const vehicle* vehicle_ptr)
{
    return vehicle_ptr->m_flags.m_on_fire != 0;
}

// 0x00B780B0 (:6139)
bool vehicle_is_on_fire(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL && vp->m_flags.m_on_fire != 0;
}

// 0x00B65110 (:6155)
bool vehicle_is_hidden(const vehicle* vp)
{
    return vp->m_flags.m_hidden != 0;
}

// 0x00B65130 (:6166)
bool vehicle_human_in_vehicle(const vehicle* vp, const human* hp)
{
    return vp != NULL && hp != NULL && human_in_vehicle(hp, vp->get_handle());
}

// 0x00B65160 (:6181)
bool vehicle_player_in_vehicle(const vehicle* vp)
{
    return vehicle_human_in_vehicle(vp, player_get_local_player());
}

// 0x00B65190 (:6196)
bool vehicle_is_player_car(const vehicle* vehicle_ptr)
{
    return vehicle_ptr->m_flags.m_is_player_car != 0;
}

// 0x00B780E0 (:6219)
bool vehicle_has_gunner_seat(const vehicle* vp)
{
    for (int i = 0; i < vp->m_num_turret_mounts; i++) {
        if (vp->m_turret_mounts[i].attached_turret != 0xFFFFFFFF) {
            turret* tp = World.object_from_handle<turret>(vp->m_turret_mounts[i].attached_turret);
            if (tp != NULL && tp->is_functional(false)) {
                return true;
            }
        }
    }
    return false;
}

// 0x00B651B0 (:6245)
void vehicle_set_player_disabled(vehicle* vp, bool disabled)
{
    vp->m_flags.disable_for_player = disabled;
}

// 0x00B651E0 (:6254)
void vehicle_set_accelerator_disabled(vehicle* vp, bool disabled)
{
    vp->m_flags.disable_accelerator = disabled;
}

// 0x00B65210 (:6265)
void vehicle_get_center_of_mass_pos_orient(const vehicle* vp, vector& com_position, matrix* com_orient)
{
    havok_to_game_vector(com_position, havok_body_get_pointer(vp->havok_handle)->getCenterOfMassInWorld());
    if (com_orient != NULL) {
        *com_orient = vp->orient;
    }
}

// 0x00B652A0 (:6284)
bool vehicle_is_vehicle_at_end_of_line(vehicle* vp)
{
    return vehicle_ai_is_at_end_of_line(&vp->vai);
}

// 0x00B652C0 (:6294)
void vehicle_fade_in(vehicle* vp, int time)
{
    vp->m_flags.m_fading_in = 1;
    vp->m_flags.m_fading_from_nano = 0;
    vp->m_flags.m_fading_out = 0;
    vp->m_fade_timer.set(0);
    vp->m_fade_time = time;
}

// Fades a piece of debris that came off this vehicle.
static void vehicle_fade_debris_object(const vehicle* vp, unsigned int handle, int time)
{
    if (handle != 0xFFFFFFFF) {
        object_debris* debris = World.object_from_handle<object_debris>(handle);
        if (debris != NULL && debris->m_create_info.m_vehicle_handle == vp->get_handle()) {
            object_debris_mark_for_fade(debris, 0, time, false);
        }
    }
}

// 0x00B78140 (:6309)
void vehicle_fade_out(vehicle* vp, int time, bool despawn_after_fade, bool fading_from_nano)
{
    if (despawn_after_fade ? vp->m_flags.m_despawn_after_fade != 0 : vp->m_flags.m_fading_out != 0) {
        return;
    }
    if (time < 0) {
        time = 2000;
    }
    vp->m_flags.m_fading_in = 0;
    vp->m_flags.m_despawn_after_fade = despawn_after_fade;
    vp->m_flags.m_fading_out = 1;
    vp->m_flags.m_fading_from_nano = fading_from_nano;
    vp->m_fade_timer.set(0);
    vp->m_fade_time = time;
    if (vp->rdd.is_loaded()) {
        vehicle::resource_dependent_data* data = vp->rdd.get_data();
        for (unsigned int i = 0; i < data->m_num_components; i++) {
            vehicle_fade_debris_object(vp, data->m_components[i].m_debris_object, time);
        }
        for (int i = 0; i < 80; i++) {
            vehicle_fade_debris_object(vp, data->spline_debris[i], time);
        }
    }
    if (vp->m_corpse_effect != 0xFFFFFFFF) {
        object_effect* effect = World.object_from_handle<object_effect>(vp->m_corpse_effect);
        if (effect != NULL) {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
        vp->m_corpse_effect = 0xFFFFFFFF;
    }
    for (int i = 0; i < 4; i++) {
        if (vp->m_exhaust_effect_normal[i] != 0xFFFFFFFF) {
            object_effect* effect = World.object_from_handle<object_effect>(vp->m_exhaust_effect_normal[i]);
            if (effect != NULL) {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
            vp->m_exhaust_effect_normal[i] = 0xFFFFFFFF;
        }
        if (vp->m_exhaust_effect_burst[i] != 0xFFFFFFFF) {
            object_effect* effect = World.object_from_handle<object_effect>(vp->m_exhaust_effect_burst[i]);
            if (effect != NULL) {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
            vp->m_exhaust_effect_burst[i] = 0xFFFFFFFF;
        }
    }
}

// 0x00B65300 (:6401)
float vehicle::get_vehicle_damage() const
{
    const int max_hit_points = m_damage_areas[0].m_max_hit_points;
    float damage = (float)(max_hit_points - m_damage_areas[0].m_hit_points) / (float)max_hit_points;
    if (damage - 0.0f < 0.0f) {
        damage = 0.0f;
    }
    if (0.0f <= damage - 1.0f) {
        damage = 1.0f;
    }
    return damage;
}

// 0x00B65380 (:6414)
float vehicle::get_reported_vehicle_damage() const
{
    return last_damage_reported;
}

// 0x00B65390 (:6420)
void vehicle::update_last_damage_reported()
{
    last_damage_reported = get_vehicle_damage();
}

// 0x00B78300 (:6426)
int vehicle::get_num_tags() const
{
    if (!rdd.is_loaded()) {
        return -1;
    }
    const resource_dependent_data* data = rdd.get_data();
    const vehicle_subinfo* sub_info = data->sub_info;
    int count = sub_info->m_num_components + sub_info->m_chassis_tags.get_num_tags();
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        count += sub_info->m_components[i].m_tags.get_num_tags();
    }
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        const vehicle_mesh_light* light = data->m_vehicle_lights[i].mesh_light;
        if (light != NULL) {
            count += 1 + light->on_mesh_tags.get_num_tags() + light->off_mesh_tags.get_num_tags();
        }
    }
    return count;
}

// Tag index layout: high byte = where it is (0 chassis tag, 1 component itself, 2 component's tag, 3 light itself,
// 4 light's off tag, 5 light's on tag), next byte = which component or light, low word = the tag.
static const int VEHICLE_TAG_COMPONENT = 0x100;
static const int VEHICLE_TAG_COMPONENT_TAG = 0x200;
static const int VEHICLE_TAG_LIGHT = 0x300;
static const int VEHICLE_TAG_LIGHT_OFF = 0x400;
static const int VEHICLE_TAG_LIGHT_ON = 0x500;

// 0x00B8BFD0 (:6460)
int vehicle::get_tag_index_from_name(const char* tag_name) const
{
    if (tag_name == NULL || !rdd.is_loaded()) {
        return -1;
    }
    const resource_dependent_data* data = rdd.get_data();
    const int chassis_tag = data->sub_info->m_chassis_tags.get_tag(tag_name);
    if (0 <= chassis_tag) {
        return chassis_tag;
    }
    const int component = get_component_index_from_name(tag_name, 0);
    if (-1 < component) {
        return ((component & 0xFF) | VEHICLE_TAG_COMPONENT) << 16;
    }
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        const int tag = data->sub_info->m_components[i].m_tags.get_tag(tag_name);
        if (-1 < tag) {
            return ((i & 0xFF) | VEHICLE_TAG_COMPONENT_TAG) << 16 | tag;
        }
    }
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        if (data->m_vehicle_lights[i].mesh_light != NULL) {
            const int tag = data->m_vehicle_lights[i].mesh_light->off_mesh_tags.get_tag(tag_name);
            if (-1 < tag) {
                return ((i & 0xFF) | VEHICLE_TAG_LIGHT_OFF) << 16 | tag;
            }
        }
    }
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        if (data->m_vehicle_lights[i].mesh_light != NULL) {
            const int tag = data->m_vehicle_lights[i].mesh_light->on_mesh_tags.get_tag(tag_name);
            if (-1 < tag) {
                return ((i & 0xFF) | VEHICLE_TAG_LIGHT_ON) << 16 | tag;
            }
        }
    }
    return -1;
}

// 0x00B783B0 (:6521)
int vehicle::get_component_tag_index_from_name(int component_idx, const char* tag_name) const
{
    if (!rdd.is_loaded()) {
        return -1;
    }
    const resource_dependent_data* data = rdd.get_data();
    const int tag = data->sub_info->m_components[component_idx].m_tags.get_tag(tag_name);
    if (0 <= tag) {
        return ((component_idx & 0xFF) | VEHICLE_TAG_COMPONENT_TAG) << 16 | tag;
    }
    const vehicle_component_data* component = data->m_components[component_idx].get_data();
    for (unsigned int i = 0; i < component->m_num_children; i++) {
        const int child_tag = get_component_tag_index_from_name(component->m_children_indices[i], tag_name);
        if (-1 < child_tag) {
            return child_tag;
        }
    }
    return -1;
}

// Counts down *remaining through a tag list's matches; true when the wanted one is found.
static bool vehicle_count_tag_matches(const mesh_tags& tags, const char* tag_name, int& remaining, int& found)
{
    for (int nth = 1; 0 < remaining; nth++) {
        found = tags.get_nth_tag(tag_name, nth);
        if (found == -1) {
            break;
        }
        remaining--;
    }
    return remaining == 0;
}

// 0x00B78450 (:6548)
int vehicle::get_nth_tag_index_from_name(const char* tag_name, int num_tag) const
{
    if (!rdd.is_loaded()) {
        return -1;
    }
    const resource_dependent_data* data = rdd.get_data();
    int remaining = num_tag;
    int found = -1;
    if (vehicle_count_tag_matches(data->sub_info->m_chassis_tags, tag_name, remaining, found)) {
        return found;
    }
    for (unsigned int i = 0; i < data->sub_info->m_num_components; i++) {
        if (vehicle_count_tag_matches(data->sub_info->m_components[i].m_tags, tag_name, remaining, found)) {
            return ((i & 0xFF) | VEHICLE_TAG_COMPONENT_TAG) << 16 | found;
        }
    }
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        const vehicle_mesh_light* light = data->m_vehicle_lights[i].mesh_light;
        if (light != NULL && vehicle_count_tag_matches(light->off_mesh_tags, tag_name, remaining, found)) {
            return ((i & 0xFF) | VEHICLE_TAG_LIGHT_OFF) << 16 | found;
        }
    }
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        const vehicle_mesh_light* light = data->m_vehicle_lights[i].mesh_light;
        if (light != NULL && vehicle_count_tag_matches(light->on_mesh_tags, tag_name, remaining, found)) {
            return ((i & 0xFF) | VEHICLE_TAG_LIGHT_ON) << 16 | found;
        }
    }
    return -1;
}

// 0x00B78630 (:6667)
int vehicle::get_nth_tag_index(int index) const
{
    if (!rdd.is_loaded()) {
        return -1;
    }
    const resource_dependent_data* data = rdd.get_data();
    const vehicle_subinfo* sub_info = data->sub_info;
    if (index < sub_info->m_chassis_tags.get_num_tags()) {
        return index;
    }
    int rest = index - sub_info->m_chassis_tags.get_num_tags();
    if (rest < (int)data->m_num_components) {
        return ((rest & 0xFF) | VEHICLE_TAG_COMPONENT) << 16;
    }
    rest -= data->m_num_components;
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        if (rest < sub_info->m_components[i].m_tags.get_num_tags()) {
            return ((i & 0xFF) | VEHICLE_TAG_COMPONENT_TAG) << 16 | rest;
        }
        rest -= sub_info->m_components[i].m_tags.get_num_tags();
    }
    int lights[20];
    int num_lights = 0;
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        if (data->m_vehicle_lights[i].mesh_light != NULL) {
            lights[num_lights++] = i;
        }
    }
    if (rest < num_lights) {
        return ((lights[rest] & 0xFF) | VEHICLE_TAG_LIGHT) << 16;
    }
    rest -= num_lights;
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        const vehicle_mesh_light* light = data->m_vehicle_lights[i].mesh_light;
        if (light != NULL) {
            if (rest < light->off_mesh_tags.get_num_tags()) {
                return ((i & 0xFF) | VEHICLE_TAG_LIGHT_OFF) << 16 | rest;
            }
            rest -= light->off_mesh_tags.get_num_tags();
        }
    }
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        const vehicle_mesh_light* light = data->m_vehicle_lights[i].mesh_light;
        if (light != NULL) {
            if (rest < light->on_mesh_tags.get_num_tags()) {
                return ((i & 0xFF) | VEHICLE_TAG_LIGHT_ON) << 16 | rest;
            }
            rest -= light->on_mesh_tags.get_num_tags();
        }
    }
    return -1;
}

// 0x00B78800 (:6760)
int vehicle::get_component_index_from_name(const char* comp_name, int start_from) const
{
    if (!rdd.is_loaded()) {
        return -1;
    }
    const resource_dependent_data* data = rdd.get_data();
    const unsigned int hash = str_hash_string(comp_name);
    for (unsigned int i = start_from; i < data->m_num_components; i++) {
        if (str_hash_string(data->m_components[i].get_data()->bone_name) == hash) {
            return (int)i;
        }
    }
    return -1;
}

// 0x00B65410 (:6781)
int vehicle::get_component_index_from_tag_index(int tag_index) const
{
    const unsigned int where = tag_index & 0xFF000000;
    return (where == 0x1000000 || where == 0x2000000) ? (tag_index >> 16) & 0xFF : -1;
}

// 0x00B65440 (:6796)
int vehicle::get_light_index_from_tag_index(int tag_index) const
{
    const unsigned int where = tag_index & 0xFF000000;
    return (where == 0x3000000 || where == 0x4000000 || where == 0x5000000) ? (tag_index >> 16) & 0xFF : -1;
}

// Brings light-space coordinates into chassis space through the component a light is on (when any).
static void vehicle_tag_through_light_parent(const vehicle::resource_dependent_data* data, const vehicle_light& light, vector& tag_pos, matrix* tag_orient)
{
    if (light.parent_idx == -1) {
        return;
    }
    const vehicle_component& parent = data->m_components[light.parent_idx];
    tag_pos.unrotate(parent.get_chassis_space_rotation());
    tag_pos.x += parent.get_chassis_space_position().x;
    tag_pos.y += parent.get_chassis_space_position().y;
    tag_pos.z += parent.get_chassis_space_position().z;
    if (tag_orient != NULL) {
        tag_orient->unrotate(parent.get_chassis_space_rotation());
    }
}

// 0x00B78880 (:6822)
bool vehicle::get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const
{
    tag_pos.zero();
    if (tag_orient != NULL) {
        tag_orient->make_identity();
    }
    if (!rdd.is_loaded()) {
        return false;
    }
    const resource_dependent_data* data = rdd.get_data();
    const int tag = tag_index & 0xFFFF;
    const unsigned int where = tag_index & 0xFF000000;
    const int which = (tag_index >> 16) & 0xFF;
    matrix43 transform;
    switch (where) {
    case 0: {
        data->sub_info->m_chassis_tags.get_tag_transform(transform, tag);
        tag_pos = transform.m_translation;
        if (tag_orient != NULL) {
            *tag_orient = transform.m_rotation;
        }
        if (!Vehicle_deformation_enabled || m_tag_deform_weights[tag] == 0) {
            return true;
        }
        // Original bug kept: the weight is read at the whole tag index, which here equals the tag.
        const float weight = mesh_deform_weight_convert(m_tag_deform_weights[tag_index]);
        const vehicle_subinfo::mesh_tag_deformation_offset& offset = data->sub_info->m_mesh_tag_deformation_offsets[tag];
        tag_pos.x += offset.m_position_offset.x * weight;
        tag_pos.y += offset.m_position_offset.y * weight;
        tag_pos.z += offset.m_position_offset.z * weight;
        if (tag_orient != NULL) {
            quaternion rotation;
            quat_lerp(rotation, IDENTITY_QUATERNION, offset.m_rotation_offset, weight);
            matrix rotation_matrix;
            rotation.extract_matrix(rotation_matrix);
            *tag_orient = *tag_orient * rotation_matrix;
        }
        return true;
    }
    case 0x1000000: {
        const vehicle_component& component = data->m_components[which];
        tag_pos = component.get_chassis_space_position();
        if (tag_orient != NULL) {
            *tag_orient = component.get_chassis_space_rotation();
        }
        return true;
    }
    case 0x2000000: {
        const vehicle_component& component = data->m_components[which];
        data->sub_info->m_components[which].m_tags.get_tag_transform(transform, tag);
        tag_pos = transform.m_translation;
        tag_pos.unrotate(component.get_chassis_space_rotation());
        tag_pos.x += component.get_chassis_space_position().x;
        tag_pos.y += component.get_chassis_space_position().y;
        tag_pos.z += component.get_chassis_space_position().z;
        if (tag_orient != NULL) {
            *tag_orient = transform.m_rotation;
            tag_orient->unrotate(component.get_chassis_space_rotation());
        }
        return true;
    }
    case 0x3000000: {
        const vehicle_light& light = data->m_vehicle_lights[which];
        if (light.tag_index != -1) {
            get_tag_local_coords(light.tag_index, tag_pos, tag_orient);
        }
        if (light.mesh_light == NULL) {
            return true;
        }
        tag_pos = light.mesh_light->position;
        if (tag_orient != NULL) {
            *tag_orient = light.mesh_light->rotation;
        }
        vehicle_tag_through_light_parent(data, light, tag_pos, tag_orient);
        return true;
    }
    case 0x4000000:
    case 0x5000000: {
        const vehicle_light& light = data->m_vehicle_lights[which];
        const vehicle_mesh_light* mesh_light = light.mesh_light;
        (where == 0x4000000 ? mesh_light->off_mesh_tags : mesh_light->on_mesh_tags).get_tag_transform(transform, tag);
        tag_pos = transform.m_translation;
        if (tag_orient != NULL) {
            *tag_orient = transform.m_rotation;
        }
        tag_pos.unrotate(mesh_light->rotation);
        tag_pos.x += mesh_light->position.x;
        tag_pos.y += mesh_light->position.y;
        tag_pos.z += mesh_light->position.z;
        if (tag_orient != NULL) {
            tag_orient->unrotate(mesh_light->rotation);
        }
        vehicle_tag_through_light_parent(data, light, tag_pos, tag_orient);
        return true;
    }
    default:
        return false;
    }
}

// 0x00B65480 (:7008)
bool vehicle::get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const
{
    const vector& base_pos = obj_pos != NULL ? *obj_pos : pos;
    const matrix& base_orient = obj_orient != NULL ? *obj_orient : orient;
    const bool result = get_tag_local_coords(tag_index, tag_pos, tag_orient);
    if (tag_orient != NULL) {
        *tag_orient = *tag_orient * base_orient;
    }
    tag_pos.unrotate(base_orient);
    tag_pos.x += base_pos.x;
    tag_pos.y += base_pos.y;
    tag_pos.z += base_pos.z;
    return result;
}

// 0x00B79040 (:7039)
void vehicle_hide(unsigned int v_handle, bool hide)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return;
    }
    // m_rdd_flags.m_hide_vehicle: a hide asked for before the data was in; otherwise only a change does anything
    if (!vp->m_rdd_flags.m_hide_vehicle && (vp->m_flags.m_hidden != 0) == hide) {
        return;
    }
    vp->engine_stop(false, false);
    vp->m_flags.m_hidden = hide;
    vp->set_is_visible(!hide);
    if (!vp->rdd.is_loaded()) {
        vp->m_rdd_flags.m_hide_vehicle = hide;
        return;
    }
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    havok_body_enable_collision(body, &vp->rdd.get_data()->m_saved_collision_filter, !hide);
    body->setMotionType(hide ? hkpMotion::MOTION_KEYFRAMED : hkpMotion::MOTION_DYNAMIC, HK_ENTITY_ACTIVATION_DO_ACTIVATE, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK);
    const hkVector4f zero(0.0f, 0.0f, 0.0f, 0.0f);
    body->setLinearVelocity(zero);
    body->setAngularVelocity(zero);
}

// 0x00B791B0 (:7095)
void vehicle_hide_all_passengers(unsigned int v_handle, bool hide)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return;
    }
    for (int i = 0; i < 8; i++) {
        human* hp = vehicle_human_from_handle(vp->vii.humans[i].handle);
        if (hp != NULL) {
            human_hide(hp, hide);
        }
    }
}

// 0x00B975F0 (:7116)
bool vehicle_is_flipped(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL && vehicle_is_flipped(vp);
}

// 0x00B8C120 (:7125)
bool vehicle_is_flipped(const vehicle* vp)
{
    float speed;
    vehicle_get_speed(vp, speed);
    return speed <= 1.0f && vp->orient.uvec.y < 0.2f;
}

// 0x00B8C160 (:7144)
bool vehicle_is_usable(const human* hp, const vi_enter_data* parameters, const vehicle* vp, const bool ignore_measurements, const bool allow_flipped)
{
    const bool is_player = object_is_player(hp);
    if (vp == NULL) {
        return false;
    }
    if (vehicle_human_is_riding_with_non_exiting_player(hp)) {
        return true;
    }
    const bool working = (vp->m_flags.m_corpse == 0 && vp->m_flags.m_on_fire == 0 && (!vehicle_is_flipped(vp) || allow_flipped)) || vp->m_flags.m_invulnerable != 0;
    if (!working || vp->obj_flags.destroyed || 1.0f <= vp->m_electrical_damage_percent) {
        return false;
    }
    // a walker being carried cannot be entered
    if ((vp->get_sub_type() == OT_SUB_VEHICLE_WALKER && vp->get_parent() != 0xFFFFFFFF) || vehicle_get_num_seats(vp) < 1) {
        return false;
    }
    if (ignore_measurements || (parameters != NULL && parameters->flags.teleport != 0)) {
        return true;
    }
    if (vehicle_is_flipped(vp) && !allow_flipped) {
        return false;
    }
    if (hp != NULL && !human_in_vehicle(hp, vp) && vp->info->vehicle_classification == BVT_ART_TANK && 1.6899998f < vec_xz_dist_squared(hp->pos, vp->pos)) {
        // beside an artillery tank (beyond 1.3 m): only while the gun points along the hull, within 45 degrees
        turret* tp = vehicle_get_seat_turret(vp, VSI_DRIVER);
        if (tp == NULL || tp->orient.fvec * vp->orient.fvec < 0.70710677f) {
            return false;
        }
    }
    if (!is_player) {
        return true;
    }
    if (vp->m_flags.no_player_use != 0 || !vp->rdd.is_loaded() || vp->m_flags.m_hidden != 0 || vp->m_flags.disable_for_player != 0) {
        return false;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        const float speed = (float)((double)automobile_get_mph((automobile*)vp) * 0.44703999161720276); // mph to m/s
        if (hp->h_info->get_max_speed() < speed) {
            return false;
        }
        if (!automobile_any_wheel_on_ground((automobile*)vp) && (!allow_flipped || !vehicle_is_flipped(vp))) {
            return false;
        }
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER) {
        return false;
    }
    int remaining = vp->vii.num_humans;
    for (int i = 0; 0 < remaining && i < 8; i++) {
        const vi_human_manager& hm = vp->vii.humans[i];
        if (hm.handle == 0xFFFFFFFF) {
            continue;
        }
        remaining--;
        human* occupant = World.object_from_handle<human>(hm.handle);
        if (occupant == NULL || hm.last_path_idx == -1) {
            continue;
        }
        // Original quirk kept: these read npc fields of any occupant, the player's memory too (rfg.exe tests no subtype).
        if (human_teams_are_unfriendly(occupant, hp) && ((npc*)occupant)->npc_flags.tied_up == 0) {
            return false;
        }
        if ((hm.seat == VSI_DRIVER || hm.enter_data.destination_seat == VSI_DRIVER) && occupant->get_sub_type() == OT_SUB_HUMAN_NPC) {
            const gfm_orders_types orders = ((npc*)occupant)->get_ai()->get_orders_type();
            // orders 7 and 8 do not hold on to the vehicle
            const bool no_vehicle_orders = orders == GFM_ORDERS_TYPE_NONE || (6 < (int)orders && (int)orders < 9);
            if (!no_vehicle_orders && ((npc*)occupant)->npc_flags.activity_driver_vr == 0) {
                return false;
            }
        }
    }
    const vehicle_interaction_info* vi_info = vp->info->vi_info;
    if (vi_info == NULL || !vi_info->player_prompt_only_at_entry) {
        if (hp->ground_obj_handle == vp->get_handle()) {
            return true;
        }
        vector center;
        hp->get_center_pos(center);
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        bmax.y += 1.14f; // about a human's height above the roof
        vec_change_to_local_coords(center, vp->pos, vp->orient);
        return bbox_find_distance_squared(bmin, bmax, center) <= 16.0f; // 4 m
    }
    for (int i = 0; i < vi_info->num_waypoints; i++) {
        if (vi_info->waypoints[i].node_type == 2) { // an entry node, within 1.6 m
            vector waypoint_pos;
            vi_info->get_waypoint_world_pos_orient(vp, i, waypoint_pos, NULL, UP_VECTOR);
            if (vec_dist_squared(hp->pos, waypoint_pos) < 2.5600002f) {
                return true;
            }
        }
    }
    return false;
}

// 0x00B79220 (:7338)
void vehicle_flip(unsigned int handle)
{
    vehicle* vp = vehicle_from_handle(handle);
    if (vp == NULL) {
        return;
    }
    const vector up(0.0f, 1.0f, 0.0f);
    matrix new_orient;
    new_orient.make_uvec(vp->orient.fvec, up);
    const vector new_pos(up.x * 2.0f + vp->pos.x, vp->pos.y + up.y * 2.0f, vp->pos.z + up.z * 2.0f);
    vp->update_pos_and_orient(new_pos, new_orient, true);
    if (player_get_local_player()->vehicle_handle == handle) {
        camera_snap_to_default_pos();
    }
}

// 0x00B79340 (:7363)
void vehicle_flip_upside_down(unsigned int handle, bool flip_45_degrees)
{
    vehicle* vp = vehicle_from_handle(handle);
    if (vp == NULL) {
        return;
    }
    matrix new_orient;
    vector new_pos;
    if (flip_45_degrees) {
        new_orient = vp->orient;
        new_pos = vp->pos;
        // Original bug kept: a world height of 2, not a lift of 2.
        if (vp->m_flags.m_wheels_on_ground != 0 && 0.8f < vp->orient.uvec.y) {
            new_pos.y = 2.0f;
        }
        new_orient.rotate_around_local_vector(vp->orient.fvec, 0.7853982f); // 45 degrees
    }
    else {
        const vector down(-UP_VECTOR.x, -UP_VECTOR.y, -UP_VECTOR.z);
        new_orient.make_uvec(vp->orient.fvec, down);
        new_pos.set(vp->pos.x + UP_VECTOR.x * 2.0f, vp->pos.y + UP_VECTOR.y * 2.0f, vp->pos.z + UP_VECTOR.z * 2.0f);
    }
    vp->update_pos_and_orient(new_pos, new_orient, true);
    if (player_get_local_player()->vehicle_handle == handle) {
        camera_snap_to_default_pos();
    }
}

// 0x00B8C5A0 (:7441) (LTCG: from_type in EAX; hit_points on the stack holds the attacker in this build)
static void notify_vehicle_ai_of_damage(vehicle* vp, object* attacker, damaged_by_types from_type)
{
    // a fast vehicle under fire swerves, unless a friend shot it or it is on a rail
    if (from_type != DBT_EXPLOSION && from_type != DBT_BULLET) {
        return;
    }
    if (vp->info->vehicle_classification == BVT_TANK || vp->info->vehicle_classification == BVT_ART_TANK) {
        return;
    }
    float speed;
    vehicle_get_speed(vp, speed);
    if (fabsf(speed) < 10.0f) {
        return;
    }
    vehicle_ai* ai = vehicle_get_vehicle_ai(vp->get_handle());
    if (ai == NULL || ai->get_state() == VS_AI_IDLE) {
        return;
    }
    human* driver = NULL;
    if (!vp->most_recent_driver_exit_time.valid()) {
        driver = vehicle_human_from_handle(vp->most_recent_driver);
        if (driver != NULL && driver->get_sub_type() == OT_SUB_HUMAN_NPC && vrail_is_patrol(((npc*)driver)->get_ai()->get_patrol_object())) {
            return;
        }
    }
    if (attacker != NULL && attacker->get_obj_type() == OT_HUMAN && driver != NULL && human_teams_are_allies((human*)attacker, driver)) {
        return;
    }
    unsigned int bridge_navpoint;
    if (ai->vai_flags.m_should_not_swerve == 0 && ai->vai_flags.m_lateral_offset == 0 && !is_vehicle_on_bridge(vp, bridge_navpoint)) {
        ai->vai_flags.m_lateral_offset = 1;
        ai->swerve_timer.set(1500);
        ai->initial_swerve_timer.set(500);
        float offset = fl_rand_range(LOW_SWERVE_DISTANCE, HIGH_SWERVE_DISTANCE);
        if (0.5f < fl_rand()) {
            offset = -offset;
        }
        ai->steer_info.lateral_offset = offset;
    }
}

// 0x00BBFC60 (:7559)
bool vehicle_apply_damage(vehicle* vp, object* attacker, const weapon_info* attacker_weapon, damaged_by_types from_type, const vector* damage_position, const vector* damage_normal, float damage_radius, int damage_to_apply, const vector* damage_velocity, bool from_server)
{
    if (vp == NULL || !vp->rdd.is_loaded()) {
        return false;
    }
    if ((Multiplayer && !multi_is_server() && !from_server) || vp->m_flags.m_corpse != 0 || vp->m_flags.m_only_fire_damage != 0) {
        return true;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER && (from_type == DBT_CAR || from_type == DBT_WORLD || from_type == DBT_LEVEL_OBJECT)) {
        return false;
    }
    // a building's damage is rate limited
    if (attacker != NULL && attacker->get_obj_type() == OT_MOVER) {
        int elapsed = vp->m_vehicle_vs_building_damage.time_since();
        if (1000 < elapsed) {
            elapsed = 1000;
        }
        const float wanted = (float)(int)((float)damage_to_apply * Vehicle_vs_building_damage_multiplier);
        const float allowed = (float)elapsed * 0.001f * Vehicle_vs_building_max_damage_per_second;
        damage_to_apply = (int)(wanted - allowed < 0.0f ? wanted : allowed);
        const int unused_ms = elapsed + (int)(((float)damage_to_apply * -1000.0f) / Vehicle_vs_building_max_damage_per_second);
        vp->m_vehicle_vs_building_damage.set(-(unused_ms > 0 ? unused_ms : 0));
        const difficulty_levels level = difficulty_get_level();
        if (level == DIFFICULTY_LEVEL_HARD || level == DIFFICULTY_LEVEL_INSANE) {
            damage_to_apply = (int)((float)damage_to_apply * 1.5f);
        }
    }

    player* local = player_get_local_player();
    if (0 < damage_to_apply) {
        if (local->vehicle_handle != vp->get_handle() && (attacker == NULL || attacker->get_sub_type() == OT_SUB_HUMAN_NPC)) {
            damage_to_apply = (int)((double)(float)damage_to_apply * (double)Damage_multiplier_npc_vehicle); // double product in rfg.exe
        }
        // only the player can hurt a courier
        human* driver = vehicle_passenger_in_seat(vp->get_handle(), VSI_DRIVER);
        if (driver != NULL && driver->get_sub_type() == OT_SUB_HUMAN_NPC && activity_cc_is_courier((npc*)driver)) {
            if (attacker == NULL) {
                return false;
            }
            bool from_player;
            if (attacker->get_obj_type() == OT_HUMAN) {
                from_player = object_is_local_player(attacker);
            }
            else if (attacker->get_obj_type() == OT_VEHICLE) {
                from_player = object_is_local_player(vehicle_passenger_in_seat(attacker->get_handle(), VSI_DRIVER));
            }
            else {
                return false;
            }
            if (!from_player) {
                return false;
            }
            activity_cc_courier_damaged_start();
        }
    }
    if (attacker != NULL && attacker->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        const walker_attack_types attack = walker_get_attack_type((walker*)attacker);
        if (attack == WALKER_ATTACK_THROW) {
            damage_to_apply *= 5;
        }
        else if (attack == WALKER_ATTACK_SMASH) {
            damage_to_apply *= 30;
        }
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER) {
        flyer* fp = (flyer*)vp;
        if (attacker == (object*)player_get_local_player()) {
            fp->last_player_damage.set(10000);
        }
        human* pilot = vehicle_passenger_in_seat(vp->get_handle(), VSI_DRIVER);
        if (pilot != NULL && pilot->get_sub_type() == OT_SUB_HUMAN_NPC) {
            ((npc*)pilot)->get_ai()->action_timer.set(5000);
        }
    }
    if (from_type == DBT_BULLET) {
        damage_to_apply = (int)(vp->info->m_bullet_damage_scale * (float)damage_to_apply);
    }
    if (human_in_vehicle_interaction(player_get_local_player(), vp) && from_type != DBT_EXPLOSION) {
        damage_to_apply = (int)((float)damage_to_apply * Vehicle_player_damage_factor);
    }
    if (local != NULL) {
        if (local->vehicle_handle == vp->get_handle()) {
            damage_to_apply = (int)((double)(float)damage_to_apply * (double)difficulty_get_value(DIFFICULTY_PLAYER_VEHICLE_DAMAGE, NUM_DIFFICULTY_LEVELS)); // x87 product in rfg.exe
            if (stats_cheat_enabled(CHEATS_SUPER_TOUGHNESS)) {
                damage_to_apply = (int)((double)damage_to_apply * 0.25);
            }
        }
        else {
            human* occupant = vp->most_recent_driver_exit_time.valid() ? NULL : vehicle_human_from_handle(vp->most_recent_driver);
            if (occupant == NULL) {
                // Original quirk kept: only the first passenger's seat is looked at.
                occupant = vehicle_passenger_in_seat(vp, VSI_PASSENGER_1);
            }
            const human_teams team = occupant != NULL ? occupant->current_team : HUMAN_TEAM_NONE;
            if (!human_teams_are_friendly(local->current_team, team)) {
                damage_to_apply = (int)((double)(float)damage_to_apply * (double)difficulty_get_value(DIFFICULTY_ENEMY_VEHICLE_DAMAGE, NUM_DIFFICULTY_LEVELS)); // x87 product in rfg.exe
            }
        }
    }
    if (from_type == DBT_WORLD) {
        damage_to_apply = (int)(vp->info->m_terrain_damage_scale * (float)damage_to_apply);
    }

    // NPCs nearby hear it
    if (vp->damage_sound_propagate_timer.elapsed() && attacker != NULL && (from_type == DBT_BULLET || from_type == DBT_MELEE || from_type == DBT_CAR)) {
        human* source = NULL;
        if (attacker->get_obj_type() == OT_HUMAN) {
            source = (human*)attacker;
        }
        else if (attacker->get_obj_type() == OT_VEHICLE) {
            source = World.object_from_handle<human>(((vehicle*)attacker)->most_recent_driver);
        }
        else if (attacker->get_obj_type() == OT_TURRET) {
            source = human_combat_turret_get_operator((turret*)attacker);
        }
        if (source != NULL) {
            human* driver = vp->most_recent_driver_exit_time.valid() ? NULL : vehicle_human_from_handle(vp->most_recent_driver);
            const vector& sound_pos = damage_position != NULL ? *damage_position : vp->pos;
            if (human_teams_are_unfriendly(source, vp->info->default_team) || (driver != NULL && human_teams_are_enemies(source, driver))) {
                npc_sound_info_vehicle_damage sound(vp, driver, source, sound_pos, (float)damage_to_apply, from_type == DBT_CAR);
                npc_sound_info::queue<npc_sound_info_vehicle_damage>(sound);
            }
            else {
                npc_sound_info_unimportant_vehicle_damage sound(vp, driver, source, sound_pos, (float)damage_to_apply, from_type == DBT_CAR);
                npc_sound_info::queue<npc_sound_info_unimportant_vehicle_damage>(sound);
            }
            vp->damage_sound_propagate_timer.set(2000);
        }
    }
    notify_vehicle_ai_of_damage(vp, attacker, from_type);

    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    const float radius_sqr = (float)((double)damage_radius * (double)damage_radius);
    vehicle_component* hit_components[66];
    unsigned int num_hit = 0;
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_component* component = &data->m_components[i];
        if ((component->get_flags() & (VCF_DETACHED | VCF_NOT_RENDERED)) == 0 && component->get_squared_distance(vp, damage_position) < radius_sqr && component->get_data()->m_type != VCT_TREAD_SEGMENT) {
            if (num_hit == 66) {
                break;
            }
            hit_components[num_hit++] = component;
        }
    }
    const int hit_points_before = vp->m_damage_areas[0].m_hit_points;
    const bool player_protected = local != NULL && local->is_invulnerable() && local->vehicle_handle == vp->get_handle() && 0 < damage_to_apply;
    if (!player_protected && vp->m_flags.m_invulnerable == 0) {
        if (vp->m_damage_areas[0].m_type == VDA_ENGINE) {
            vehicle_apply_damage_to_engine(vp, attacker, from_type == DBT_CAR, attacker_weapon, &vp->m_damage_areas[0], damage_to_apply, false); // inlined in rfg.exe
        }
        else {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        for (unsigned int i = 0; i < num_hit; i++) {
            hit_components[i]->apply_damage(vp, attacker, damage_to_apply, from_type, damage_velocity, damage_position, damage_normal, attacker_weapon);
        }
    }

    if (damage_position != NULL) {
        if (damage_to_apply < 1) {
            return true;
        }
        // lights within the radius break
        for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
            vehicle_light& light = data->m_vehicle_lights[i];
            if (light.type != VLT_INVALID && (light.mesh_light == NULL || (!light.flags.broken && (light.parent_idx == -1 || (data->m_components[light.parent_idx].get_flags() & VCF_DETACHED) == 0)))) {
                vector light_pos;
                vp->get_tag_world_coords(((i & 0xFF) | VEHICLE_TAG_LIGHT) << 16, light_pos, NULL, NULL, NULL);
                const vector to_light(light_pos.x - damage_position->x, light_pos.y - damage_position->y, light_pos.z - damage_position->z);
                if (to_light.mag_squared() <= radius_sqr) {
                    light.flags.broken = true;
                    light.flags.dirty = true;
                }
            }
        }
    }
    if (0 < damage_to_apply) {
        vehicle_deform(vp, hit_components, num_hit, damage_position, damage_radius, damage_to_apply, from_type);
        activity_register_damage(vp, attacker, (float)damage_to_apply, hit_points_before);
        stats_register_vehicle_damage(vp, attacker, hit_points_before);
    }
    return true;
}

// 0x00B65560 (:7950)
bool vehicle_queue_damage(vehicle* vp, object* attacker, const weapon_info* attacker_weapon, damaged_by_types from_type, const vector* damage_position, const vector* damage_normal, float damage_radius, int damage_to_apply, const vector* damage_velocity, bool from_server)
{
    vp->damage_events_cs.enter();
    if (4 < vp->num_damage_events) {
        vp->damage_events_cs.leave();
        return false;
    }
    vehicle_damage_event& event = vp->damage_events[vp->num_damage_events++];
    event.attacker_handle = attacker != NULL ? attacker->get_handle() : 0xFFFFFFFF;
    event.attacker_weapon = attacker_weapon;
    event.from_type = from_type;
    event.damage_position = *damage_position;
    event.damage_normal = *damage_normal;
    event.damage_radius = damage_radius;
    event.damage_to_apply = damage_to_apply;
    event.from_server = from_server;
    if (damage_velocity != NULL) {
        event.damage_velocity = *damage_velocity;
        event.damage_velocity_valid = true;
    }
    else {
        event.damage_velocity.zero();
        event.damage_velocity_valid = false;
    }
    vp->damage_events_cs.leave();
    return 4 < damage_to_apply;
}

// 0x00BC08A0 (:8001)
void vehicle_dequeue_damage(vehicle* vp)
{
    vp->damage_events_cs.enter();
    for (int i = 0; i < vp->num_damage_events; i++) {
        vehicle_damage_event& event = vp->damage_events[i];
        object* attacker = World.object_from_handle(event.attacker_handle);
        if (attacker != NULL) {
            vehicle_apply_damage(vp, attacker, event.attacker_weapon, event.from_type, &event.damage_position, &event.damage_normal, event.damage_radius, event.damage_to_apply, event.damage_velocity_valid ? &event.damage_velocity : NULL, event.from_server);
        }
    }
    vp->num_damage_events = 0;
    vp->damage_events_cs.leave();
}

// 0x00B79530 (:8035)
void vehicle_process_animations(vehicle* vp)
{
    if (vp->aim_handle == -1 || !vp->rdd.is_loaded() || vp->vp == NULL) {
        return;
    }
    vanim_process_states(vp->vp);
    if (!animlib_allocate_bones(vp->aim_handle)) {
        return;
    }
    animlib_clear_bone_info(vp->aim_handle);
    animlib_process_instance(vp->aim_handle, cutscene_is_playing() ? Cutscene_timer.get_frame_time() : Frametime);
    for (int bone = 0; bone < vp->info->bones_used_info.num_bones; bone++) {
        animlib_invalidate_cache_bone(vp->aim_handle, bone, true, true);
    }
    static matrix43 bones[74]; // on the stack in rfg.exe (0xC00 bytes); not touched by the two calls without a list
    animlib_transform_all_bones_pre(vp->aim_handle, bones, NULL, 0);
    animlib_transform_all_bones_post(vp->aim_handle, bones, NULL, 0);
    vanim_check_prop_handles(vp->vp, vp);
    vanim_process_completed_actions(vp->vp);
}

// 0x00B8C790 (:8084)
bool vehicle_get_ai_info(unsigned int v_handle, havok_vehicle_ai_info& info)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return false;
    }
    const unsigned int wanted = info.flags;
    if ((wanted & 1) != 0) {
        info.pos = vp->pos;
    }
    if ((wanted & 8) != 0) {
        info.orient = vp->orient;
    }
    if ((wanted & 0x20) != 0) {
        info.steering_max_angle = vp->info->max_steering_angle;
        info.steering_max_angle_speed = vp->info->max_speed_steering_angle;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO && (wanted & 0x16) != 0) {
        automobile* ap = (automobile*)vp;
        if ((wanted & 2) != 0 && vp->havok_handle != 0xFFFFFFFF) {
            havok_to_game_vector(info.vel, havok_body_get_pointer(vp->havok_handle)->getLinearVelocity());
        }
        else {
            info.vel.zero();
        }
        if ((wanted & 4) != 0 && ap->a_rdd.is_loaded()) {
            info.speed = (float)((double)ap->a_rdd.get_data()->m_hk_vehicle_instance->calcKMPH() * 0.2777799963951111); // km/h to m/s
        }
        else {
            info.speed = 0.0f;
        }
        info.moving_in_reverse = (wanted & 0x10) != 0 && automobile_moving_in_reverse(ap);
    }
    info.is_tank = vp->info->vehicle_classification == BVT_TANK || vp->info->vehicle_classification == BVT_ART_TANK;
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER && (wanted & 0x16) != 0) {
        if ((wanted & 2) != 0 && vp->havok_handle != 0xFFFFFFFF) {
            vehicle_get_velocity(vp->get_handle(), info.vel);
        }
        else {
            info.vel.zero();
        }
        if ((info.flags & 4) != 0 && ((flyer*)vp)->f_rdd.is_loaded()) {
            vehicle_get_speed(vp, info.speed);
        }
        else {
            info.speed = 0.0f;
        }
        info.moving_in_reverse = false;
    }
    if ((info.flags & 0x40) != 0) {
        vp->get_local_bbox(info.bbox_min, info.bbox_max);
    }
    return true;
}

// 0x00B79670 (:8163)
bool vehicle_is_resource_loaded(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL && vp->rdd.is_loaded();
}

// 0x00B796A0 (:8180)
hkpRigidBody* vehicle_get_rigid_body(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL ? havok_body_get_pointer(vp->havok_handle) : NULL;
}

// 0x00B97650 (:8202)
bool vehicle_calculate_ground_pos_and_orient(vector& out_pos, matrix& out_orient, const vector& start_pos, const vector& start_fvec, float raycast_y_offset, bool allow_upwards_raycast)
{
    raycast_input ray;
    ray.radius = 0.0f;
    ray.m_filter_info = COLLIDE_RAY_DEFAULT;
    ray.start.set(start_pos.x, start_pos.y + raycast_y_offset, start_pos.z);
    ray.end.set(start_pos.x, start_pos.y + raycast_y_offset - 150.0f, start_pos.z);
    ray.filter_params = NULL;
    ray.filter_callback = NULL;
    ray.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    ray.m_flags = 0;
    raycast_output hits;
    vector normal;
    if (havok_raycast_against_static_level_only(ray, hits)) {
        out_pos = hits.m_hits[0].hit_pos;
        normal = hits.m_hits[0].hit_normal;
    }
    else {
        // under the level: look up, for the ground's underside
        ray.end.y += 300.0f;
        raycast_output up_hits;
        if (!allow_upwards_raycast || !havok_raycast_against_static_level_only(ray, up_hits)) {
            return false;
        }
        out_pos = up_hits.m_hits[0].hit_pos;
        const vector& hit_normal = up_hits.m_hits[0].hit_normal;
        normal.set(hit_normal.x * -1.0f, hit_normal.y * -1.0f, hit_normal.z * -1.0f);
    }
    float cos_angle = normal.y * UP_VECTOR.y + UP_VECTOR.x * normal.x + normal.z * UP_VECTOR.z;
    if (cos_angle - -1.0f < 0.0f) {
        cos_angle = -1.0f;
    }
    if (0.0f <= cos_angle - 1.0f) {
        cos_angle = 1.0f;
    }
    const float angle = fl_acos(cos_angle);
    if (angle <= 0.017453f) { // a degree
        out_orient.make_uvec(start_fvec, normal);
    }
    else {
        out_orient.make_uvec(start_fvec, UP_VECTOR);
        out_orient.rotate_around_local_x(start_fvec.y * normal.y + start_fvec.x * normal.x + start_fvec.z * normal.z <= 0.0f ? -angle : angle);
    }
    return true;
}

// 0x00B979B0 (:8267) (LTCG: vp in ECX and EAX (orient), new_pos in EDI)
static bool vehicle_has_solid_ground_behind_itself(vehicle* vp, vector& new_pos, matrix& new_orient)
{
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    const float half_length = (bmax.x - bmin.x) * 0.5f;
    raycast_input ray;
    ray.start.set(new_pos.x - new_orient.fvec.x * half_length, new_pos.y - new_orient.fvec.y * half_length + 2.0f, new_pos.z - new_orient.fvec.z * half_length);
    ray.end.set(ray.start.x, new_pos.y - new_orient.fvec.y * half_length - 150.0f, ray.start.z);
    ray.radius = 0.0f;
    ray.filter_params = NULL;
    ray.filter_callback = NULL;
    ray.m_filter_info = COLLIDE_RAY_DEFAULT;
    ray.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    ray.m_flags = 0;
    raycast_output hits;
    return havok_raycast_against_static_level_only(ray, hits);
}

// 0x00B8C9A0 (:8287)
bool vehicle_reposition_point_has_obstacles(vehicle* vp, vector& new_pos, matrix& new_orient)
{
    vector bmin;
    vector bmax;
    vehicle_get_chassis_bbox(vp->get_handle(), bmin, bmax); // inlined in rfg.exe
    bmax.y -= bmin.y;
    bmin.x -= 1.0f;
    bmin.y = -1.0f;
    bmin.z -= 1.0f;
    bmax.y += 1.0f;
    bmax.x += 1.0f;
    bmax.z += 1.0f;
    unsigned int objects[13];
    const int num_objects = havok_collect_objects_fast(bmin, bmax, new_pos, new_orient, objects, 13, COLLIDE_OBJECT_COLLECTOR_NO_ITEMS, NULL, NULL);
    for (int i = 0; i < num_objects; i++) {
        // only the vehicle's own passengers do not count
        human* hp = vehicle_human_from_handle(objects[i]);
        if (hp == NULL || !human_in_vehicle(hp, vp->get_handle())) {
            return true;
        }
    }
    return false;
}

// 0x00B97B60 (:8321)
bool vehicle_reposition_on_ground(vehicle* vp)
{
    vector new_pos;
    matrix new_orient;
    if (vehicle_calculate_ground_pos_and_orient(new_pos, new_orient, vp->pos, vp->orient.fvec, 2.0f, true) && vehicle_has_solid_ground_behind_itself(vp, new_pos, new_orient) && !vehicle_reposition_point_has_obstacles(vp, new_pos, new_orient)) {
        vp->update_pos_and_orient(new_pos, new_orient, true);
        return true;
    }
    return false;
}

// 0x00B65660 (:8346)
void vehicle_init_damage_areas(vehicle* vp)
{
    if (vp != NULL) {
        vp->m_damage_areas[0].m_max_hit_points = vp->info->m_max_hitpoints;
        vp->m_damage_areas[0].m_hit_points = vp->m_damage_areas[0].m_max_hit_points;
        vp->m_damage_areas[0].m_type = VDA_ENGINE;
        vp->m_damage_areas[0].m_effect_instances[0] = 0xFFFFFFFF;
        vp->m_damage_areas[0].m_effect_instances[1] = 0xFFFFFFFF;
    }
}

// 0x00B8CB30 (:8372)
void vehicle_freeze(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return;
    }
    const hkVector4f zero(0.0f, 0.0f, 0.0f, 0.0f);
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    body->setLinearVelocity(zero);
    body->setAngularVelocity(zero);
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        // Original bug kept: no check that the automobile's data is loaded.
        hk_custom_vehicle_instance* instance = ((automobile*)vp)->a_rdd.get_data()->m_hk_vehicle_instance;
        instance->set_accelerator_pedal_input(0.0f);
        instance->set_forward_or_reverse_accelerator(0.0f);
        instance->set_handbrake_on(true);
    }
}

// 0x00B796D0 (:8398)
void vehicle_lock_position(unsigned int v_handle, bool lock)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL || (vp->m_flags.locked_in_place != 0) == lock) {
        return;
    }
    vp->m_flags.locked_in_place = lock;
    if (vp->havok_handle != 0xFFFFFFFF && !lock) {
        havok_body_set_motion_type(vp->havok_handle, 3);
        havok_body_force_activate(vp->havok_handle);
        vp->m_flags.fixed_motion = 0;
    }
}

// 0x00B656B0 (:8434)
const char* vehicle_get_info_name(const vehicle* vp)
{
    return vp->info->name;
}

// 0x00B79750 (:8488)
void vehicle_get_common_info(unsigned int v_handle, vector& pos, matrix& orient, float& speed, bool& moving_in_reverse)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return;
    }
    if (!vp->rdd.is_loaded()) {
        pos.zero();
        orient.make_identity();
        speed = 0.0f;
        moving_in_reverse = false;
        return;
    }
    pos = vp->pos;
    orient = vp->orient;
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        automobile* ap = (automobile*)vp;
        speed = (float)((double)ap->a_rdd.get_data()->m_hk_vehicle_instance->calcKMPH() * 0.2777799963951111); // km/h to m/s
        moving_in_reverse = ap->a_rdd.is_loaded() && ap->a_rdd.get_data()->m_hk_vehicle_instance != NULL && ap->a_rdd.get_data()->m_hk_vehicle_instance->m_isReversing;
        return;
    }
    speed = 0.0f;
    moving_in_reverse = false;
}

// 0x00B79860 (:8522)
bool vehicle_get_speed(const vehicle* vp, float& speed)
{
    speed = 0.0f;
    if (vp == NULL) {
        return false;
    }
    vehicle* live = vehicle_from_handle(vp->get_handle());
    if (live == NULL) {
        return false;
    }
    hkpRigidBody* body = havok_body_get_pointer(live->havok_handle);
    if (body == NULL) {
        return false;
    }
    // along the body's forward axis (its third rotation column)
    const hkVector4f& velocity = body->getLinearVelocity();
    const hkVector4f& forward = body->getTransform().getRotation().getColumn(2);
    speed = velocity(1) * forward(1) + velocity(0) * forward(0) + velocity(2) * forward(2);
    return true;
}

// 0x00B798E0 (:8555)
void vehicle_get_chassis_bbox(unsigned int v_handle, vector& bmin, vector& bmax)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp != NULL) {
        vp->get_local_bbox(bmin, bmax);
        return;
    }
    bmin.zero();
    bmax.zero();
}

// 0x00B79930 (:8596)
void vehicle_set_damage_trigger(unsigned int v_handle, unsigned short func_handle, float damage_percent)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp != NULL) {
        vp->damage_percent = damage_percent;
        vp->damage_func_handle = func_handle;
    }
}

// 0x00B79970 (:8614)
void vehicle_set_destroy_trigger(unsigned int v_handle, unsigned short func_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp != NULL) {
        vp->destroy_func_handle = func_handle;
    }
}

// 0x00B799A0 (:8632)
unsigned int vehicle_get_number_of_components(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle); // Original bug kept: not checked for null
    return vp->rdd.is_loaded() ? vp->rdd.get_data()->m_num_components : 0;
}

// 0x00B799E0 (:8651)
void vehicle_set_invulnerable(unsigned int v_handle, bool invulnerable)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp != NULL) {
        vp->m_flags.m_invulnerable = invulnerable;
    }
}

// 0x00B79A20 (:8661)
void vehicle_set_npc_ray_override(unsigned int v_handle, bool ride)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp != NULL && vp->get_sub_type() == OT_SUB_VEHICLE_AUTO && ((automobile*)vp)->a_rdd.is_loaded() && ((automobile*)vp)->a_rdd.get_data()->m_hk_vehicle_instance != NULL) {
        ((hk_custom_vehicle_raycast_wheel_collide*)((automobile*)vp)->a_rdd.get_data()->m_hk_vehicle_instance->m_wheelCollide)->set_npc_ray_override(ride);
    }
}

// 0x00B8CC30 (:8686)
void vehicle_set_components_strongest(unsigned int v_handle, bool strongest)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL || !vp->rdd.is_loaded()) {
        return;
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        int durability = data->m_components[i].get_data()->m_durability;
        if (strongest && durability < 5) {
            durability = 5;
        }
        data->m_components[i].set_hitpoints_to_durability(vp, durability);
    }
}

// 0x00B79A70 (:8715)
void vehicle_set_hit_points(unsigned int v_handle, unsigned int hit_points)
{
    vehicle* vp = vehicle_from_handle(v_handle); // Original bug kept: without one it writes through null
    vp->m_damage_areas[0].m_hit_points = hit_points;
}

// 0x00B79AB0 (:8729)
void vehicle_get_smoking_and_fire(unsigned int v_handle, bool& smoking, bool& on_fire)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp != NULL) {
        on_fire = vp->m_flags.m_on_fire != 0;
        smoking = vp->m_flags.m_engine_smoking != 0;
    }
}

// Where the engine effects play: the "engine" tag, else the vehicle's origin.
static int vehicle_engine_effect_placement(vehicle* vp, vector& pos, matrix& orient)
{
    const int tag = vp->get_tag_index_from_name("engine");
    if (tag < 0) {
        pos = vp->pos;
        orient = vp->orient;
    }
    else {
        vp->get_tag_world_coords(tag, pos, &orient, NULL, NULL);
    }
    return tag;
}

// 0x00B8CCB0 (:8747)
void vehicle_set_smoking_and_fire(unsigned int v_handle, bool smoking, bool on_fire, bool from_nano)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return;
    }
    vector pos;
    matrix orient;
    if (vp->m_flags.m_on_fire == 0 && on_fire) {
        if (vp->m_damage_areas[0].m_effect_instances[1] == 0xFFFFFFFF) {
            const int tag = vehicle_engine_effect_placement(vp, pos, orient);
            if (from_nano) {
                vp->on_fire_timestamp.set(0);
                vp->m_flags.m_on_fire = 1;
                vp->m_flags.m_nano_sim_fire = 1;
            }
            else {
                const unsigned int effect = effects_play(vp->info->m_effect_engine_fire, pos, orient, true, vp->get_handle(), tag, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
                if (effect != 0xFFFFFFFF) {
                    vp->on_fire_timestamp.set(0);
                    vp->m_damage_areas[0].m_effect_instances[1] = effect;
                    vp->m_flags.m_on_fire = 1;
                    vp->m_flags.m_nano_sim_fire = 0;
                }
            }
        }
    }
    else if (vp->m_flags.m_on_fire != 0 && !on_fire) {
        vp->m_flags.m_on_fire = 0;
        vp->m_flags.m_nano_sim_fire = 0;
        vp->on_fire_timestamp.invalidate();
        const unsigned int effect = vp->m_damage_areas[0].m_effect_instances[1];
        if (effect != 0xFFFFFFFF) {
            vp->m_damage_areas[0].m_effect_instances[1] = 0xFFFFFFFF;
            object_effect* fire = World.object_from_handle<object_effect>(effect);
            if (fire != NULL) {
                fire->stop(object_effect::SS_STOP_EMITTING);
            }
        }
    }
    if (vp->m_flags.m_engine_smoking == 0 && smoking) {
        if (vp->m_damage_areas[0].m_effect_instances[0] == 0xFFFFFFFF) {
            const int tag = vehicle_engine_effect_placement(vp, pos, orient);
            if (from_nano) {
                vp->m_flags.m_engine_smoking = 1;
                vp->m_flags.m_nano_sim_smoke = 1;
                vehicle_update_smoke_value(vp);
                return;
            }
            const unsigned int effect = effects_play(vp->info->m_effect_engine_smoke, pos, orient, true, vp->get_handle(), tag, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            if (effect != 0xFFFFFFFF) {
                vp->m_damage_areas[0].m_effect_instances[0] = effect;
                vp->m_flags.m_engine_smoking = 1;
                vp->m_flags.m_nano_sim_smoke = 0;
                vehicle_update_smoke_value(vp);
            }
        }
    }
    else if (vp->m_flags.m_engine_smoking != 0 && !smoking) {
        vp->m_flags.m_engine_smoking = 0;
        const unsigned int effect = vp->m_damage_areas[0].m_effect_instances[0];
        if (effect != 0xFFFFFFFF) {
            vp->m_damage_areas[0].m_effect_instances[0] = 0xFFFFFFFF;
            object_effect* smoke = World.object_from_handle<object_effect>(effect);
            if (smoke != NULL) {
                smoke->stop(object_effect::SS_STOP_EMITTING);
            }
        }
    }
}

// 0x00B79B00 (:8874)
bool vehicle_all_passengers_have_resources_loaded(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        human* hp = vehicle_human_from_handle(vp->vii.humans[i].handle);
        if (hp != NULL && !hp->char_inst->rdd.is_loaded()) {
            return false;
        }
    }
    return true;
}

// 0x00B79B80 (:8899)
bool vehicle_get_honked_horn(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL && vp->m_flags.m_mp_honked_horn != 0;
}

// 0x00B79BB0 (:8914)
void vehicle_turnoff_horn(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp != NULL) {
        vp->m_flags.m_mp_honked_horn = 0;
    }
}

// 0x00B8CFE0 (:8932)
void vehicle_set_max_hp(int new_max, unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return;
    }
    if (new_max == 0) {
        new_max = 1;
    }
    const int old_max = vp->m_damage_areas[0].m_max_hit_points;
    vp->m_damage_areas[0].m_max_hit_points = new_max;
    vp->m_damage_areas[0].m_hit_points = (int)((float)new_max * ((float)vp->m_damage_areas[0].m_hit_points / (float)old_max));
    if (vp->rdd.is_loaded()) {
        vehicle::resource_dependent_data* data = vp->rdd.get_data();
        for (unsigned int i = 0; i < data->m_num_components; i++) {
            data->m_components[i].set_hitpoints(vp, false);
        }
    }
}

// 0x00B79BE0 (:8966)
bool vehicle_collisions_disabled(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return false;
    }
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    return body != NULL && body->getCollidable()->getCollisionFilterInfo() == COLLIDE_NO_COLLISION;
}

// 0x00B79C40 (:8983)
void vehicle_disable_collisions(unsigned int v_handle, bool disable)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL || !vp->rdd.is_loaded()) {
        return;
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    if (disable) {
        if (vehicle_get_collision_filter(vp) != COLLIDE_NO_COLLISION) { // inlined in rfg.exe
            havok_body_enable_collision(body, &data->m_saved_collision_filter, false);
            for (unsigned int i = 0; i < data->m_num_components; i++) {
                vehicle_component& component = data->m_components[i];
                if ((component.get_flags() & VCF_DETACHED) == 0) {
                    hkpRigidBody* component_body = havok_body_get_pointer(component.havok_handle);
                    if (component_body != NULL) {
                        // Original bug kept: a component's filter is saved to the handle parameter, so it is lost.
                        havok_body_enable_collision(component_body, &v_handle, false);
                    }
                }
            }
        }
    }
    else if (vehicle_get_collision_filter(vp) == COLLIDE_NO_COLLISION) {
        havok_body_enable_collision(body, &data->m_saved_collision_filter, true);
        for (unsigned int i = 0; i < data->m_num_components; i++) {
            vehicle_component& component = data->m_components[i];
            if ((component.get_flags() & VCF_DETACHED) == 0) {
                hkpRigidBody* component_body = havok_body_get_pointer(component.havok_handle);
                if (component_body != NULL) {
                    havok_body_enable_collision(component_body, &data->m_saved_collision_filter, true);
                }
            }
        }
    }
}

// 0x00B79DA0 (:9069)
void vehicle_set_chassis_variant(unsigned int v_handle, unsigned int num_rands, const char* rand_set)
{
    // Nothing is left of it in rfg.exe but the lookup.
    if (v_handle != 0xFFFFFFFF) {
        World.object_from_handle(v_handle);
    }
}

// 0x00B79DC0 (:9100)
void vehicle_update_orient_pos_from_havok(vehicle* vp)
{
    if (!vp->rdd.is_loaded()) {
        return;
    }
    const unsigned int handle = vp->get_handle();
    const bool is_player_vehicle = player_get_local_player()->vehicle_handle == handle;
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    if (!body->isActive() && vp->m_flags.fixed_motion == 0) {
        return;
    }
    matrix43 transform;
    havok_to_game_transform(transform, body->getTransform());
    // a live walker follows its animation instead
    if (vp->get_sub_type() != OT_SUB_VEHICLE_WALKER || vp->m_damage_areas[0].m_hit_points < 1) {
        if (Vehicle_interpolate_havok && is_player_vehicle) {
            havok_interpolate_object(*vp, transform);
        }
        else {
            vp->update_pos_and_orient(transform.m_translation, transform.m_rotation, false);
        }
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_component& component = data->m_components[i];
        if (component.havok_handle == 0xFFFFFFFF) {
            continue;
        }
        matrix43 component_transform;
        havok_to_game_transform(component_transform, havok_body_get_pointer(component.havok_handle)->getTransform());
        if (Vehicle_interpolate_havok && is_player_vehicle) {
            const matrix43 old_transform(component.get_world_space_position(), component.get_world_space_rotation());
            component_transform = havok_interpolate_transform(old_transform, component_transform);
        }
        component.set_orient_pos_in_world_space(vp, component_transform.m_rotation, component_transform.m_translation);
    }
    if (!Multiplayer) {
        bb_pf_update_last_visited_nav_cell(vp);
    }
}

// hkpVehicleDefaultVelocityDamper as far as the walker hack uses it (the class joins with the automobile files).
struct vehicle_velocity_damper_layout
{
    void* vftable; // 0x00
    unsigned int mem_size_and_ref_count; // 0x04
    float m_normalSpinDamping; // 0x08
    float m_collisionSpinDamping; // 0x0C
};

// An hkHalf's value, its short being the high half of the float.
static float vehicle_half_to_float(const hkHalf& half)
{
    const int bits = (int)*(const short*)&half << 16;
    return *(const float*)&bits;
}

// 0x00BBB570 (:9166)
void vehicle_do_frame_presim(vehicle* vp)
{
    if (vp->obj_flags.destroyed || !vp->rdd.is_loaded()) {
        return;
    }
    if (!World.is_zone_streamed(vp->pos) && !player_occupied_vehicle(vp) && !vehicle_should_avoid_despawning(vp)) {
        World.destroy_object(vp, false);
        return;
    }
    vehicle_process_animations(vp);
    vp->last_forward_velocity = vp->forward_velocity;
    memset(&vp->m_collisions_this_frame, 0, sizeof(vp->m_collisions_this_frame));
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    hkVector4f linear_velocity = body->getLinearVelocity();
    hkVector4f angular_velocity = body->getAngularVelocity();
    if (vp->get_sub_type() != OT_SUB_VEHICLE_FLYER && vertical_max_speed < linear_velocity(1)) {
        linear_velocity(1) = fl_sign(linear_velocity(1)) * vertical_max_speed;
        body->setLinearVelocity(linear_velocity);
    }
    // thrown by a walker: while the hack runs, speed it up (to 27 m/s) and undamp its spin
    if (!vp->m_walker_velocity_hack.elapsed()) {
        linear_velocity(1) = (float)((double)Frametime * 1.5 + (double)linear_velocity(1));
        linear_velocity.mul(hkSimdFloat32::fromFloat(1.1f));
        const float speed = linear_velocity.length<3>().getReal();
        if (27.0f < speed) {
            linear_velocity.mul(hkSimdFloat32::fromFloat((float)(27.0 / (double)speed)));
        }
        body->setLinearVelocity(linear_velocity);
        angular_velocity.mul(hkSimdFloat32::fromFloat(8.0f));
        body->setAngularVelocity(angular_velocity);
        hkHalf& damping = body->getRigidMotion()->m_motionState.m_angularDamping;
        const float saved = vehicle_half_to_float(damping);
        if (saved != 0.0f) {
            vp->saved_angular_dampening = saved;
        }
        *(short*)&damping = 0;
        if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO && ((automobile*)vp)->a_rdd.is_loaded()) {
            vehicle_velocity_damper_layout* damper = (vehicle_velocity_damper_layout*)((automobile*)vp)->a_rdd.get_data()->m_hk_vehicle_instance->m_velocityDamper;
            damper->m_normalSpinDamping = 0.0f;
            damper->m_collisionSpinDamping = 0.0f;
        }
        vp->m_walker_velocity_hack_followup.set(3000);
        vp->m_walker_thrown_energy_scaled.set(8000);
    }
    if (vp->m_walker_velocity_hack_followup.elapsed()) {
        // 1.00390625 rounds the float up to the next hkHalf
        const float restored = (float)((double)vp->saved_angular_dampening * 1.00390625);
        *(short*)&body->getRigidMotion()->m_motionState.m_angularDamping = (short)(*(const int*)&restored >> 16);
        if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO && ((automobile*)vp)->a_rdd.is_loaded()) {
            vehicle_velocity_damper_layout* damper = (vehicle_velocity_damper_layout*)((automobile*)vp)->a_rdd.get_data()->m_hk_vehicle_instance->m_velocityDamper;
            damper->m_normalSpinDamping = vp->info->m_normal_spin_damping;
            damper->m_collisionSpinDamping = vp->info->m_collision_spin_damping;
        }
        vp->m_walker_velocity_hack_followup.invalidate();
    }
    havok_to_game_vector(vp->last_velocity, linear_velocity);
    havok_to_game_vector(vp->last_angular_velocity, angular_velocity);
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        automobile_do_frame_presim((automobile*)vp);
    }
    else if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER) {
        vehicle_ai_do_frame(vp);
    }
    else if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        walker_do_frame_presim((walker*)vp);
    }
    const vector& velocity = vp->last_velocity;
    const float speed = sqrtf((float)((double)velocity.x * velocity.x + (double)velocity.y * velocity.y + (double)velocity.z * velocity.z));
    for (int i = 0; i < 2; i++) {
        object_effect* effect = vehicle_effect_from_handle(vp->m_damage_areas[0].m_effect_instances[i]);
        if (effect != NULL) {
            effect->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, speed);
        }
    }
    for (int i = 0; i < 4; i++) {
        object_effect* normal = vehicle_effect_from_handle(vp->m_exhaust_effect_normal[i]);
        object_effect* burst = vehicle_effect_from_handle(vp->m_exhaust_effect_burst[i]);
        if (normal != NULL) {
            normal->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, speed);
        }
        if (burst != NULL) {
            burst->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, speed);
        }
    }
    const float time_step = Frametime;
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_component& component = data->m_components[i];
        const unsigned int flags = component.get_flags();
        if ((flags & VCF_DETACHED) == 0 && component.get_data()->m_type == VCT_DOOR && (flags & VCF_UNLATCHED) != 0) {
            vehicle_component::door_attr& door = component.m_type_attribute.door;
            if (door.m_state == vehicle_component::ADI_NOT_ANIMATING) {
                const float last_t = door.m_t_actual;
                const float t = component.get_unlatched_door_t_value(vp);
                door.m_t_actual = t;
                door.m_t_velocity = 0.0f < time_step ? (t - last_t) / time_step : 0.0f;
            }
            else {
                component.update_animatable_door(time_step, vp);
            }
        }
    }
    if (vp->m_flags.m_on_fire == 0 || !vehicle_engine_on_fire_do_frame(vp, Frametime)) {
        vehicle_audio_engine_update(vp);
        if (vp->m_flags.locked_in_place != 0 && vp->m_flags.fixed_motion == 0 && vp->havok_handle != 0xFFFFFFFF && !havok_body_is_active(vp->havok_handle)) {
            havok_body_set_motion_type(vp->havok_handle, 5);
            vp->m_flags.fixed_motion = 1;
        }
        vehicle_preemptively_ragdoll_nearby_humans(vp);
    }
}

// 0x00B656C0 (:9428)
void compute_tread_wheel_ground_cast_positions(const vehicle* vp, const vehicle_component* comp, vector& start, vector& end, float& length, float& radius)
{
    const vehicle_component_data* data = comp->get_data();
    if (data->m_type != VCT_TREAD_WHEEL_GROUND) {
        start = data->m_position;
        end = data->m_position;
        length = 0.0f;
        return;
    }
    radius = vp->info->enlarged_wheel_radius + comp->m_type_attribute.tread_wheel.wheel_radius;
    length = radius * 2.0f;
    start = data->m_position;
    start.x += UP_VECTOR.x * radius * 0.5f;
    start.y += UP_VECTOR.y * radius * 0.5f;
    start.z += UP_VECTOR.z * radius * 0.5f;
    const float enlarged = vp->info->enlarged_wheel_radius;
    start.x -= UP_VECTOR.x * enlarged;
    start.y -= UP_VECTOR.y * enlarged;
    start.z -= UP_VECTOR.z * enlarged;
    end = start;
    end.x -= UP_VECTOR.x * length;
    end.y -= UP_VECTOR.y * length;
    end.z -= UP_VECTOR.z * length;
}

// Components a cutscene animation has brought back.
static const unsigned int VCF_ANIM_INSTANCE = 0x400000;

static void vehicle_destroy_anim_instance(vehicle* vp, vehicle_component& component)
{
    if ((component.get_flags() & VCF_ANIM_INSTANCE) != 0) {
        component.destroy_renderable_instance_subtree(vp);
        component.clear_flags(VCF_ANIM_INSTANCE);
    }
}

// The part of the post simulation step that plays the vehicle's animation on the chassis (non-looping cutscene
// animations move the root) and on the components. Inlined in rfg.exe.
static void vehicle_postsim_animate(vehicle* vp, vehicle::resource_dependent_data* data)
{
    matrix43 root;
    bool moved_root = false;
    if (cutscene_is_playing()) {
        unsigned int handles[10];
        int num_handles;
        if (animlib_get_active_animations(handles, num_handles, 10, vp->aim_handle)) {
            for (int i = 0; i < num_handles; i++) {
                if (animlib_is_looping(vp->aim_handle, handles[i])) {
                    continue;
                }
                animlib_get_bone_transform(vp->aim_handle, 0, root);
                const matrix& last_rot = vp->last_anim_transform.m_rotation;
                const vector& last_pos = vp->last_anim_transform.m_translation;
                vp->orient = root.m_rotation * last_rot;
                const vector& t = root.m_translation;
                vp->pos.x = last_rot.rvec.x * t.x + last_rot.uvec.x * t.y + last_rot.fvec.x * t.z + last_pos.x;
                vp->pos.y = last_pos.y + last_rot.rvec.y * t.x + last_rot.uvec.y * t.y + last_rot.fvec.y * t.z;
                vp->pos.z = last_pos.z + last_rot.rvec.z * t.x + last_rot.uvec.z * t.y + last_rot.fvec.z * t.z;
                vp->update_pos_and_orient(vp->pos, vp->orient, true);
                vehicle_calc_ws_pos_orient_for_components(vp, false);
                hkVector4f zero;
                zero.setZero();
                havok_body_get_pointer(vp->havok_handle)->setLinearVelocity(zero);
                moved_root = true;
                break;
            }
        }
    }
    if (!moved_root) {
        vp->last_anim_transform.m_translation = vp->pos;
        vp->last_anim_transform.m_rotation = vp->orient;
        root.make_identity();
    }
    const int num_active = animlib_get_num_active_anims(vp->aim_handle);
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        const int bone = data->component_bone_index[i];
        vehicle_component& component = data->m_components[i];
        if (bone < 0 || (component.get_flags() & VCF_DETACHED) != 0) {
            continue;
        }
        if (num_active == 0) {
            if (component.m_renderable_instance_p != NULL) {
                vehicle_destroy_anim_instance(vp, component);
            }
            continue;
        }
        if (!animlib_is_bone_animating(vp->aim_handle, bone)) {
            continue;
        }
        vp->set_render_flags(8);
        matrix43 bone_transform;
        animlib_get_bone_transform(vp->aim_handle, bone, bone_transform);
        if (bone_transform.m_translation.is_null()) {
            if (component.m_renderable_instance_p != NULL) {
                vehicle_destroy_anim_instance(vp, component);
            }
            continue;
        }
        if (component.m_renderable_instance_p == NULL) {
            component.create_renderable_instance_subtree(vp);
            component.set_flags(VCF_ANIM_INSTANCE);
        }
        const matrix rotation = component.get_data()->m_rotation * bone_transform.m_rotation * root.m_rotation.copy_inverse() * vp->orient;
        vector position(bone_transform.m_translation.x - root.m_translation.x, bone_transform.m_translation.y - root.m_translation.y, bone_transform.m_translation.z - root.m_translation.z);
        position.unrotate(vp->last_anim_transform.m_rotation);
        position.x += vp->pos.x;
        position.y += vp->pos.y;
        position.z += vp->pos.z;
        component.set_orient_pos_in_world_space(vp, rotation, position);
        hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
        if (body != NULL) {
            body->activate();
        }
    }
}

// The tread wheels spin by what their spline moved, and the ground ones follow the ground. Inlined in rfg.exe.
static void vehicle_postsim_tread_wheels(vehicle* vp, vehicle::resource_dependent_data* data, const float* spline_travel, hk_custom_vehicle_raycast_wheel_collide* wheel_collide, hkpAabbPhantom* phantom)
{
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_component& component = data->m_components[i];
        const unsigned int type = component.get_data()->m_type;
        if (type != VCT_TREAD_WHEEL_GROUND && type != VCT_TREAD_WHEEL_IDLER) {
            continue;
        }
        const vehicle_component_data& component_data = data->sub_info->m_components[i];
        matrix rotation = component_data.m_rotation;
        rotation.unrotate(vp->orient);
        vector position = component_data.m_position;
        position.unrotate(vp->orient);
        position.x += vp->pos.x;
        position.y += vp->pos.y;
        position.z += vp->pos.z;
        vehicle_component::tread_wheel_attr& wheel = component.m_type_attribute.tread_wheel;
        float travel;
        if (wheel.spline_index == -1) {
            vector point_velocity;
            havok_body_get_point_velocity(vp->havok_handle, position, point_velocity);
            travel = (point_velocity.x * vp->orient.fvec.x + point_velocity.y * vp->orient.fvec.y + point_velocity.z * vp->orient.fvec.z) * Frametime;
        }
        else {
            travel = spline_travel[wheel.spline_index];
        }
        if (fabsf(travel) < 0.001f) {
            travel = 0.0f;
        }
        travel /= wheel.wheel_radius;
        if (0.7853982f < travel) { // past 45 degrees a frame: blur the spin by up to 11.25
            travel += fl_rand_range(-0.19634955f, 0.19634955f);
        }
        wheel.wheel_rotation += travel;
        if (6.2831855f < wheel.wheel_rotation) { // 2 pi
            wheel.wheel_rotation -= 6.2831855f;
        }
        rotation.rotate_around_local_x(wheel.wheel_rotation);
        if (type == VCT_TREAD_WHEEL_GROUND && phantom != NULL && wheel_collide != NULL) {
            vector start;
            vector end;
            float length;
            float radius;
            compute_tread_wheel_ground_cast_positions(vp, &component, start, end, length, radius);
            start.unrotate(vp->orient);
            start.x += vp->pos.x;
            start.y += vp->pos.y;
            start.z += vp->pos.z;
            end.unrotate(vp->orient);
            end.x += vp->pos.x;
            end.y += vp->pos.y;
            end.z += vp->pos.z;
            const vector dir(end.x - start.x, end.y - start.y, end.z - start.z);
            hkpWorldRayCastInput input;
            game_to_havok_vector(input.m_from, start);
            game_to_havok_vector(input.m_to, end);
            input.m_enableShapeCollectionFilter = true;
            input.m_filterInfo = wheel_collide->m_wheelCollisionFilterInfo;
            input.m_userData = 0;
            havok_closest_ray_hit_collector collector;
            phantom->castRay(input, collector);
            const vector up_offset(vp->orient.uvec.x * radius, vp->orient.uvec.y * radius, vp->orient.uvec.z * radius);
            if (collector.hasHit()) {
                const float fraction = collector.getHit().m_hitFraction;
                position.set(dir.x * fraction + start.x + up_offset.x, fraction * dir.y + start.y + up_offset.y, fraction * dir.z + start.z + up_offset.z);
            }
            else {
                position.set(up_offset.x + end.x, up_offset.y + end.y, up_offset.z + end.z);
            }
        }
        component.set_orient_pos_in_world_space(vp, rotation, position);
    }
}

// Lays the track pads along the splines into the frame's spline_render_update. Inlined in rfg.exe.
static void vehicle_postsim_tread_splines(vehicle::resource_dependent_data* data, const float* spline_travel)
{
    const vehicle_subinfo* sub_info = data->sub_info;
    for (unsigned int s = 0; s < sub_info->m_num_splines; s++) {
        // A spline has at most 20 control points in rfg.exe's stack layout, each drawn by 4 chords; nothing checks it.
        vector control_points[23];
        float chord_lengths[82];
        vector chord_points[82];
        vector chord_normals[82];
        const vehicle_subinfo::vehicle_spline_data& spline_data = sub_info->m_spline_data[s];
        int num_points = 0;
        for (; num_points < (int)spline_data.num_control_points; num_points++) {
            vector& point = control_points[num_points];
            point = spline_data.control_points[num_points];
            const int attached = spline_data.attached_component_index[num_points];
            if (attached >= 0) {
                const vector& deformed = data->m_components[attached].get_chassis_space_position();
                const vector& original = sub_info->m_components[attached].m_position;
                point.x += deformed.x - original.x;
                point.y += deformed.y - original.y;
                point.z += deformed.z - original.z;
            }
        }
        // closed by repeating the first three
        control_points[num_points] = control_points[0];
        control_points[num_points + 1] = control_points[1];
        control_points[num_points + 2] = control_points[2];
        cardinal_spline spline;
        spline.set_tension(0.0f);
        spline.set_points(num_points + 3, control_points);
        const float tread_width = sub_info->m_tread_width;
        float total_length = 0.0f;
        chord_lengths[0] = 0.0f;
        vector tangent;
        spline.get_point(chord_points[0], 1.0f);
        spline.get_tangent(tangent, 1.0f);
        tangent.normalize();
        chord_normals[0].set(tangent.y * RIGHT_VECTOR.z - tangent.z * RIGHT_VECTOR.y, tangent.z * RIGHT_VECTOR.x - tangent.x * RIGHT_VECTOR.z, tangent.x * RIGHT_VECTOR.y - tangent.y * RIGHT_VECTOR.x);
        int chord = 0;
        for (int p = 0; p < num_points; p++) {
            vector last = control_points[p + 1];
            chord_points[chord] = last;
            for (int q = 1; q < 5; q++) {
                const float u = (float)q * 0.25f + (float)(p + 1);
                vector point;
                spline.get_point(point, u);
                spline.get_tangent(tangent, u);
                tangent.normalize();
                chord_points[chord + 1] = point;
                chord_normals[chord + 1].set(tangent.y * RIGHT_VECTOR.z - tangent.z * RIGHT_VECTOR.y, tangent.z * RIGHT_VECTOR.x - tangent.x * RIGHT_VECTOR.z, tangent.x * RIGHT_VECTOR.y - tangent.y * RIGHT_VECTOR.x);
                const float dist = vec_dist(point, last);
                chord_lengths[chord + 1] = dist;
                total_length += dist;
                last = point;
                chord++;
            }
        }
        const int last_chord = num_points * 4;
        chord_points[last_chord + 1] = chord_points[1];
        chord_normals[last_chord + 1] = chord_normals[1];
        chord_lengths[last_chord + 1] = chord_lengths[1];
        chord_lengths[last_chord + 2] = 0.0f;
        const int max_chord = last_chord + 1; // (num_points + 3) * 4 - 11
        vehicle::vehicle_spline_data& spline_state = data->spline_data[s];
        if (spline_state.num_treads < 0) {
            spline_state.num_treads = (int)(total_length / tread_width);
        }
        const int num_treads = spline_state.num_treads;
        const float spacing = total_length / (float)num_treads;
        float advance = spline_travel[s] / spacing;
        if (0.8f < advance || advance < -0.8f) { // would strobe: pick something that looks in motion
            advance = fl_rand_range(0.3f, 0.7f);
        }
        const float offset = advance + spline_state.starting_offset_percent;
        spline_state.starting_offset_percent = offset - floorf(offset);
        float along = spacing * spline_state.starting_offset_percent;
        int index = 0;
        for (int t = 0; t < num_treads; t++) {
            while (chord_lengths[index] <= along && index < max_chord) {
                along -= chord_lengths[index];
                index++;
            }
            const float f = along / chord_lengths[index];
            const float r = 1.0f - f;
            const vector& p0 = chord_points[index - 1];
            const vector& p1 = chord_points[index];
            const vector& n0 = chord_normals[index - 1];
            const vector& n1 = chord_normals[index];
            const vector position(p1.x * f + p0.x * r, p1.y * f + p0.y * r, p1.z * f + p0.z * r);
            const vector normal(n0.x * r + f * n1.x, n1.y * f + n0.y * r, n1.z * f + n0.z * r);
            matrix43 transform;
            transform.m_rotation.make_ruvec(RIGHT_VECTOR, normal);
            transform.set_translation(position);
            data->spline_render_update->add_transform(transform);
            along += spacing;
        }
    }
}

// 0x00BC0950 (:9456)
void vehicle_do_frame_postsim(vehicle* vp)
{
    if (vp->obj_flags.destroyed || !vp->rdd.is_loaded()) {
        return;
    }
    vehicle_dequeue_damage(vp);
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    if (vp->m_flags.m_hidden != 0) {
        vehicle_interaction_update_human_positions(vp);
        return;
    }
    vehicle_update_orient_pos_from_havok(vp);
    vehicle_calc_ws_pos_orient_for_components(vp, false);
    vector velocity;
    vehicle_get_velocity(vp, velocity);
    vp->forward_velocity = velocity.y * vp->orient.fvec.y + velocity.x * vp->orient.fvec.x + velocity.z * vp->orient.fvec.z;
    if (vp->get_sub_type() != OT_SUB_VEHICLE_WALKER && -1 < vp->aim_handle) {
        vehicle_postsim_animate(vp, data);
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        automobile_do_frame_postsim((automobile*)vp, true);
    }
    else if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER) {
        flyer_do_frame_postsim((flyer*)vp);
    }
    else if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        walker_do_frame_postsim((walker*)vp);
    }
    vehicle_interaction_update_human_positions(vp);

    hk_custom_vehicle_raycast_wheel_collide* wheel_collide = NULL;
    hkpAabbPhantom* phantom = NULL;
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        // Original bug kept: not checked for loaded data; automobile's stream code keeps it in step with rdd.
        wheel_collide = (hk_custom_vehicle_raycast_wheel_collide*)((automobile*)vp)->a_rdd.get_data()->m_hk_vehicle_instance->m_wheelCollide;
        phantom = wheel_collide->m_phantom;
    }
    const vehicle_subinfo* sub_info = data->sub_info;
    float spline_travel[4];
    for (unsigned int s = 0; s < sub_info->m_num_splines; s++) {
        vector center = sub_info->m_spline_data[s].center;
        center.unrotate(vp->orient);
        center.x += vp->pos.x;
        center.y += vp->pos.y;
        center.z += vp->pos.z;
        vector point_velocity;
        havok_body_get_point_velocity(vp->havok_handle, center, point_velocity);
        float forward = point_velocity.x * vp->orient.fvec.x + point_velocity.y * vp->orient.fvec.y + point_velocity.z * vp->orient.fvec.z;
        if (fabsf(forward) < 0.01f) {
            forward = 0.0f;
        }
        spline_travel[s] = forward * Frametime;
    }
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    if (body != NULL && (body->isActive() || vp->m_flags.fixed_motion != 0)) {
        vehicle_postsim_tread_wheels(vp, data, spline_travel, wheel_collide, phantom);
    }
    if (sub_info->m_num_splines != 0 && -1 < data->track_pad_component_id) {
        if (!body->isActive() && vp->m_flags.fixed_motion == 0) {
            // Original bug kept: the pointer stays, so update_render frees it a second time.
            if (data->spline_render_update != NULL) {
                vehicle_render_free_spline_render_update(data->spline_render_update);
            }
        }
        else {
            if (data->spline_render_update == NULL) {
                data->spline_render_update = vehicle_render_allocate_spline_render_update(); // inlined in rfg.exe
            }
            if (data->spline_render_update == NULL) {
                body->activate();
            }
            else {
                data->spline_render_update->clear();
                vehicle_postsim_tread_splines(data, spline_travel);
            }
        }
    }
    if (!vp->is_killed()) {
        hkpRigidBody* friction_body = havok_body_get_pointer(vp->havok_handle);
        if (friction_body != NULL) {
            // less grip as it tips over: 0.5 upright, down to 0.03 on its side
            friction_body->setFriction(fl_parameter(vp->orient.uvec.y, 0.5f, 0.1f, 0.03f, 0.5f, true));
        }
    }
    vehicle_effects_process_lights(vp);
    vehicle_audio_process_pass_by(vp);
}

// 0x00B79FB0 (:9995)
bool vehicle_post_sim_fade_despawn(vehicle* vp)
{
    if (vp->m_flags.m_despawn_after_fade == 0 || vp->m_flags.m_fading_out != 0 || (Multiplayer && !multi_is_server())) {
        return false;
    }
    if (vp->is_killed()) {
        for (int i = 0; i < 8; i++) {
            human* hp = vehicle_human_from_handle(vp->vii.humans[i].handle);
            if (hp != NULL && !hp->is_killed()) {
                human_damage(hp, NULL, -1.0f, DBT_NONE, NULL, NULL, true, false);
                if (hp->get_sub_type() == OT_SUB_HUMAN_NPC && ((npc*)hp)->get_ai() != NULL) {
                    ((npc*)hp)->get_ai()->do_death_func();
                }
            }
        }
    }
    World.destroy_object(vp, false);
    return true;
}

// 0x00B7A0A0 (:10046)
void vehicle_post_sim_update_corpse(vehicle* vp)
{
    vehicle_update_orient_pos_from_havok(vp);
    if (mission_is_active() && Vehicle_system_avoid_despawn && vp != NULL && object_is_local_player(vp->most_recent_driver)) {
        return;
    }
    if (vp->m_flags.m_despawn_after_fade != 0) {
        return;
    }
    if (vp->m_corpse_absolute_longest_timer.elapsed()) {
        vehicle_fade_out(vp, 1000, true, false);
        return;
    }
    if (vp->m_corpse_timer.elapsed()) {
        if (70.0f < vp->render_distance.get_apparent_distance(vp->pos)) {
            vehicle_fade_out(vp, 1000, true, false);
            return;
        }
        if (vp->is_offscreen()) {
            World.destroy_object(vp, false);
            return;
        }
        vp->m_corpse_timer.set_rand(500, 1500);
    }
}

// 0x00B97BF0 (:10089)
void vehicle_free_havok_data(vehicle* vp)
{
    if (!vp->rdd.is_loaded()) {
        return;
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        hkpConstraintInstance*& constraint = data->m_components[i].m_constraint;
        if (constraint != NULL) {
            havok_constraint_release(constraint);
            constraint = NULL;
        }
    }
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        data->m_components[i].free_rigid_body_and_constraints(vp);
    }
    havok_body_free(vp->havok_handle, true, true, false);
    vp->m_flags.fixed_motion = 0;
    vp->havok_handle = 0xFFFFFFFF;
}

// 0x00B65980 (:10123)
unsigned int vehicle_get_collision_filter(const vehicle* vp)
{
    hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
    return body != NULL ? body->getCollidable()->getCollisionFilterInfo() : (unsigned int)COLLIDE_VEHICLE;
}

// 0x00B7A1A0 (:10133)
unsigned int vehicle_get_collision_filter(const vehicle* vp, unsigned int alt_body_idx, unsigned int shapekey)
{
    const unsigned int alt_handle = vp->get_alt_hk_body_handle(alt_body_idx);
    if (alt_handle == 0xFFFFFFFF) {
        return vehicle_get_collision_filter(vp);
    }
    hkpRigidBody* body = havok_body_get_pointer(alt_handle);
    if (body != NULL) {
        const havok_filter_shape_collection* collection = havok_filter_shape_collection_get_from_rigid_body(body);
        if (collection != NULL && (int)shapekey < collection->m_childInfo.getSize()) {
            return collection->getCollisionFilterInfo(shapekey);
        }
    }
    return 0xFFFFFFFF;
}

// How many shape keys a component's collision model takes (disabled children still hold theirs).
static unsigned int vehicle_cm_num_shape_keys(const vehicle_component_data* data, bool& has_shape)
{
    const hkpShape* shape = havok_cm_manager_get(data->m_cm_dynamics_info.m_cm_index);
    has_shape = shape != NULL;
    if (shape == NULL) {
        return 0;
    }
    if (shape->getType() == hkcdShapeType::LIST) {
        const hkpListShape* list = (const hkpListShape*)shape;
        return list->getNumChildShapes() + list->m_numDisabledChildren;
    }
    return 1;
}

// Walks down from a component to the one a shape key falls in: -1 when it is past the subtree. Inlined in rfg.exe.
static unsigned int vehicle_component_index_from_key(const vehicle* vp, vehicle::resource_dependent_data* data, unsigned int index, unsigned int key)
{
    unsigned int num_shapes = data->m_components[index].get_comp_num_shapes(vp);
    while (num_shapes <= key) {
        key -= num_shapes;
        const vehicle_component_data* parent_data = data->m_components[index].get_data();
        unsigned int c = 0;
        for (; c < parent_data->m_num_children; c++) {
            const unsigned int child = parent_data->m_children_indices[c];
            if ((data->m_components[child].get_flags() & VCF_SHAPE_IN_PARENT) != 0) {
                bool has_shape;
                const unsigned int child_keys = vehicle_cm_num_shape_keys(data->m_components[child].get_data(), has_shape);
                if (has_shape && key < child_keys) {
                    index = child;
                    break;
                }
                key -= child_keys;
            }
        }
        if (c == parent_data->m_num_children) {
            return 0xFFFFFFFF;
        }
        num_shapes = data->m_components[index].get_comp_num_shapes(vp);
    }
    return index;
}

// 0x00B8D0A0 (:10158)
unsigned int vehicle_get_component_index_from_shape_key(const vehicle* vp, unsigned int shape_key, unsigned int alt_body_index)
{
    vehicle::resource_dependent_data* data = const_cast<vehicle*>(vp)->rdd.get_data();
    const bool on_alt_body = alt_body_index != 0xFFFFFFFF && alt_body_index != 0 && (!vp->rdd.is_loaded() || alt_body_index - 1 < data->m_num_components) && alt_body_index - 1 != 0xFFFFFFFF;
    if (on_alt_body) {
        const unsigned int index = alt_body_index - 1;
        const vehicle_component& component = data->m_components[index];
        if (component.havok_handle == 0xFFFFFFFF) {
            return 0xFFFFFFFF;
        }
        if (shape_key != 0xFFFFFFFF) {
            if (component.get_node_num_shapes(vp) <= shape_key) {
                return 0xFFFFFFFF;
            }
            return vehicle_component_index_from_key(vp, data, index, shape_key);
        }
        return index;
    }
    if (shape_key == 0xFFFFFFFF || shape_key < vp->m_num_sub_pieces) {
        return 0xFFFFFFFF;
    }
    // in the chassis body: past its own sub pieces come the root components' pieces, in order
    unsigned int key = shape_key - vp->m_num_sub_pieces;
    const unsigned int num_components = data->sub_info->m_num_components;
    unsigned int root = 0;
    for (; root < num_components; root++) {
        const vehicle_component& component = data->m_components[root];
        if (component.get_data()->m_parent_index == 0xFFFFFFFF && (component.get_flags() & VCF_SHAPE_IN_PARENT) != 0) {
            const unsigned int pieces = component.get_data()->m_parent_num_sub_pieces;
            if (key < pieces) {
                break;
            }
            key -= pieces;
        }
    }
    if (root == num_components) {
        return 0xFFFFFFFF;
    }
    return vehicle_component_index_from_key(vp, data, root, key);
}

// 0x00B7A210 (:10404)
void vehicle_enable_human_door_collisions(vehicle* vp, bool enable)
{
    if (!vp->rdd.is_loaded()) {
        return;
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_component& component = data->m_components[i];
        const unsigned int flags = component.get_flags();
        // bit 2: a door with its own body
        if (component.get_data()->m_type == VCT_DOOR && (flags & 4) != 0 && (flags & VCF_DETACHED) == 0) {
            vehicle_component_enable_collision_with_humans(&component, enable);
        }
    }
}

// 0x00B7A280 (LTCG: npc_handle in EAX)
static void notify_npc_of_vehicle_on_fire(vehicle* vp, unsigned int npc_handle)
{
    human* hp = vehicle_human_from_handle(npc_handle);
    if (hp != NULL && hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed()) {
        ((npc*)hp)->get_ai()->perception.register_vehicle_is_on_fire(vp);
    }
}

// 0x00B8D340 (:10442)
void vehicle_catch_fire(vehicle* vp, bool from_nano)
{
    if (vp->m_flags.m_on_fire != 0 || vp->is_killed()) {
        return;
    }
    vp->m_damage_areas[0].m_hit_points = 750; // what the fire burns down from, see vehicle_engine_on_fire_do_frame
    float rate;
    if (vehicle_player_is_occupant(vp)) {
        vp->m_flags.m_only_fire_damage = 1;
        const base_vehicle_types classification = vp->info->vehicle_classification;
        if (classification == BVT_TANK || classification == BVT_ART_TANK || vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
            rate = 75.0f; // 10 s to burn out
        }
        else {
            rate = 107.14286f; // 7 s
        }
    }
    else {
        rate = 150.0f; // 5 s
    }
    vp->m_fire_damage_rate = rate;
    vp->m_fire_fractional_damage = 0.0f;
    vehicle_set_smoking_and_fire(vp->get_handle(), false, true, from_nano);
    const vehicle_interaction_info* vi_info = vp->info->vi_info;
    const int num_seats = vi_info != NULL ? vi_info->num_seats : 1;
    for (int i = 0; i < num_seats; i++) {
        notify_npc_of_vehicle_on_fire(vp, vp->m_seat_info[i].m_current_occupant_handle);
    }
}

// 0x00B7A2D0 (:10490)
void vehicle_switch_headlights(unsigned int vehicle_handle, unsigned int ignored, bool is_on)
{
    vehicle* vp = vehicle_from_handle(vehicle_handle);
    // only for a vehicle with a driver who has not left
    if (vp != NULL && !vp->most_recent_driver_exit_time.valid() && vehicle_human_from_handle(vp->most_recent_driver) != NULL) {
        vp->m_flags.m_headlights_on = is_on;
    }
}

// 0x00B7A360 (:10614)
void vehicle_detach_turrets(vehicle* vp)
{
    for (int i = 0; i < vp->m_num_turret_mounts; i++) {
        turret* tp = World.object_from_handle<turret>(vp->m_turret_mounts[i].attached_turret);
        if (tp != NULL) {
            tp->kill(NULL, NULL);
        }
    }
    vp->m_num_turret_mounts = 0;
}

// 0x00B659A0 (:10647)
bool vehicle_engine_is_running(const vehicle* const vp)
{
    return vp != NULL && vp->m_flags.m_engine_running != 0;
}

// 0x00B8D470 (:10659)
void vehicle_set_driver_inputs_due_to_crash(vehicle* vp, float steering_param)
{
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        ((automobile*)vp)->a_rdd.get_data()->m_hk_vehicle_instance->set_inputs_due_to_crash(steering_param);
    }
}

// 0x00B659C0 (:10678)
float calculate_vehicle_steering_param_for_crash(const vehicle* p_vehicle, const vector* collision_point, const vector* collision_direction)
{
    hkpRigidBody* body = havok_body_get_pointer(p_vehicle->havok_handle);
    vector center_of_mass;
    havok_to_game_vector(center_of_mass, body->getCenterOfMassInWorld());
    const matrix& orient = p_vehicle->orient;
    if (orient.uvec.y < 0.8f) { // on its side or roof
        return 0.0f;
    }
    const float forward = orient.fvec.x * collision_direction->x + collision_direction->y * orient.fvec.y + collision_direction->z * orient.fvec.z;
    vector to_point(collision_point->x - center_of_mass.x, collision_point->y - center_of_mass.y, collision_point->z - center_of_mass.z);
    to_point.normalize_safe();
    const vector cross(collision_direction->y * to_point.z - collision_direction->z * to_point.y, collision_direction->z * to_point.x - collision_direction->x * to_point.z, collision_direction->x * to_point.y - collision_direction->y * to_point.x);
    const bool left_turn = cross.x * orient.uvec.x + cross.y * orient.uvec.y + cross.z * orient.uvec.z < 0.0f;
    if (forward < 0.0f) {
        return left_turn ? -1.0f : 1.0f;
    }
    return left_turn ? 1.0f : -1.0f;
}

// 0x00B65C70 (:10726)
float vehicle_get_dist_to_back_bumper(const vehicle* vp)
{
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    return -bmin.z;
}

// 0x00B65CA0 (:10741)
float vehicle_get_dist_to_front_bumper(const vehicle* vp)
{
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    return bmax.z;
}

// 0x00B65CC0 (:10755)
void vehicle_set_bomb_status(vehicle* vp, vehicle_bomb_status status)
{
    if (vp != NULL) {
        vp->m_bomb_status = status;
        if (status != VEHICLE_BOMB_NONE) {
            activity_car_bomb_start(NULL);
        }
    }
}

// 0x00B65CE0 (:10771)
void vehicle_set_scripted_car_bomb(vehicle* vp)
{
    if (vp != NULL) {
        vp->m_bomb_status = VEHICLE_BOMB_ARMED;
    }
}

// 0x00B7A3D0 (:10783)
vehicle_ai* vehicle_get_vehicle_ai(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL ? &vp->vai : NULL;
}

// 0x00B65D00 (:10796)
float vehicle::get_vehicle_opacity() const
{
    const int elapsed_ms = m_fade_timer.time_since();
    if (m_flags.m_fading_in != 0) {
        return elapsed_ms < m_fade_time ? (float)elapsed_ms / (float)m_fade_time : 1.0f;
    }
    if (m_flags.m_fading_out != 0 && m_flags.m_fading_from_nano == 0) {
        return elapsed_ms < m_fade_time ? 1.0f - (float)elapsed_ms / (float)m_fade_time : 0.0f;
    }
    return 1.0f;
}

// 0x00B65DA0 (:10823)
bool vehicle::created_from_managed_layer() const
{
    // the handle's top bit of its third byte marks a managed layer (0x80 of ((handle >> 16) & 0xFF))
    if (spawn_node_handle == 0xFFFFFFFF) {
        return false;
    }
    return 0x80 <= ((spawn_node_handle >> 16) & 0xFF);
}

// 0x00BB3AC0 (:10835)
void vehicle::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    object::update_pos_and_orient(update_pos, update_orient, set_havok_data);
    if (!set_havok_data) {
        return;
    }
    if (get_sub_type() == OT_SUB_VEHICLE_AUTO && ((automobile*)this)->a_rdd.is_loaded()) {
        ((hk_custom_vehicle_raycast_wheel_collide*)((automobile*)this)->a_rdd.get_data()->m_hk_vehicle_instance->m_wheelCollide)->clear_wheel_caches();
    }
    if (rdd.is_loaded()) {
        resource_dependent_data* data = rdd.get_data();
        for (unsigned int i = 0; i < data->m_num_components; i++) {
            vehicle_component& component = data->m_components[i];
            if (component.havok_handle == 0xFFFFFFFF) {
                continue;
            }
            vector position = component.get_chassis_space_position();
            matrix rotation = component.get_chassis_space_rotation();
            position.unrotate(update_orient);
            position.x += update_pos.x;
            position.y += update_pos.y;
            position.z += update_pos.z;
            rotation.unrotate(update_orient);
            component.set_orient_pos_in_world_space(this, rotation, position);
            havok_body_set_pos_orient(component.havok_handle, position, rotation);
        }
        for (int i = 0; i < m_num_turret_mounts; i++) {
            turret* tp = World.object_from_handle<turret>(m_turret_mounts[i].attached_turret);
            if (tp != NULL) {
                const int tag_index = get_tag_index_from_name(m_turret_mounts[i].info->prop_point);
                if (tag_index != -1) {
                    vector tag_pos;
                    matrix tag_orient;
                    get_tag_world_coords(tag_index, tag_pos, &tag_orient, NULL, NULL);
                    tp->snap_pos_and_orient(tag_pos, tag_orient);
                }
            }
        }
    }
    if (m_flags.m_occupied != 0) {
        vehicle_interaction_teleport_human_positions(this);
    }
}

// 0x00B9DAF0 (:10896)
void vehicle::update_render(rl_renderer*)
{
    if (!rdd.is_loaded()) {
        return;
    }
    resource_dependent_data* data = rdd.get_data();
    float opacity = 1.0f;
    int lod = 0;
    const bool visible = obj_flags._dont_use_me != 0;
    if (visible) {
        const float apparent_distance = render_distance.get_apparent_distance(pos);
        for (lod = 0; lod < 4; lod++) {
            if (apparent_distance <= ms_lod_distances[lod].dist) {
                break;
            }
        }
        if (2 < lod) {
            lod = 3;
        }
        opacity = get_vehicle_opacity();
        if (m_flags.m_fading_in != 0) {
            if (m_fade_time <= m_fade_timer.time_since()) {
                m_flags.m_fading_in = 0;
            }
        }
        else if (m_flags.m_fading_out != 0 && m_fade_time <= m_fade_timer.time_since()) {
            m_flags.m_fading_out = 0;
            if (m_flags.m_fading_from_nano != 0) {
                set_is_visible(false);
            }
            m_flags.m_fading_from_nano = 0;
        }
    }
    rl_deformable_mesh_instance* chassis = data->m_renderable_instance_p;
    const vector* treads_pos = NULL;
    const matrix* treads_orient = NULL;
    if (!visible) {
        if (chassis->get_is_visible()) {
            chassis->set_is_visible(false);
        }
    }
    else {
        if (!chassis->get_is_visible()) {
            chassis->set_is_visible(true);
        }
        // a live walker draws where its animation puts the chassis
        if (get_sub_type() == OT_SUB_VEHICLE_WALKER && 0 < m_damage_areas[0].m_hit_points) {
            treads_pos = &((walker*)this)->chassis_pos;
            treads_orient = &((walker*)this)->chassis_orient;
        }
        else {
            treads_pos = &pos;
            treads_orient = &orient;
        }
        chassis->set_position(*treads_pos);
        chassis->set_rotation(*treads_orient);
        chassis->set_opacity(opacity);
        chassis->set_lod_level(lod);
        game_render_scene_update(chassis);
    }

    // Sub instances of the chassis mesh to turn on and off. rfg.exe allocates these on the stack by
    // (m_num_components + m_num_vehicle_lights * 2) shorts; at most 66 + 20 * 2.
    short activate[106];
    short deactivate[106];
    int num_activate = 0;
    int num_deactivate = 0;
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        vehicle_light& light = data->m_vehicle_lights[i];
        if (light.mesh_light == NULL || !light.flags.dirty) {
            continue;
        }
        if (light.parent_idx != -1) {
            const vehicle_component& parent_component = data->m_components[light.parent_idx];
            const unsigned int hidden = parent_component.m_renderable_instance_p == NULL ? parent_component.get_flags() & VCF_DETACHED : parent_component.get_flags() & VCF_ACTIVE_IN_CHASSIS_MESH;
            if (hidden != 0) {
                deactivate[num_deactivate] = (short)light.mesh_index_off;
                deactivate[num_deactivate + 1] = (short)light.mesh_index_on;
                num_deactivate += 2;
            }
        }
        const bool off_on = !light.flags.broken && !light.flags.is_on;
        const bool on_on = !light.flags.broken && light.flags.is_on;
        if (light.component_mesh_index_off == -1) {
            if (off_on) {
                activate[num_activate++] = (short)light.mesh_index_off;
            }
            else {
                deactivate[num_deactivate++] = (short)light.mesh_index_off;
            }
        }
        else if (light.parent_idx != -1 && data->m_components[light.parent_idx].m_renderable_instance_p != NULL) {
            short index = (short)light.component_mesh_index_off;
            data->m_components[light.parent_idx].m_renderable_instance_p->set_sub_instance_is_active(&index, 1, off_on);
        }
        if (light.component_mesh_index_on == -1) {
            if (on_on) {
                activate[num_activate++] = (short)light.mesh_index_on;
            }
            else {
                deactivate[num_deactivate++] = (short)light.mesh_index_on;
            }
        }
        else if (light.parent_idx != -1 && data->m_components[light.parent_idx].m_renderable_instance_p != NULL) {
            short index = (short)light.component_mesh_index_on;
            data->m_components[light.parent_idx].m_renderable_instance_p->set_sub_instance_is_active(&index, 1, on_on);
        }
    }
    const unsigned int VCF_HIDES_CHASSIS_SUB_INSTANCE = VCF_DETACHED | VCF_NOT_RENDERED | VCF_NO_CHASSIS_MESH; // 0x2A0000
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_component& component = data->m_components[i];
        rl_deformable_mesh_instance* instance = component.m_renderable_instance_p;
        const unsigned int flags = component.get_flags();
        if (instance == NULL) {
            // no instance of its own: its part of the chassis mesh
            if ((flags & VCF_HIDES_CHASSIS_SUB_INSTANCE) != 0 && (flags & VCF_ACTIVE_IN_CHASSIS_MESH) != 0) {
                deactivate[num_deactivate++] = component.m_sub_instance_idx;
                component.clear_flags(VCF_ACTIVE_IN_CHASSIS_MESH);
            }
            else if ((flags & VCF_HIDES_CHASSIS_SUB_INSTANCE) == 0 && (flags & VCF_ACTIVE_IN_CHASSIS_MESH) == 0 && component.m_sub_instance_idx != -1) {
                activate[num_activate++] = component.m_sub_instance_idx;
                component.set_flags(VCF_ACTIVE_IN_CHASSIS_MESH);
            }
            continue;
        }
        if ((flags & VCF_ACTIVE_IN_CHASSIS_MESH) != 0) {
            deactivate[num_deactivate++] = component.m_sub_instance_idx;
            component.clear_flags(VCF_ACTIVE_IN_CHASSIS_MESH);
        }
        if (!visible || (component.get_flags() & (VCF_DETACHED | VCF_NOT_RENDERED)) != 0) {
            if (instance->get_is_visible()) {
                instance->set_is_visible(false);
            }
            continue;
        }
        if (!instance->get_is_visible()) {
            instance->set_is_visible(true);
        }
        if (component.get_data()->m_type == VCT_TREAD_SEGMENT && treads_pos != NULL && treads_orient != NULL) {
            instance->set_position(*treads_pos);
            instance->set_rotation(*treads_orient);
        }
        else {
            instance->set_position(component.get_world_space_position());
            instance->set_rotation(component.get_world_space_rotation());
        }
        instance->set_opacity(opacity);
        instance->set_lod_level(lod);
        game_render_scene_update(instance);
    }
    chassis->set_sub_instance_is_active(activate, num_activate, true);
    chassis->set_sub_instance_is_active(deactivate, num_deactivate, false);

    // track pads: the transforms the post simulation step laid out, the rest of the 256 turned off
    if (data->sub_info->m_num_splines != 0 && -1 < data->track_pad_component_id && data->spline_render_update != NULL) {
        rl_deformable_mesh_instance* track_pad = data->m_components[data->track_pad_component_id].m_renderable_instance_p;
        if (track_pad != NULL) {
            rl_sub_instance_update updates[256];
            for (int i = 0; i < 256; i++) {
                updates[i].m_info.m_disable_deferred = 0;
            }
            const unsigned int submesh_id = data->sub_info->m_components[data->track_pad_component_id].m_submesh_id;
            const vehicle_spline_render_update* spline_update = data->spline_render_update;
            int num_updates = 0;
            for (; num_updates < spline_update->num_objects; num_updates++) {
                updates[num_updates].m_info.m_is_active = 1;
                updates[num_updates].m_base_index = num_updates;
                updates[num_updates].m_info.m_submesh_id = submesh_id;
                updates[num_updates].m_info.m_transform = spline_update->transform[num_updates];
            }
            for (; num_updates < 256; num_updates++) {
                updates[num_updates].m_info.m_is_active = 0;
                updates[num_updates].m_info.m_transform = IDENTITY_MATRIX43;
                updates[num_updates].m_base_index = num_updates;
                updates[num_updates].m_info.m_submesh_id = submesh_id;
            }
            track_pad->update_instances(updates, num_updates);
            vehicle_render_free_spline_render_update(data->spline_render_update); // inlined in rfg.exe
            data->spline_render_update = NULL;
        }
    }
    if (Vehicle_deformation_enabled) {
        vehicle_deform_update_render(this);
    }
}

// 0x00B65DD0 (:11135)
bool vehicle::ready_to_stream_out()
{
    if (engine_started_inst != -1) {
        AUDIOLIB_VEHICLE_ENGINE_STATE state;
        if (audiolib_vehicle_engine_get_state(engine_started_inst, &state) == AUDIOLIB_ERR_NONE) {
            return state == (AUDIOLIB_VEHICLE_ENGINE_STATE)3; // stopped
        }
    }
    return true;
}

// 0x00B7A400 (:11163)
void vehicle::disable_turret_fov_limits()
{
    for (int i = 0; i < 2; i++) {
        turret* tp = World.object_from_handle<turret>(m_turret_mounts[i].attached_turret);
        if (tp != NULL) {
            tp->disable_fov_limits();
        }
    }
}

// 0x00B8D4B0 (:11173)
void vehicle_set_controls_when_abandoned(const vehicle* vp)
{
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        automobile_set_controls_when_abandoned((automobile*)vp);
    }
    else if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER) {
        ((flyer*)vp)->req_lookat_isvalid = false;
    }
    else if (vp->get_sub_type() != OT_SUB_VEHICLE_WALKER) {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x00B8D4F0 (:11199)
void vehicle_set_controls_when_abandoned(unsigned int v_handle)
{
    // Original bug kept: not checked for null.
    vehicle_set_controls_when_abandoned((const vehicle*)vehicle_from_handle(v_handle));
}

// 0x00BB75D0 (:11206)
void vehicle_mp_force_blow_up(vehicle* vp)
{
    human* attacker = (human*)World.object_from_handle(vp->m_killer_handle);
    vehicle_blow_up(vp, attacker, vp->m_flags.m_kill_by_vehicle != 0, vp->m_killer_weapon, true);
}

// 0x00B7A440 (:11214)
bool vehicle_is_reserved(const vehicle* vp)
{
    const vehicle* live = vehicle_from_handle(vp->get_handle());
    if (live != NULL && vehicle_passenger_in_or_entering_seat(live, VSI_DRIVER) != NULL) {
        return true;
    }
    return vehicle_human_from_handle(vp->reserved_by) != NULL;
}

// 0x00BBBBA0 (:11234)
void vehicle_reserve_vehicle(vehicle* vp, human* hp)
{
    vi_enter_data parameters;
    parameters.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
    parameters.specified_seat = VSI_DRIVER;
    *(unsigned int*)&parameters.flags = 8; // only reserve the seat
    parameters.destination_seat = VSI_INVALID;
    vehicle_interaction_request_enter_internal(hp, vp, &parameters);
}

// 0x00B7A4A0 (:11247)
bool is_vehicle_on_bridge(vehicle* vp, unsigned int& navpoint_handle)
{
    if (vp == NULL) {
        return false;
    }
    navpoint_handle = 0xFFFFFFFF;
    if (vp->get_sub_type() != OT_SUB_VEHICLE_AUTO) {
        return false;
    }
    const unsigned int mover_handle = ((automobile*)vp)->find_supporting_rfg_mover();
    if (mover_handle == 0xFFFFFFFF) {
        return false;
    }
    object* mover = World.object_from_handle(mover_handle);
    // a fixed rfg_mover: its first child navpoint of the bridge type
    if (mover == NULL || mover->get_obj_type() != OT_MOVER || mover->get_sub_type() != OT_SUB_MOVER_RFG || ((rfg_mover*)mover)->get_move_type() != RFGM_FIXED) {
        return false;
    }
    object* first = mover->child_ptr;
    object* child = first;
    while (child != NULL) {
        const navpoint* point = (const navpoint*)child;
        // 22: bridge navpoint, with its link of the same type
        if (child->get_obj_type() == OT_NAVPOINT && point->type == 22 && 0 < point->num_links && point->links[0] != NULL && point->links[0]->type == 22) {
            vector closest;
            const float dist_sqr = vec_find_closest_dist_squared_to_linesegment(vp->pos, point->pos, point->links[0]->pos, &closest);
            float radius = point->radius;
            if (radius < point->links[0]->radius) {
                radius = point->links[0]->radius;
            }
            if (dist_sqr < radius * radius) {
                navpoint_handle = child->get_handle();
                return true;
            }
            return false;
        }
        child = child->child_next;
        if (child == first) {
            break;
        }
    }
    return false;
}

// 0x00B7A5E0 (:11314)
int vehicle_get_smoke_threshold(vehicle_damage_area* engine)
{
    const int threshold = (int)((float)engine->m_max_hit_points * 0.3f);
    return threshold < 0x6D7 ? 0x6D6 : threshold; // at least 1750, 1000 above the 750 hit points a fire starts at
}

// 0x00B7A610 (:11324)
float vehicle_get_smoke_amount(vehicle_damage_area* engine)
{
    const int threshold = vehicle_get_smoke_threshold(engine); // inlined in rfg.exe
    return 1.0f - (float)(engine->m_hit_points - 750) / (float)(threshold - 750);
}
