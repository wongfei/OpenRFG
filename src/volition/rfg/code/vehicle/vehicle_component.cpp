// Original: volition/rfg/code/vehicle/vehicle_component.cpp
//
// The breakable parts of a vehicle: hit points scaled by durability, damage (queued while Havok simulates), hinged doors
// on limited hinge constraints with a velocity motor, detaching into debris, glass shatter, renderable instances taken
// out of the chassis mesh, and the shared shape keys of the chassis body. Compiled in the vehicle unity TU.

#include "volition/rfg/code/vehicle/vehicle_component.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/limitedhinge/hkplimitedhingeconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/velocity/hkpvelocityconstraintmotor.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/object_debris.h"

// vehicle_component::get_flags() bits used only here (the rest are in vehicle_class.cpp; rfg.pdb has no names)
static const unsigned int VCF_HAS_POSITION = 0x1;
static const unsigned int VCF_HAS_ROTATION = 0x2;
static const unsigned int VCF_HINGED = 0x4;
static const unsigned int VCF_DEFORM_POSITION = 0x8;
static const unsigned int VCF_DEFORM_ROTATION = 0x10;
static const unsigned int VCF_NO_DEBRIS = 0x80;
static const unsigned int VCF_BLOWS_UP_VEHICLE = 0x100;
static const unsigned int VCF_DEFORMED_HINGE = 0x100000;
static const unsigned int VCF_HINGE_BROKEN = 0x2000000;

// ---- file-static globals
// Durability to the share of the vehicle's maximum hit points.
static const float VEHICLE_COMPONENT_HITPOINT_SCALERS[7] = {0.0f, 0.0f, 0.05f, 0.2f, 0.5f, 1.0f, 1.0f}; // 0x01336CE8
static const float vehicle_light_camera_fudge_distance = 0.25f; // 0x013385D0
static const float air_control_deadzone = 0.2f; // 0x01338680
static int Component_fire_chance = 33; // 0x016BA4E0
static float Vehicle_component_hinge_tau = 0.3f; // 0x016BA4E8
static int Vehicle_door_collision_threshold_mph = 5; // 0x016BA4DC

// size 0x34
struct damaged_component_post_process_info
{
    vehicle_component* m_component; // 0x00
    vehicle* m_vehicle; // 0x04
    damaged_by_types m_damaged_by_type; // 0x08
    vector m_damage_velocity; // 0x0C
    vector m_damage_hit_position; // 0x18
    vector m_damage_hit_normal; // 0x24
    const weapon_info* m_weapon_info; // 0x30
};

static unsigned int Vehicle_shatter_glass_effect; // 0x0376F664
static vehicle_component_accident_info Accidents_this_frame[128]; // 0x0376F9F8
static unsigned int Num_damaged_components_this_frame; // 0x03770034
static damaged_component_post_process_info Damaged_components_this_frame[128]; // 0x0383B948

// The type hashes (declared in vehicle_component_data.h).
unsigned int VCT_MISC = str_hash_string("Misc."); // 0x0381A64C
unsigned int VCT_FRONT_WHEEL = str_hash_string("Front Wheel"); // 0x037701BC
unsigned int VCT_REAR_WHEEL = str_hash_string("Rear Wheel"); // 0x037701D0
unsigned int VCT_TREAD_WHEEL_GROUND = str_hash_string("Tread Wheel Ground"); // 0x03770BA0
unsigned int VCT_TREAD_WHEEL_IDLER = str_hash_string("Tread Wheel Idler"); // 0x0377CC38
unsigned int VCT_DOOR = str_hash_string("Door"); // 0x03770BA8
unsigned int VCT_STEERING_WHEEL = str_hash_string("Steering Wheel"); // 0x03770158
unsigned int VCT_TRACK_SPLINE = str_hash_string("Track Spline"); // 0x0377CC3C
unsigned int VCT_TREAD_SEGMENT = str_hash_string("Tread Segment"); // 0x03770BAC
unsigned int VCT_MUFFLER = str_hash_string("Exhaust Pipe"); // 0x03770204
unsigned int VCT_SPOTLIGHT = str_hash_string("Spotlight"); // 0x03770214
unsigned int VCT_DOWN_THRUSTER = str_hash_string("Down Thruster"); // 0x0384FC0C
unsigned int VCT_TURRET = str_hash_string("Turret Component"); // 0x037701E4
unsigned int VCT_AXLE_3_WHEEL = str_hash_string("Axel Wheel 3"); // 0x03770110
unsigned int VCT_AXLE_4_WHEEL = str_hash_string("Axel Wheel 4"); // 0x0377015C
unsigned int VCT_SWING_ARM = str_hash_string("Swing Arm"); // 0x0383D36C
unsigned int VCT_STEERING_JOINT = str_hash_string("Steering Joint"); // 0x0386BFB4
unsigned int VCT_WHEEL_HUB = str_hash_string("Wheel Hub"); // 0x03770138
unsigned int VCT_WHEEL_COLLISION = str_hash_string("Wheel Collision"); // 0x0386BF44
unsigned int VCT_WING = str_hash_string("Wing"); // 0x037701C0
unsigned int VCT_WHEEL_BUMPER = str_hash_string("Wheel_Bumpers"); // 0x03770108
unsigned int VCT_CHASSIS_COLLISION = str_hash_string("Chassis_Collision"); // 0x0386BFF4
unsigned int VCT_HEADLIGHT = str_hash_string("Headlight_Mesh"); // 0x03770BA4
unsigned int VCT_FOGLIGHT = str_hash_string("Foglight_Mesh"); // 0x037701E8
unsigned int VCT_TAILLIGHT = str_hash_string("Taillight_Mesh"); // 0x037700F4
unsigned int VCT_BRAKELIGHT = str_hash_string("Brakelight_Mesh"); // 0x0386BF28
unsigned int VCT_REVERSELIGHT = str_hash_string("Reverselight_Mesh"); // 0x0381A5CC
unsigned int VCT_EMERGENCYLIGHT = str_hash_string("Emergencylight_Mesh"); // 0x0386BFF8
unsigned int VCT_STROBELIGHT = str_hash_string("Strobelight_Mesh"); // 0x037701E0
unsigned int VCT_TAILLIGHTG = str_hash_string("Taillight_Glare"); // 0x0386BF48
unsigned int VCT_BRAKELIGHTG = str_hash_string("Brakelight_Glare"); // 0x0377010C
unsigned int VCT_EMERGENCYLIGHTG = str_hash_string("Emergencylight_Glare"); // 0x037700F8
unsigned int VCT_REVERSELIGHTG = str_hash_string("Reverselight_Glare"); // 0x0381A648

// A durability's hit points, between 1 and 65535. Inlined in rfg.exe.
static int vehicle_component_durability_hit_points(const vehicle* vp, int durability)
{
    const int hit_points = (int)((float)(unsigned int)vp->m_damage_areas[0].m_max_hit_points * VEHICLE_COMPONENT_HITPOINT_SCALERS[durability]);
    if (hit_points < 1) {
        return 1;
    }
    return hit_points < 0x10000 ? hit_points : 0xFFFF;
}

// How many shape keys a collision model takes (disabled list children keep theirs). Inlined in rfg.exe.
static unsigned int vehicle_shape_key_count(const hkpShape* shape)
{
    if (shape->getType() == hkcdShapeType::LIST) {
        const hkpListShape* list = (const hkpListShape*)shape;
        return list->getNumChildShapes() + list->m_numDisabledChildren;
    }
    return 1;
}

// 0x00B7A670 (:1057)
static void vehicle_component_shatter_children(vehicle_component* component, vehicle* vp, const vector* damage_hit_position, const vector* damage_hit_normal, const vector* damage_velocity)
{
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    // the first 7 glass parts below it, breadth first
    vehicle_component* found[8];
    found[0] = component;
    unsigned int num_found = 1;
    unsigned int next = 0;
    do {
        const vehicle_component_data* parent_data = found[next]->get_data();
        for (unsigned int i = 0; i < parent_data->m_num_children; i++) {
            if (num_found == 8) {
                break;
            }
            vehicle_component* child = &data->m_components[parent_data->m_children_indices[i]];
            if ((child->get_flags() & VCF_GLASS) != 0) {
                found[num_found++] = child;
            }
        }
    } while (num_found != 8 && ++next < num_found);
    for (unsigned int i = 1; i < num_found; i++) {
        vehicle_component_shatter_glass(found[i], vp, damage_hit_position, damage_hit_normal, damage_velocity);
    }
}

// 0x00B8D550 (LTCG: vp in EAX, p_component in ECX, damage_velocity in EDI)
static void vehicle_component_play_shatter_effect(vehicle_component* p_component, vehicle* vp, const vector* damage_velocity)
{
    vector bmin;
    vector bmax;
    ((rl_mesh*)vp->rdd.get_data()->sub_info->m_vehicle_mesh)->get_subpiece_local_bb(p_component->get_data()->m_submesh_id, bmin, bmax);
    const float size_x = bmax.x - bmin.x;
    const float size_y = bmax.y - bmin.y;
    const float size_z = bmax.z - bmin.z;
    // the pane's thinnest axis becomes the effect's forward
    matrix orient = p_component->get_world_space_rotation();
    float side_a = size_y;
    float side_b = size_x;
    if (size_y <= size_x && size_y <= size_z) {
        const vector swap = orient.uvec;
        orient.uvec = orient.fvec;
        orient.fvec = swap;
        side_a = size_z;
    }
    if (size_x <= size_y && size_x <= size_z) {
        const vector swap = orient.rvec;
        orient.rvec = orient.fvec;
        orient.fvec = swap;
        side_b = size_z;
    }
    const unsigned int handle = effects_play(Vehicle_shatter_glass_effect, p_component->get_world_space_position(), orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    object_effect* effect = vehicle_effect_from_handle(handle);
    if (effect != NULL) {
        const float speed = damage_velocity != NULL ? sqrtf(damage_velocity->x * damage_velocity->x + damage_velocity->y * damage_velocity->y + damage_velocity->z * damage_velocity->z) : 1.0f;
        effect->set_expression_variable(EFFECTS_VAR_SCALE_X, size_x * 0.5f);
        effect->set_expression_variable(EFFECTS_VAR_SCALE_Y, size_y * 0.5f);
        effect->set_expression_variable(EFFECTS_VAR_OBJECT_SIZE, sqrtf(side_b * side_b + side_a * side_a));
        effect->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, speed);
    }
}

// 0x00B8D840 (:379)
void vehicle_component::init(const vehicle_component_data& data, const vehicle_subinfo& subinfo, const vehicle* vp, bool reinitialization)
{
    vehicle_component_data* writable = const_cast<vehicle_component_data*>(&data); // the durability below is written into the file's data
    m_data = writable;
    m_flags = data.m_flags;
    havok_handle = 0xFFFFFFFF;
    m_constraint = NULL;
    m_last_hit_by_handle = 0xFFFFFFFF;
    m_debris_object = 0xFFFFFFFF;
    m_pos_orient_deform_weight = 0;
    m_mesh_deform_pending_list = NULL;
    m_numb_shapes = 0xFF;
    m_type_attribute_valid = false;
    const int hit_points = (m_flags & VCF_GLASS) != 0 ? 200 : vehicle_component_durability_hit_points(vp, data.m_durability);
    m_max_hit_points = (unsigned short)hit_points;
    m_hit_points = (unsigned short)hit_points;
    if (data.m_type == VCT_DOOR) {
        m_type_attribute.door.m_state = ADI_NOT_ANIMATING;
        m_type_attribute_valid = true;
    }
    if (data.m_type == VCT_WHEEL_COLLISION || data.m_type == VCT_WHEEL_BUMPER || data.m_type == VCT_CHASSIS_COLLISION) {
        m_flags |= VCF_NO_CHASSIS_MESH;
        writable->m_durability = 6;
    }
    const base_vehicle_types classification = vp->info->vehicle_classification;
    if ((classification == BVT_TANK || classification == BVT_ART_TANK) && (data.m_type == VCT_FRONT_WHEEL || data.m_type == VCT_REAR_WHEEL)) {
        m_flags |= VCF_NO_CHASSIS_MESH;
        writable->m_durability = 6;
    }
}

// 0x00B8D960 (:422)
void vehicle_component::setup(vehicle* vp)
{
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    const vehicle_subinfo* sub_info = data->sub_info;
    const unsigned int type = m_data->m_type;
    if (type == VCT_TREAD_WHEEL_GROUND || type == VCT_TREAD_WHEEL_IDLER) {
        // on the nearest spline (the search starts at 100 m)
        float closest = 10000.0f;
        m_type_attribute_valid = true;
        m_type_attribute.tread_wheel.spline_index = -1;
        for (unsigned int i = 0; i < sub_info->m_num_splines; i++) {
            const float dist = vec_dist_squared(m_cs_position, sub_info->m_spline_data[i].center);
            if (dist < closest) {
                m_type_attribute.tread_wheel.spline_index = (int)i;
                closest = dist;
            }
        }
        vector bmin;
        vector bmax;
        ((rl_mesh*)sub_info->m_vehicle_mesh)->get_subpiece_local_bb(m_data->m_submesh_id, bmin, bmax);
        m_type_attribute.tread_wheel.wheel_radius = (bmax.y - bmin.y) * 0.5f;
        m_type_attribute.tread_wheel.wheel_rotation = fl_rand_range(0.0f, 3.1415927f);
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO && (m_data->m_type == VCT_SWING_ARM || m_data->m_type == VCT_WHEEL_HUB || m_data->m_type == VCT_STEERING_JOINT)) {
        // a suspension part: on the nearest wheel (a swing arm only on one ahead of it)
        automobile::resource_dependent_data* a_data = ((automobile*)vp)->a_rdd.get_data();
        float closest = 10000.0f;
        suspension_attr& suspension = m_type_attribute.suspension;
        suspension.wheel_index = -1;
        suspension.pivot_pos_mag = 0.0f;
        suspension.pivot_pos[0] = 0.0f;
        suspension.pivot_pos[1] = 0.0f;
        suspension.pivot_pos[2] = 0.0f;
        suspension.wheel_bias[0] = 0.0f;
        suspension.wheel_bias[1] = 0.0f;
        suspension.wheel_bias[2] = 0.0f;
        for (int i = 0; (unsigned int)i < a_data->m_num_wheels; i++) {
            const vector& wheel_pos = a_data->m_wheel_components[i]->get_chassis_space_position();
            vector to_wheel(wheel_pos.x - m_cs_position.x, wheel_pos.y - m_cs_position.y, wheel_pos.z - m_cs_position.z);
            const float dist = to_wheel.x * to_wheel.x + to_wheel.y * to_wheel.y + to_wheel.z * to_wheel.z;
            if (dist < closest) {
                to_wheel.rotate(m_cs_rotation);
                if (m_data->m_type != VCT_SWING_ARM || 0.0f < to_wheel.z) {
                    suspension.pivot_pos[0] = to_wheel.x;
                    suspension.pivot_pos[1] = to_wheel.y;
                    suspension.pivot_pos[2] = to_wheel.z;
                    m_type_attribute_valid = true;
                    suspension.wheel_index = i;
                    suspension.pivot_pos_mag = sqrtf(to_wheel.x * to_wheel.x + to_wheel.y * to_wheel.y + to_wheel.z * to_wheel.z);
                    closest = dist;
                }
            }
        }
        if (m_type_attribute_valid) {
            if (m_data->m_type == VCT_SWING_ARM) {
                const int tag = m_data->m_tags.get_tag("suspension_pivot");
                if (-1 < tag) {
                    const vector old_pivot(suspension.pivot_pos[0], suspension.pivot_pos[1], suspension.pivot_pos[2]);
                    matrix43 tag_transform;
                    m_data->m_tags.get_tag_transform(tag_transform, tag);
                    vector bias(tag_transform.m_translation.x - old_pivot.x, tag_transform.m_translation.y - old_pivot.y, tag_transform.m_translation.z - old_pivot.z);
                    bias.unrotate(m_data->m_rotation);
                    suspension.wheel_bias[0] = bias.x;
                    suspension.wheel_bias[1] = bias.y;
                    suspension.wheel_bias[2] = bias.z;
                    suspension.pivot_pos[0] = tag_transform.m_translation.x;
                    suspension.pivot_pos[1] = tag_transform.m_translation.y;
                    suspension.pivot_pos[2] = tag_transform.m_translation.z;
                    suspension.pivot_pos_mag = sqrtf(tag_transform.m_translation.y * tag_transform.m_translation.y + tag_transform.m_translation.x * tag_transform.m_translation.x + tag_transform.m_translation.z * tag_transform.m_translation.z);
                }
            }
            create_renderable_instance(vp);
        }
    }
}

// 0x00B7A720 (:561)
void vehicle_component::set_hitpoints(const vehicle* vp, bool reset_to_max)
{
    const int hit_points = (m_flags & VCF_GLASS) != 0 ? 200 : vehicle_component_durability_hit_points(vp, m_data->m_durability);
    if (reset_to_max) {
        m_max_hit_points = (unsigned short)hit_points;
        m_hit_points = (unsigned short)hit_points;
        return;
    }
    const unsigned short old_max = m_max_hit_points;
    m_max_hit_points = (unsigned short)hit_points;
    m_hit_points = (unsigned short)(int)((float)hit_points * ((float)m_hit_points / (float)old_max));
}

// 0x00B7A7E0 (:593)
void vehicle_component::set_hitpoints_to_durability(const vehicle* vp, int durability)
{
    if (vp != NULL && (m_flags & VCF_GLASS) == 0) {
        const int hit_points = vehicle_component_durability_hit_points(vp, durability);
        m_max_hit_points = (unsigned short)hit_points;
        m_hit_points = (unsigned short)hit_points;
    }
}

// 0x00B65E10 (:617)
void vehicle_component::deinit()
{
    if (m_mesh_deform_pending_list != NULL) {
        mesh_deform_free_deform_spheres(&m_mesh_deform_pending_list);
        m_mesh_deform_pending_list = NULL;
    }
}

// 0x00B7A850 (:627)
void vehicle_component::update_chassis_space(const vehicle* vp)
{
    const vehicle::resource_dependent_data* data = vp->rdd.get_data();
    m_cs_position = m_data->m_position;
    m_cs_rotation = m_data->m_rotation;
    unsigned int parent = m_data->m_parent_index;
    while (parent != 0xFFFFFFFF) {
        const vehicle_component_data* parent_data = data->m_components[parent].get_data();
        m_cs_position.unrotate(parent_data->m_rotation);
        m_cs_position.x += parent_data->m_position.x;
        m_cs_position.y += parent_data->m_position.y;
        m_cs_position.z += parent_data->m_position.z;
        m_cs_rotation.unrotate(parent_data->m_rotation);
        parent = parent_data->m_parent_index;
    }
}

// 0x00B8DDE0 (:655)
void vehicle_component::free_rigid_body_and_constraints(vehicle* vp)
{
    hkpRigidBody* body = havok_body_get_pointer(havok_handle);
    if (body == NULL) {
        return;
    }
    if (m_constraint != NULL) {
        havok_constraint_release(m_constraint);
        m_constraint = NULL;
    }
    const int num_actions = body->m_actions.getSize();
    for (int i = 0; i < num_actions; i++) {
        hkpAction* action = body->m_actions[i];
        if (action != NULL) {
            Havok_world->removeAction(action);
        }
    }
    if (!body->isActive()) {
        body->activate();
    }
    // objects attached to this part go back on the chassis or its nearest parent with a body
    constraint* first = vp->host_constraint_ptr;
    constraint* cp = first;
    if (cp != NULL) {
        vehicle::resource_dependent_data* data = vp->rdd.get_data();
        do {
            if (cp->host_alt_hk_body_index != -1) {
                // to a key of the shared chassis shape
                const int alt = cp->host_alt_hk_body_index;
                // Original bug kept: an out of range index reads the component before the first.
                const int host = (alt == 0 || (vp->rdd.is_loaded() && data->m_num_components <= (unsigned int)(alt - 1))) ? -1 : alt - 1;
                cp->host_attach_index += data->m_components[host].get_first_shape_key(vp);
                cp->host_alt_hk_body_index = -1;
            }
            if ((int)get_first_shape_key(vp) <= cp->host_attach_index && cp->host_attach_index <= (int)get_last_shape_key(vp)) {
                hkpRigidBody* new_body = havok_body_get_pointer(vp->havok_handle);
                for (unsigned int parent = m_data->m_parent_index; parent != 0xFFFFFFFF; parent = data->m_components[parent].get_data()->m_parent_index) {
                    if (data->m_components[parent].havok_handle != 0xFFFFFFFF) {
                        new_body = havok_body_get_pointer(data->m_components[parent].havok_handle);
                        break;
                    }
                }
                object* other = World.object_from_handle(cp->child_handle);
                if (other != NULL) {
                    object_constraint_transfer(true, cp, other, vp, vp, new_body);
                }
            }
            cp = cp->host_constraint_next;
        } while (cp != vp->host_constraint_ptr && cp != NULL);
    }
    havok_body_free(havok_handle, true, true, false);
    havok_handle = 0xFFFFFFFF;
}

// 0x00B65E30 (:735)
void vehicle_component::free_constraint()
{
    if (m_constraint != NULL) {
        havok_constraint_release(m_constraint);
        m_constraint = NULL;
    }
}

// 0x00B7A960 (:753)
float vehicle_component::get_squared_distance(const vehicle* vp, const vector* point) const
{
    vector local(point->x - m_ws_position.x, point->y - m_ws_position.y, point->z - m_ws_position.z);
    local.rotate(m_ws_rotation);
    vector bmin;
    vector bmax;
    ((rl_mesh*)vp->rdd.get_data()->sub_info->m_vehicle_mesh)->get_subpiece_local_bb(m_data->m_submesh_id, bmin, bmax);
    vector closest;
    return bbox_find_closest_point(closest, bmin, bmax, local);
}

// 0x00B7AA40 (:774)
void vehicle_component::get_local_bbox(const vehicle* const vp, vector& local_bmin, vector& local_bmax) const
{
    ((rl_mesh*)vp->rdd.get_data()->sub_info->m_vehicle_mesh)->get_subpiece_local_bb(m_data->m_submesh_id, local_bmin, local_bmax);
}

// 0x00BBFA90 (:795)
bool vehicle_component::apply_damage(vehicle* vp, object* attacker, int hit_points, damaged_by_types from_type, const vector* damage_velocity, const vector* damage_hit_position, const vector* damage_hit_normal, const weapon_info* w_info)
{
    if (m_hit_points == 0 || m_data->m_type == VCT_TURRET) {
        return false;
    }
    m_hit_points -= (unsigned short)hit_points < m_hit_points ? (unsigned short)hit_points : m_hit_points;
    if (hit_points < 0 && m_max_hit_points < m_hit_points) {
        m_hit_points = m_max_hit_points; // repaired
    }
    if (4 < m_data->m_durability) {
        // Original bug kept: the upper bound is a lower bound, so this takes the low 16 bits of the larger of the two.
        unsigned int kept = (unsigned int)vp->m_damage_areas[0].m_hit_points;
        if (kept < m_hit_points) {
            kept = m_hit_points;
        }
        if (kept < 0xFFFF) {
            kept = 0xFFFF;
        }
        m_hit_points = (unsigned short)kept;
    }
    if (m_data->m_durability == 6 && m_hit_points == 0) {
        m_hit_points = 1;
    }
    m_last_hit_by_handle = attacker != NULL ? attacker->get_handle() : 0xFFFFFFFF;
    if (!havok_world_is_simulating()) {
        return process_damage(vp, from_type, damage_velocity, damage_hit_position, damage_hit_normal, w_info);
    }
    // while the world steps it waits for vehicle_components_process_damage
    if (Num_damaged_components_this_frame < 128) {
        for (unsigned int i = 0; i < Num_damaged_components_this_frame; i++) {
            if (Damaged_components_this_frame[i].m_component == this) {
                return false;
            }
        }
        damaged_component_post_process_info& info = Damaged_components_this_frame[Num_damaged_components_this_frame++];
        info.m_damaged_by_type = from_type;
        info.m_component = this;
        info.m_vehicle = vp;
        info.m_weapon_info = w_info;
        if (damage_velocity != NULL) {
            info.m_damage_velocity = *damage_velocity;
        }
        else {
            info.m_damage_velocity.zero();
        }
        if (damage_hit_position != NULL) {
            info.m_damage_hit_position = *damage_hit_position;
        }
        else {
            info.m_damage_hit_position.zero();
        }
        if (damage_hit_normal != NULL) {
            info.m_damage_hit_normal = *damage_hit_normal;
        }
        else {
            info.m_damage_hit_normal.set(0.0f, 1.0f, 0.0f);
        }
    }
    else if (m_hit_points == 0) {
        m_hit_points = 1;
    }
    return false;
}

// 0x00BBBBE0 (:928)
bool vehicle_component::process_damage(vehicle* vp, damaged_by_types from_type, const vector* damage_velocity, const vector* damage_hit_position, const vector* damage_hit_normal, const weapon_info* w_info)
{
    if (m_hit_points == 0 && process_damage_zero_hit_points(vp, from_type, damage_velocity, damage_hit_position, damage_hit_normal, w_info)) {
        return true;
    }
    // a hinged part under a third of its hit points swings free for good
    if ((m_flags & VCF_HINGED) != 0 && (m_flags & VCF_HINGE_BROKEN) == 0 && 0.01f < m_data->m_cm_dynamics_info.m_volume && (float)m_hit_points / (float)m_max_hit_points < 0.33f) {
        m_flags |= VCF_HINGE_BROKEN;
        vehicle_component_unlatch_hinged(this, vp);
        return true;
    }
    return false;
}

// 0x00BB7610 (:968)
bool vehicle_component::process_damage_zero_hit_points(vehicle* vp, damaged_by_types from_type, const vector* damage_velocity, const vector* damage_hit_position, const vector* damage_hit_normal, const weapon_info* w_info)
{
    if (Multiplayer && !multi_is_server()) {
        return true;
    }
    if ((m_flags & VCF_DETACHED) != 0) {
        return true;
    }
    if ((m_flags & VCF_GLASS) != 0) {
        vehicle_component_shatter_glass(this, vp, damage_hit_position, damage_hit_normal, damage_velocity);
        return true;
    }
    bool collided = false;
    if (from_type == DBT_CAR || from_type == DBT_LEVEL_OBJECT || from_type == DBT_WORLD) {
        collided = true;
        // Original bug kept: not checked for null. Under 10 m/s it holds on with a hit point.
        if (damage_velocity->x * damage_velocity->x + damage_velocity->y * damage_velocity->y + damage_velocity->z * damage_velocity->z < 100.0f) {
            m_hit_points = 1;
            return false;
        }
    }
    else if (from_type == DBT_EXPLOSION) {
        if (damage_velocity == NULL || damage_hit_position == NULL) {
            return false;
        }
        // scaled by the part's mass against the chassis, and by both distances from the blast
        const vector from_blast(m_ws_position.x - damage_hit_position->x, m_ws_position.y - damage_hit_position->y, m_ws_position.z - damage_hit_position->z);
        const vector chassis_from_blast(vp->pos.x - damage_hit_position->x, vp->pos.y - damage_hit_position->y, vp->pos.z - damage_hit_position->z);
        const float mass_scale = from_blast.mag_squared() * vp->info->chassis_mass / calc_material_mass(vp);
        const float scale = chassis_from_blast.mag_squared() * mass_scale;
        const vector scaled(scale * damage_velocity->x, damage_velocity->y * scale, scale * damage_velocity->z);
        if (scaled.mag_squared() < 100.0f) {
            m_hit_points = 1;
            return false;
        }
    }
    if (vehicle_component_is_wheel(m_data) || vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        return true;
    }
    const bool from_nano = w_info != NULL && w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE;
    if ((m_flags & VCF_BLOWS_UP_VEHICLE) != 0) {
        m_flags ^= VCF_BLOWS_UP_VEHICLE;
        human* attacker = World.object_from_handle<human>(m_last_hit_by_handle);
        vehicle_blow_up(vp, attacker, from_type == DBT_CAR, w_info, !from_nano);
        return true;
    }
    vp->component_detach(this, true, 0, from_type, false, from_nano);
    if (vp->info->m_effect_component_detach != 0xFFFFFFFF) {
        effects_play(vp->info->m_effect_component_detach, m_ws_position, m_ws_rotation, false, vp->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    vehicle_component_shatter_children(this, vp, damage_hit_position, damage_hit_normal, damage_velocity);
    if (collided) {
        // the crash throws the debris of this part and the first 7 below it
        vehicle::resource_dependent_data* data = vp->rdd.get_data();
        vehicle_component* found[8];
        found[0] = this;
        unsigned int num_found = 1;
        unsigned int next = 0;
        do {
            const vehicle_component_data* parent_data = found[next]->get_data();
            for (unsigned int i = 0; i < parent_data->m_num_children; i++) {
                if (num_found == 8) {
                    break;
                }
                found[num_found++] = &data->m_components[parent_data->m_children_indices[i]];
            }
        } while (num_found != 8 && ++next < num_found);
        for (unsigned int i = 0; i < num_found; i++) {
            if ((found[i]->get_flags() & VCF_GLASS) != 0) {
                continue;
            }
            object* debris = World.object_from_handle(found[i]->m_debris_object);
            if (debris != NULL && debris->havok_handle != 0xFFFFFFFF) {
                const float mass = havok_body_get_mass(debris->havok_handle);
                const vector impulse(mass * damage_velocity->x, damage_velocity->y * mass, mass * damage_velocity->z);
                havok_body_apply_linear_impulse(debris->havok_handle, impulse);
            }
        }
    }
    return true;
}

// 0x00BAA6E0 (:1133)
void vehicle_component::update_animatable_door(float time_step, vehicle* vp)
{
    if (!vp->rdd.is_loaded()) {
        return;
    }
    door_attr& door = m_type_attribute.door;
    door.m_t_actual = get_unlatched_door_t_value(vp);
    hkpLimitedHingeConstraintData* hinge = m_constraint != NULL ? (hkpLimitedHingeConstraintData*)m_constraint->m_data : NULL;
    switch (door.m_state) {
    case ADI_NOT_ANIMATING:
        return;
    case ADI_BEGIN_OPENING:
        if ((m_flags & VCF_UNLATCHED) == 0) {
            vehicle_component_unlatch_hinged(this, vp);
            hinge = m_constraint != NULL ? (hkpLimitedHingeConstraintData*)m_constraint->m_data : NULL;
        }
        if (hinge != NULL) {
            hinge->setMotorEnabled(m_constraint->m_internal->m_runtime, true);
            if (vp->m_flags.fixed_motion != 0 && havok_handle != 0xFFFFFFFF) {
                havok_body_force_activate(havok_handle);
            }
            ((hkpVelocityConstraintMotor*)hinge->m_atoms.m_angMotor.m_motor)->m_velocityTarget = (float)((double)((const vehicle_component_data::hinge*)m_data->m_hinge)->m_max_angle * (double)door.m_t_velocity);
        }
        door.m_state = ADI_OPENING;
        GAME_TRACE("vehicle", "door opening: vehicle=%s component=%d", vp->info->name, (int)(this - vp->rdd.get_data()->m_components));
        door.m_t = door.m_t_actual;
        // fall through
    case ADI_OPENING:
        door.m_t = door.m_t_velocity * time_step + door.m_t;
        if (door.m_t_actual < 0.97f) {
            break;
        }
        door.m_t = 1.0f;
        door.m_t_actual = 1.0f;
        door.m_t_velocity = 0.0f;
        door.m_state = ADI_OPENED;
        GAME_TRACE("vehicle", "door open: vehicle=%s component=%d", vp->info->name, (int)(this - vp->rdd.get_data()->m_components));
        // fall through
    case ADI_OPENED:
        if (hinge != NULL) {
            ((hkpVelocityConstraintMotor*)hinge->m_atoms.m_angMotor.m_motor)->m_velocityTarget = 0.0f;
        }
        door.m_state = ADI_NOT_ANIMATING;
        return;
    case ADI_BEGIN_CLOSING:
        if (hinge != NULL) {
            hinge->setMotorEnabled(m_constraint->m_internal->m_runtime, true);
            ((hkpVelocityConstraintMotor*)hinge->m_atoms.m_angMotor.m_motor)->m_velocityTarget = (float)((double)((const vehicle_component_data::hinge*)m_data->m_hinge)->m_max_angle * (double)door.m_t_velocity);
            if (vp->m_flags.fixed_motion != 0 && havok_handle != 0xFFFFFFFF) {
                havok_body_force_activate(havok_handle);
            }
        }
        door.m_state = ADI_CLOSING;
        GAME_TRACE("vehicle", "door closing: vehicle=%s component=%d", vp->info->name, (int)(this - vp->rdd.get_data()->m_components));
        door.m_t = door.m_t_actual;
        // fall through
    case ADI_CLOSING: {
        door.m_t = door.m_t_velocity * time_step + door.m_t;
        if (0.03f < door.m_t_actual) {
            break;
        }
        door.m_t = 0.0f;
        door.m_t_actual = 0.0f;
        door.m_state = ADI_CLOSED;
        GAME_TRACE("vehicle", "door closed: vehicle=%s component=%d", vp->info->name, (int)(this - vp->rdd.get_data()->m_components));
        // the close sound of the first interaction transition that uses this door
        const vehicle_interaction_info* vi_info = vp->info->vi_info; // Original bug kept: not checked for null
        vehicle::resource_dependent_data* data = vp->rdd.get_data();
        for (int i = 0; i < vi_info->num_waypoint_transitions; i++) {
            if (&data->m_components[vi_info->transitions[i].component_obstacle_index] == this) {
                audiolib_cue_play_3d(vi_info->transitions[i].component_close_foley, &m_ws_position, NULL, NULL, NULL);
                break;
            }
        }
    }
        // fall through
    case ADI_CLOSED:
        door.m_state = ADI_NOT_ANIMATING;
        if (hinge != NULL) {
            hinge->setMotorEnabled(m_constraint->m_internal->m_runtime, false);
        }
        vehicle_component_latch_hinged(this, vp);
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
    float t = door.m_t;
    if (t - 0.0f < 0.0f) {
        t = 0.0f;
    }
    if (0.0f <= t - 1.0f) {
        t = 1.0f;
    }
    door.m_t = t;
    hkpRigidBody* chassis = havok_body_get_pointer(vp->havok_handle);
    if (chassis != NULL) {
        chassis->activate();
    }
}

// 0x00B8E090 (:1311)
void vehicle_component::process_loose_door(float time_step, vehicle* vp)
{
    door_attr& door = m_type_attribute.door;
    const float last_t = door.m_t_actual;
    const float t = get_unlatched_door_t_value(vp);
    door.m_t_actual = t;
    door.m_t_velocity = 0.0f < time_step ? (t - last_t) / time_step : 0.0f;
}

// 0x00B7AA70 (:1332)
float vehicle_component::get_unlatched_door_t_value(const vehicle* vp) const
{
    if (m_constraint == NULL) {
        return m_type_attribute.door.m_t;
    }
    const matrix& parent_rotation = m_data->m_parent_index == 0xFFFFFFFF ? vp->orient : vp->rdd.get_data()->m_components[m_data->m_parent_index].get_world_space_rotation();
    const vehicle_component_data::hinge* hinge_data = (const vehicle_component_data::hinge*)m_data->m_hinge;
    const vector axis = hinge_data->m_axis;
    // how far it has turned from where the file places it
    matrix turned = m_ws_rotation;
    turned.rotate(parent_rotation);
    turned.rotate(m_data->m_rotation);
    vector turned_axis;
    float angle;
    turned.extract_axis_and_angle(turned_axis, angle);
    if (0.7f < turned_axis.x * axis.x + turned_axis.y * axis.y + turned_axis.z * axis.z) {
        angle = -angle;
    }
    float t = fl_angle_cap_radians(angle, -1.5707964f) / hinge_data->m_max_angle;
    if (t - -1.0f < 0.0f) {
        t = -1.0f;
    }
    if (0.0f <= t - 2.0f) {
        t = 2.0f;
    }
    return t;
}

// 0x00B65E50 (:1379)
unsigned int vehicle_component::get_cm_index() const
{
    return m_data->m_cm_dynamics_info.m_cm_index;
}

// 0x00B7ACB0 (:1388)
const hkpMoppCode* vehicle_component::get_mopp_code() const
{
    return (const hkpMoppCode*)m_data->m_cm_dynamics_info.m_mopp_code;
}

// 0x00B65E60 (:1397)
const vector& vehicle_component::get_center_of_mass() const
{
    return m_data->m_cm_dynamics_info.m_center_of_mass;
}

// 0x00B65E70 (:1406)
const matrix& vehicle_component::get_inertia_tensor() const
{
    return m_data->m_cm_dynamics_info.m_inertia_tensor;
}

// 0x00B65E80 (:1417)
float vehicle_component::calc_material_mass(vehicle* vp) const
{
    const rfg_physical_material* material = rfg_get_physical_material_by_checksum(m_data->m_mass_type);
    if (material != NULL) {
        // a quarter of solid, at most a share of the chassis
        float mass = m_data->m_cm_dynamics_info.m_volume * material->density * 0.25f;
        if (mass - 0.0f < 0.0f) {
            mass = 0.0f;
        }
        const float limit = vp->info->chassis_mass * (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER ? 0.6f : 0.3f);
        if (0.0f <= mass - limit) {
            mass = limit;
        }
        if (mass != 0.0f) {
            return mass;
        }
    }
    return 60.0f;
}

// 0x00B65F90 (:1462)
unsigned int vehicle_component::get_parent_sub_piece_start_index() const
{
    return m_data->m_parent_sub_piece_start_index;
}

// 0x00B65F60 (:1445)
unsigned char vehicle_component::get_physical_material() const
{
    return rfg_get_physical_material_index_by_checksum(m_data->m_mass_type);
}

// 0x00B65F70 (:1451)
unsigned char vehicle_component::get_effect_material() const
{
    return rfg_get_effect_material_index(rfg_get_physical_material_index_by_checksum(m_data->m_mass_type));
}

// 0x00B7ACC0 (:1472)
unsigned int vehicle_component::get_first_shape_key(const vehicle* vp) const
{
    const vehicle::resource_dependent_data* data = vp->rdd.get_data();
    unsigned int key = m_data->m_parent_sub_piece_start_index;
    for (unsigned int parent = m_data->m_parent_index; parent != 0xFFFFFFFF; parent = data->m_components[parent].get_data()->m_parent_index) {
        key += data->m_components[parent].get_data()->m_parent_sub_piece_start_index;
    }
    return key;
}

// 0x00B8E100 (:1467)
unsigned int vehicle_component::get_shape_key(const vehicle* vp) const
{
    return get_first_shape_key(vp); // inlined in rfg.exe
}

// 0x00B7AD10 (:1490)
unsigned int vehicle_component::get_last_shape_key(const vehicle* vp) const
{
    return get_first_shape_key(vp) + m_data->m_parent_num_sub_pieces - 1; // inlined in rfg.exe
}

// 0x00B7AD70 (:1509)
unsigned int vehicle_component::get_node_num_shapes(const vehicle* vp) const
{
    const hkpShape* shape = havok_cm_manager_get(m_data->m_cm_dynamics_info.m_cm_index);
    return shape != NULL ? vehicle_shape_key_count(shape) : 0;
}

// 0x00B7ADC0 (:1534)
unsigned int vehicle_component::get_comp_num_shapes(const vehicle* vp) const
{
    if (m_numb_shapes == 0xFF) {
        const hkpShape* shape = havok_cm_manager_get(m_data->m_cm_dynamics_info.m_cm_index);
        int num_shapes = shape != NULL ? (int)vehicle_shape_key_count(shape) : 0;
        if (!vp->rdd.is_loaded()) {
            return 0xFF;
        }
        // its own shapes end where its first child's begin
        const vehicle::resource_dependent_data* data = vp->rdd.get_data();
        for (unsigned int i = 0; i < m_data->m_num_children; i++) {
            const int start = (int)data->m_components[m_data->m_children_indices[i]].get_data()->m_parent_sub_piece_start_index;
            if (start != 0 && start < num_shapes) {
                num_shapes = start;
            }
        }
        const_cast<vehicle_component*>(this)->m_numb_shapes = (unsigned char)num_shapes;
    }
    return m_numb_shapes;
}

// 0x00B65FA0 (:1568)
unsigned int vehicle_component::get_parent_num_sub_pieces() const
{
    return m_data->m_parent_num_sub_pieces;
}

// One light's two sub instances (off, on) in a component's own instance. Inlined in rfg.exe.
static void vehicle_component_add_light_sub_instance(rl_sub_instance_info& info, const vehicle_mesh_light* mesh_light, int submesh_id, bool active)
{
    info.m_is_active = active;
    info.m_submesh_id = submesh_id;
    info.m_transform.set_translation(mesh_light->position);
    info.m_transform.set_orientation(mesh_light->rotation);
}

// 0x00B7AE80 (:1575)
bool vehicle_component::create_renderable_instance(vehicle* vp)
{
    vp->component_rinstance_cs.enter();
    m_renderable_instance_refs++;
    if (m_renderable_instance_p != NULL) {
        vp->component_rinstance_cs.leave();
        return true;
    }
    vp->set_render_flags(8);
    m_renderable_instance_p = rl_deformable_mesh_instance::create();
    if (m_renderable_instance_p == NULL) {
        vp->component_rinstance_cs.leave();
        return false;
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    rl_mesh* mesh = (rl_mesh*)data->sub_info->m_vehicle_mesh;
    rl_material_map* material_map = (rl_material_map*)data->sub_info->m_material_map;
    if (m_data->m_type == VCT_TREAD_SEGMENT) {
        // 256 pads, all off until the frame's spline update
        static rl_sub_instance_info pads[256]; // on the stack in rfg.exe (0x3800 bytes)
        for (int i = 0; i < 256; i++) {
            pads[i].m_disable_deferred = 0;
            pads[i].m_is_active = 0;
            pads[i].m_submesh_id = m_data->m_submesh_id;
            pads[i].m_transform = IDENTITY_MATRIX43;
        }
        m_renderable_instance_p->init(mesh, material_map, pads, 256);
        m_renderable_instance_p->set_morph_instance(data->m_renderable_instance_p->get_morph_instance());
        m_renderable_instance_p->set_flag(0x100, false);
    }
    else {
        // itself and two sub instances for each of its lights, at most 20
        rl_sub_instance_info infos[41];
        for (int i = 0; i < 41; i++) {
            infos[i].m_disable_deferred = 0;
        }
        infos[0].m_is_active = 1;
        infos[0].m_submesh_id = m_data->m_submesh_id;
        infos[0].m_transform = IDENTITY_MATRIX43;
        int num_infos = 1;
        const int index = (int)(this - data->m_components);
        for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
            vehicle_light& light = data->m_vehicle_lights[i];
            if (light.parent_idx != index) {
                continue;
            }
            const bool off_active = !light.flags.is_on && !light.flags.broken;
            const bool on_active = light.flags.is_on && !light.flags.broken;
            light.flags.dirty = true;
            light.component_mesh_index_off = num_infos;
            vehicle_component_add_light_sub_instance(infos[num_infos++], light.mesh_light, light.mesh_light->off_mesh_id, off_active);
            light.component_mesh_index_on = num_infos;
            vehicle_component_add_light_sub_instance(infos[num_infos++], light.mesh_light, light.mesh_light->on_mesh_id, on_active);
        }
        m_renderable_instance_p->init(mesh, material_map, infos, num_infos);
        m_renderable_instance_p->set_morph_instance(data->m_renderable_instance_p->get_morph_instance());
        m_renderable_instance_p->set_flag(0x100, false);
        m_renderable_instance_p->set_position(m_ws_position);
        m_renderable_instance_p->set_rotation(m_ws_rotation);
    }
    m_renderable_instance_p->calculate_local_aabb();
    if (m_data->m_type == VCT_TREAD_SEGMENT) {
        // the pads go all around the vehicle: its bounds, not the one pad's
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        rl_bvolume& bounds = m_renderable_instance_p->m_local_bvolume;
        bounds.m_half_vec.set((bmax.x - bmin.x) * 0.5f, (bmax.y - bmin.y) * 0.5f, (bmax.z - bmin.z) * 0.5f);
        bounds.m_sphere.m_position.set(bounds.m_half_vec.x + bmin.x, bounds.m_half_vec.y + bmin.y, bounds.m_half_vec.z + bmin.z);
        bounds.m_sphere.m_radius = sqrtf(bounds.m_half_vec.y * bounds.m_half_vec.y + bounds.m_half_vec.x * bounds.m_half_vec.x + bounds.m_half_vec.z * bounds.m_half_vec.z);
        m_renderable_instance_p->set_bounding_volume_dirty(true);
        m_renderable_instance_p->update_partition_system();
    }
    vp->component_rinstance_cs.leave();
    return true;
}

// 0x00B8E110 (:1689)
void vehicle_component::destroy_renderable_instance(vehicle* vp)
{
    vp->component_rinstance_cs.enter();
    m_renderable_instance_refs--;
    if (m_renderable_instance_refs < 1) {
        destroy_renderable_instance_absolute(vp);
    }
    vp->component_rinstance_cs.leave();
}

// 0x00B7B4A0 (:1704)
bool vehicle_component::create_renderable_instance_subtree(vehicle* vp)
{
    bool ok = true;
    if ((m_flags & (VCF_DETACHED | VCF_NOT_RENDERED)) == 0) {
        ok = create_renderable_instance(vp);
    }
    if (!vp->rdd.is_loaded()) {
        return false;
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int i = 0; i < m_data->m_num_children; i++) {
        const bool child_ok = data->m_components[m_data->m_children_indices[i]].create_renderable_instance_subtree(vp);
        ok = ok && child_ok;
    }
    return ok;
}

// 0x00B8E150 (:1729)
void vehicle_component::destroy_renderable_instance_subtree(vehicle* vp)
{
    destroy_renderable_instance(vp); // inlined in rfg.exe
    if (vp->rdd.is_loaded()) {
        vehicle::resource_dependent_data* data = vp->rdd.get_data();
        for (unsigned int i = 0; i < m_data->m_num_children; i++) {
            vehicle_component& child = data->m_components[m_data->m_children_indices[i]];
            if ((child.m_flags & (VCF_DETACHED | VCF_NOT_RENDERED)) == 0) {
                child.destroy_renderable_instance_subtree(vp);
            }
        }
    }
}

// 0x00B7B520 (:1749)
void vehicle_component::destroy_renderable_instance_absolute(vehicle* vp)
{
    vp->component_rinstance_cs.enter();
    rl_deformable_mesh_instance* instance = m_renderable_instance_p;
    m_renderable_instance_refs = 0;
    if (instance != NULL) {
        instance->release();
        m_renderable_instance_p = NULL;
        vehicle::resource_dependent_data* data = vp->rdd.get_data();
        const int index = (int)(this - data->m_components);
        for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
            vehicle_light& light = data->m_vehicle_lights[i];
            if (light.parent_idx == index) {
                light.flags.is_on = false;
                light.flags.dirty = true;
                light.component_mesh_index_off = -1;
                light.component_mesh_index_on = -1;
            }
        }
        vp->set_render_flags(8);
    }
    vp->component_rinstance_cs.leave();
}

// 0x00B65FB0 (:1782)
void vehicle_component::set_orient_pos_in_world_space(vehicle* vp, const matrix& r, const vector& p)
{
    m_ws_rotation = r;
    m_ws_position = p;
    m_cs_rotation = r;
    m_cs_rotation.rotate(vp->orient);
    m_cs_position.set(p.x - vp->pos.x, p.y - vp->pos.y, p.z - vp->pos.z);
    m_cs_position.rotate(vp->orient);
}

// 0x00B660C0 (:1803)
void vehicle_component::set_orient_pos_in_chassis_space(const matrix& r, const vector& p)
{
    m_cs_rotation = r;
    m_cs_position = p;
}

// 0x00B66120 (:1815)
void vehicle_components_system_init()
{
    Vehicle_shatter_glass_effect = effects_get_handle("veh_shatter_glass_a");
}

// 0x00B66140 (:1820)
void vehicle_components_system_shutdown()
{
}

// 0x00B66150 (:1826)
void vehicle_components_level_init_pre()
{
    Num_damaged_components_this_frame = 0;
}

// 0x00B66160 (:1835)
void vehicle_components_level_shutdown()
{
}

// 0x00B66170 (:1845)
bool vehicle_component_is_wheel(const vehicle_component_data* component)
{
    const unsigned int type = component->m_type;
    return type == VCT_FRONT_WHEEL || type == VCT_REAR_WHEEL || type == VCT_AXLE_3_WHEEL || type == VCT_AXLE_4_WHEEL;
}

// 0x00B7B5D0 (:1865)
unsigned int vehicle_component_get_wheel_index(const vehicle* vp, const vehicle_component* p_component)
{
    const automobile* ap = (const automobile*)vp;
    if (ap->a_rdd.is_loaded()) {
        for (unsigned int i = 0; i < 16; i++) {
            if (p_component == ap->a_rdd.get_data()->m_wheel_components[i]) {
                return i;
            }
        }
    }
    return 0xFFFFFFFF;
}

// 0x00B7B610 (:1890)
void vehicle_component_add_user_properties(vehicle_component* component, const vehicle* vp)
{
    if (!vp->rdd.is_loaded()) {
        return;
    }
    hkpWorldObject* body = havok_body_get_pointer(component->havok_handle);
    if (body != NULL) {
        const unsigned char material = rfg_get_physical_material_index_by_checksum(component->get_data()->m_mass_type);
        havok_set_property(body, 0, const_cast<vehicle*>(vp)); // the owner
        havok_set_property(body, 5, (void*)(int)(component - vp->rdd.get_data()->m_components)); // the component's index
        havok_set_property(body, 3, (void*)(unsigned int)material);
        havok_set_property(body, 4, (void*)(unsigned int)rfg_get_effect_material_index(material));
    }
}

// 0x00B97C90 (:1919)
void vehicle_component_get_hierarchy_collision(hkArray<unsigned int,hkContainerHeapAllocator>& out_collision_filters, const vehicle_component* p_component, const vehicle* vp)
{
    out_collision_filters.setSizeUnchecked(0);
    const vehicle::resource_dependent_data* data = vp->rdd.get_data();
    const unsigned int num_pieces = p_component->get_data()->m_parent_num_sub_pieces;
    unsigned int offset = 0;
    hkpRigidBody* body;
    // up to the nearest body, whose shape holds this part's pieces
    for (;;) {
        const unsigned int parent = p_component->get_data()->m_parent_index;
        body = havok_body_get_pointer(parent == 0xFFFFFFFF ? vp->havok_handle : data->m_components[parent].havok_handle);
        if (body != NULL) {
            break;
        }
        offset += p_component->get_data()->m_parent_sub_piece_start_index;
        p_component = &data->m_components[parent];
    }
    if ((p_component->get_flags() & VCF_SHAPE_IN_PARENT) != 0) {
        havok_filter_shape_collection* collection = havok_filter_shape_collection_get_from_rigid_body(body); // inlined in rfg.exe
        if (collection == NULL) {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            return;
        }
        collection->get_collision_filter(out_collision_filters, offset + p_component->get_data()->m_parent_sub_piece_start_index, num_pieces);
        return;
    }
    // a separate part: every shape of its model takes the parent's filter
    const unsigned int filter = body->getCollidable()->getCollisionFilterInfo();
    const hkpShape* shape = havok_cm_manager_get(p_component->get_data()->m_cm_dynamics_info.m_cm_index);
    const unsigned int num_shapes = vehicle_shape_key_count(shape);
    for (unsigned int i = 0; i < num_shapes; i++) {
        out_collision_filters.pushBack(filter);
    }
}

// 0x00B9E410 (:2046)
void vehicle_component_set_hierarchy_collision(vehicle_component* component, vehicle* vp, unsigned int filter_info)
{
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    const int num_pieces = (int)component->get_data()->m_parent_num_sub_pieces;
    hkArray<unsigned int,hkContainerHeapAllocator> own_filters;
    // turning back on: only what the part's own body has on
    bool use_own_filters = filter_info != COLLIDE_NO_COLLISION;
    if (use_own_filters) {
        hkpRigidBody* own_body = component->havok_handle != 0xFFFFFFFF ? havok_body_get_pointer(component->havok_handle) : NULL;
        havok_filter_shape_collection* own_collection = own_body != NULL ? havok_filter_shape_collection_get_from_rigid_body(own_body) : NULL;
        if (own_collection != NULL) {
            own_collection->get_collision_filter(own_filters, 0, num_pieces);
        }
        else {
            use_own_filters = false;
        }
    }
    int offset = 0;
    while ((component->get_flags() & VCF_SHAPE_IN_PARENT) != 0 && (component->get_flags() & (VCF_DETACHED | VCF_NOT_RENDERED)) == 0) {
        const unsigned int parent = component->get_data()->m_parent_index;
        hkpRigidBody* body = havok_body_get_pointer(parent == 0xFFFFFFFF ? vp->havok_handle : data->m_components[parent].havok_handle);
        if (body == NULL) {
            offset += component->get_data()->m_parent_sub_piece_start_index;
            component = &data->m_components[parent];
            continue;
        }
        havok_filter_shape_collection* collection = havok_filter_shape_collection_get_from_rigid_body(body);
        if (collection == NULL) {
            return;
        }
        const int start = offset + component->get_data()->m_parent_sub_piece_start_index;
        hkArray<unsigned int,hkContainerHeapAllocator> filters;
        collection->get_collision_filter(filters, start, num_pieces);
        bool unchanged = true;
        for (int i = 0; i < num_pieces; i++) {
            unchanged = unchanged && filters[i] == (use_own_filters ? own_filters[i] : filter_info);
        }
        if (!unchanged) {
            if (use_own_filters) {
                for (int i = 0; i < num_pieces; i++) {
                    collection->setCollisionFilterInfo(start + i, own_filters[i] != COLLIDE_NO_COLLISION ? filter_info : COLLIDE_NO_COLLISION);
                }
            }
            else {
                collection->set_collision_filter(start, num_pieces, filter_info);
            }
            havok_entity_update_collision_filter(body, true, true);
        }
        return;
    }
}

// 0x00B9E680 (:2166)
void vehicle_component_create_rigid_body(vehicle_component* component, vehicle* vp, bool collide_with_vehicle)
{
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    if (component->get_data()->m_cm_dynamics_info.m_cm_index == 0xFFFFFFFF) {
        return;
    }
    hkpRigidBodyCinfo cinfo;
    game_to_havok_vector(cinfo.m_position, component->get_world_space_position());
    game_to_havok_matrix(cinfo.m_rotation, component->get_world_space_rotation());
    if (collide_with_vehicle) {
        // collision types: a flyer's part, a walker's part, an automobile's part
        cinfo.m_collisionFilterInfo = vp->get_sub_type() == OT_SUB_VEHICLE_FLYER ? 0xF : (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER ? 0xC : 9);
    }
    else {
        cinfo.m_collisionFilterInfo = vehicle_get_collision_filter(vp); // inlined for automobiles and flyers in rfg.exe
    }
    const hkpShape* base_shape = havok_cm_manager_get(component->get_data()->m_cm_dynamics_info.m_cm_index);
    hkArray<unsigned int,hkContainerHeapAllocator> filters;
    vehicle_component_get_hierarchy_collision(filters, component, vp);
    // the shape's filter: on (0 = the body's own) unless the hierarchy has that piece off
    for (int i = 0; i < filters.getSize(); i++) {
        if (filters[i] != COLLIDE_NO_COLLISION) {
            filters[i] = 0;
        }
    }
    cinfo.m_shape = havok_filter_shape_collection_create(base_shape, filters);
    const hkpMoppCode* code = (const hkpMoppCode*)component->get_data()->m_cm_dynamics_info.m_mopp_code;
    if (code != NULL) {
        hkpMoppBvTreeShape* tree = new hkpMoppBvTreeShape((hkpShapeCollection*)cinfo.m_shape, code);
        const_cast<hkpShape*>(cinfo.m_shape)->removeReference();
        cinfo.m_shape = tree;
    }
    cinfo.m_mass = component->calc_material_mass(vp); // inlined in rfg.exe
    matrix tensor = component->get_data()->m_cm_dynamics_info.m_inertia_tensor;
    tensor.rvec.x *= cinfo.m_mass;
    tensor.rvec.y *= cinfo.m_mass;
    tensor.rvec.z *= cinfo.m_mass;
    tensor.uvec.x *= cinfo.m_mass;
    tensor.uvec.y *= cinfo.m_mass;
    tensor.uvec.z *= cinfo.m_mass;
    tensor.fvec.x *= cinfo.m_mass;
    tensor.fvec.y *= cinfo.m_mass;
    tensor.fvec.z *= cinfo.m_mass;
    havok_adjust_tensor(tensor, 0.5f, 0.1f);
    game_to_havok_tensor(cinfo.m_inertiaTensor, tensor);
    game_to_havok_vector(cinfo.m_centerOfMass, component->get_data()->m_cm_dynamics_info.m_center_of_mass);
    cinfo.m_linearDamping = 0.01f;
    cinfo.m_angularDamping = 0.05f;
    cinfo.m_friction = 0.5f;
    cinfo.m_restitution = 0.05f;
    cinfo.m_motionType = (hkpMotion::MotionType)3; // MOTION_BOX_INERTIA
    cinfo.m_enableDeactivation = true;
    cinfo.m_qualityType = (hkpCollidableQualityType)2; // HK_COLLIDABLE_QUALITY_MOVING
    hkpRigidBody* body = NULL;
    const unsigned int handle = havok_rigid_body_create(cinfo, body);
    component->havok_handle = handle;
    if (handle == 0xFFFFFFFF) {
        return;
    }
    const_cast<hkpShape*>(cinfo.m_shape)->removeReference();
    component->m_original_collision_filter = cinfo.m_collisionFilterInfo;
    vehicle_component_add_user_properties(component, vp);
    havok_add_entity(body, false, true);
    // objects attached to its piece (or below it) now hang on the new body
    const int index = (int)(component - data->m_components);
    constraint* cp = vp->host_constraint_ptr;
    while (cp != NULL) {
        for (unsigned int c = vehicle_get_component_index_from_shape_key(vp, cp->host_attach_index, cp->host_alt_hk_body_index); c != 0xFFFFFFFF; c = data->m_components[c].get_data()->m_parent_index) {
            if (c == (unsigned int)index) {
                object_constraint_transfer(true, cp, World.object_from_handle(cp->child_handle), vp, vp, body);
                break;
            }
        }
        cp = cp->host_constraint_next;
        if (cp == vp->host_constraint_ptr) {
            break;
        }
    }
}

// The axis perpendicular to the hinge: its cross with whichever world axis it is least parallel to. Inlined in rfg.exe.
static void vehicle_hinge_perp_axis(const vector& axis, const vector& world_axis, float& best, vector& perp)
{
    const float parallel = fabsf(axis.y * world_axis.y + axis.x * world_axis.x + axis.z * world_axis.z);
    if (parallel < best) {
        perp.set(world_axis.z * axis.y - axis.z * world_axis.y, axis.z * world_axis.x - axis.x * world_axis.z, axis.x * world_axis.y - axis.y * world_axis.x);
        best = parallel;
    }
}

// 0x00B8E1D0 (:2354)
void vehicle_component_create_hinge(vehicle_component* component, const vehicle* vp)
{
    if (!vp->rdd.is_loaded()) {
        return;
    }
    const vehicle::resource_dependent_data* data = vp->rdd.get_data();
    const vehicle_component_data* comp_data = component->get_data();
    const vehicle_component_data::hinge* hinge_data = (const vehicle_component_data::hinge*)comp_data->m_hinge;
    vector pivot = hinge_data->m_origin;
    vector axis = hinge_data->m_axis;
    vector perp;
    // Original quirk kept: the first test is against 0.99 (an axis nearly parallel to all three leaves perp unset; a unit axis cannot be).
    float best = 0.99f;
    vehicle_hinge_perp_axis(hinge_data->m_axis, UP_VECTOR, best, perp);
    vehicle_hinge_perp_axis(hinge_data->m_axis, FORWARD_VECTOR, best, perp);
    vehicle_hinge_perp_axis(hinge_data->m_axis, RIGHT_VECTOR, best, perp);
    perp.normalize_safe();
    if ((component->get_flags() & VCF_HAS_POSITION) != 0 && (component->get_flags() & VCF_DEFORM_POSITION) != 0 && component->m_pos_orient_deform_weight != 0) {
        const float weight = mesh_deform_weight_convert(component->m_pos_orient_deform_weight);
        pivot.x += comp_data->m_deformed_position_offset.x * weight;
        pivot.y += comp_data->m_deformed_position_offset.y * weight;
        pivot.z += comp_data->m_deformed_position_offset.z * weight;
    }
    // in the part's own space
    vector child_pivot(pivot.x - comp_data->m_position.x, pivot.y - comp_data->m_position.y, pivot.z - comp_data->m_position.z);
    vector child_axis = axis;
    vector child_perp = perp;
    child_pivot.rotate(comp_data->m_rotation);
    child_axis.rotate(comp_data->m_rotation);
    child_perp.rotate(comp_data->m_rotation);
    // and in the space of the nearest parent with a body
    unsigned int parent = comp_data->m_parent_index;
    while (parent != 0xFFFFFFFF && data->m_components[parent].havok_handle == 0xFFFFFFFF) {
        const vehicle_component_data* parent_data = data->m_components[parent].get_data();
        pivot.unrotate(parent_data->m_rotation);
        pivot.x += parent_data->m_position.x;
        pivot.y += parent_data->m_position.y;
        pivot.z += parent_data->m_position.z;
        axis.unrotate(parent_data->m_rotation);
        perp.unrotate(parent_data->m_rotation);
        parent = parent_data->m_parent_index;
    }
    hkpRigidBody* body = havok_body_get_pointer(component->havok_handle);
    hkpRigidBody* parent_body = havok_body_get_pointer(parent == 0xFFFFFFFF ? vp->havok_handle : data->m_components[parent].havok_handle);
    hkVector4f hk_pivot_parent;
    hkVector4f hk_axis_parent;
    hkVector4f hk_perp_parent;
    hkVector4f hk_pivot_child;
    hkVector4f hk_axis_child;
    hkVector4f hk_perp_child;
    game_to_havok_vector(hk_pivot_parent, pivot);
    game_to_havok_vector(hk_axis_parent, axis);
    game_to_havok_vector(hk_perp_parent, perp);
    game_to_havok_vector(hk_pivot_child, child_pivot);
    game_to_havok_vector(hk_axis_child, child_axis);
    game_to_havok_vector(hk_perp_child, child_perp);
    // 2.5 % of the range past both ends
    const float limit_margin = hinge_data->m_max_angle * 0.025f;
    hkpLimitedHingeConstraintData* hinge = new hkpLimitedHingeConstraintData();
    hinge->setInBodySpace(hk_pivot_parent, hk_pivot_child, hk_axis_parent, hk_axis_child, hk_perp_parent, hk_perp_child);
    hinge->setMinAngularLimit((float)(0.0 - (double)limit_margin));
    hinge->setMaxAngularLimit((float)((double)limit_margin + (double)hinge_data->m_max_angle));
    hinge->setMaxFrictionTorque(body->getMass());
    hinge->setMotor(NULL);
    hkpVelocityConstraintMotor* motor = new hkpVelocityConstraintMotor();
    motor->m_type = hkpConstraintMotor::TYPE_VELOCITY; // the inlined constructor in rfg.exe; else hkCalcMotorData leaves its output unset
    motor->m_velocityTarget = 0.0f;
    motor->m_useVelocityTargetFromConstraintTargets = false;
    motor->m_tau = Vehicle_component_hinge_tau;
    motor->m_minForce = -10000.0f;
    motor->m_maxForce = 10000.0f;
    hinge->setMotor(motor);
    motor->removeReference();
    component->m_constraint = havok_constraint_create(parent_body, body, hinge, -1.0f, hkpConstraintInstance::PRIORITY_PSI);
    if ((component->get_flags() & VCF_DEFORMED_HINGE) != 0) {
        vehicle_deform_unlatched_hinge(component, vp);
    }
    hinge->setMotorEnabled(component->m_constraint->m_internal->m_runtime, false);
}

// 0x00B9EBB0 (:2506)
void vehicle_component_unlatch_hinged(vehicle_component* component, vehicle* vp)
{
    const unsigned int flags = component->get_flags();
    if ((flags & VCF_HINGED) == 0 || (flags & (VCF_UNLATCHED | VCF_DETACHED)) != 0) {
        return;
    }
    component->set_flags(VCF_UNLATCHED);
    if (0.01f < component->get_data()->m_cm_dynamics_info.m_volume) {
        vehicle_component_create_rigid_body(component, vp, false);
        if (component->havok_handle != 0xFFFFFFFF) {
            vehicle_component_create_hinge(component, vp);
            if ((flags & VCF_SHAPE_IN_PARENT) != 0) {
                vehicle_component_set_hierarchy_collision(component, vp, COLLIDE_NO_COLLISION);
            }
            if (component->get_data()->m_type == VCT_DOOR && vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
                hkpRigidBody* body = havok_body_get_pointer(component->havok_handle);
                if (body != NULL) {
                    // a slow car's opening door does not hit people: layer 0x35
                    const unsigned int filter = automobile_get_mph((automobile*)vp) < Vehicle_door_collision_threshold_mph ? (component->m_original_collision_filter & 0xFFFF0035) | 0x35 : component->m_original_collision_filter;
                    havok_body_set_collision_type(body, filter, false, false);
                }
            }
        }
    }
    component->create_renderable_instance_subtree(vp);
}

// 0x00B7B6B0 (LTCG: vp in EDI)
static unsigned int vehicle_component_find_parent_collision_filter(const vehicle_component* component, const vehicle* vp)
{
    const vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int parent = component->get_data()->m_parent_index; parent != 0xFFFFFFFF; parent = data->m_components[parent].get_data()->m_parent_index) {
        if (data->m_components[parent].havok_handle != 0xFFFFFFFF) {
            return havok_body_get_collision_type(data->m_components[parent].havok_handle);
        }
    }
    const unsigned int chassis_filter = vehicle_get_collision_filter(vp); // inlined in rfg.exe
    if (data->m_chassis_collision_component != 0xFFFFFFFF && vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        return (chassis_filter & 0xFFFF0000) | 0xB; // the chassis collision group
    }
    return chassis_filter;
}

// 0x00B9EC90 (:2600)
void vehicle_component_latch_hinged(vehicle_component* component, vehicle* vp)
{
    const unsigned int flags = component->get_flags();
    if ((flags & VCF_HINGED) == 0 || (flags & VCF_UNLATCHED) == 0 || (flags & (VCF_HINGE_BROKEN | VCF_DETACHED)) != 0) {
        return;
    }
    if ((flags & VCF_SHAPE_IN_PARENT) != 0) {
        vehicle_component_set_hierarchy_collision(component, vp, vehicle_component_find_parent_collision_filter(component, vp));
    }
    vehicle_component::door_attr& door = component->m_type_attribute.door;
    door.m_state = vehicle_component::ADI_NOT_ANIMATING;
    door.m_t = 0.0f;
    door.m_t_actual = 0.0f;
    door.m_t_velocity = 0.0f;
    component->free_rigid_body_and_constraints(vp);
    component->clear_flags(VCF_UNLATCHED);
    if (component->get_data()->m_parent_index == 0xFFFFFFFF) {
        component->destroy_renderable_instance_subtree(vp);
    }
}

// 0x00B7B770 (:2678)
static vehicle_component* vehicle_component_find_child_door_recursive(const vehicle_component* component, const vehicle* vp, int& idx)
{
    const vehicle::resource_dependent_data* data = vp->rdd.get_data();
    const vehicle_component_data* comp_data = component->get_data();
    for (unsigned int i = 0; i < comp_data->m_num_children; i++) {
        vehicle_component* child = const_cast<vehicle_component*>(&data->m_components[comp_data->m_children_indices[i]]);
        if (child->get_data()->m_type == VCT_DOOR) {
            if (idx == 0) {
                return child;
            }
            idx--;
        }
        else {
            vehicle_component* found = vehicle_component_find_child_door_recursive(child, vp, idx);
            if (found != NULL) {
                return found;
            }
        }
    }
    return NULL;
}

// 0x00B7B7F0 (:2699)
vehicle_component* vehicle_component_find_child_door(const vehicle_component* component, const vehicle* vp, int idx)
{
    return vehicle_component_find_child_door_recursive(component, vp, idx);
}

// 0x00B9ED00 (:2711)
void vehicle_component_open_door(vehicle_component* component, vehicle* vp, float open_duration)
{
    if ((component->get_flags() & VCF_DETACHED) != 0) {
        return;
    }
    if ((component->get_flags() & VCF_UNLATCHED) == 0) {
        vehicle_component_unlatch_hinged(component, vp);
    }
    vehicle_component::door_attr& door = component->m_type_attribute.door;
    door.m_t_velocity = 1.0f / open_duration;
    if (door.m_state != vehicle_component::ADI_BEGIN_OPENING && door.m_state != vehicle_component::ADI_OPENING && door.m_state != vehicle_component::ADI_OPENED) {
        door.m_state = vehicle_component::ADI_BEGIN_OPENING;
    }
}

// 0x00B9ED60 (:2747)
void vehicle_component_close_door(vehicle_component* component, vehicle* vp, float close_duration)
{
    if ((component->get_flags() & VCF_DETACHED) != 0) {
        return;
    }
    if ((component->get_flags() & VCF_UNLATCHED) == 0) {
        vehicle_component_unlatch_hinged(component, vp);
    }
    vehicle_component::door_attr& door = component->m_type_attribute.door;
    door.m_t_velocity = -(1.0f / close_duration);
    if (door.m_state != vehicle_component::ADI_BEGIN_CLOSING && door.m_state != vehicle_component::ADI_CLOSING && door.m_state != vehicle_component::ADI_CLOSED) {
        door.m_state = vehicle_component::ADI_BEGIN_CLOSING;
    }
}

// 0x00B661A0 (:2783)
bool vehicle_component_door_is_fully_open(const vehicle_component* component)
{
    return 0.97f < component->m_type_attribute.door.m_t_actual;
}

// 0x00B661C0 (:2797)
bool vehicle_component_door_is_fully_closed(const vehicle_component* component)
{
    return component->m_type_attribute.door.m_t_actual < 0.03f;
}

// 0x00B661F0 (:2814)
float vehicle_component_get_door_open_pct(const vehicle_component* component, const vehicle* vp)
{
    return (component->get_flags() & VCF_DETACHED) != 0 ? 1.0f : component->m_type_attribute.door.m_t_actual;
}

// A detached part's push: its share of the vehicle's velocity (three quarters), then a kick away from the vehicle at a
// random point of its box. Inlined twice in vehicle::component_detach.
static vector vehicle_detach_kick_direction(const vehicle* vp, object* debris, vector& kick_point)
{
    vector velocity;
    vector vehicle_velocity;
    havok_body_get_linear_velocity(debris->havok_handle, velocity);
    havok_body_get_linear_velocity(vp->havok_handle, vehicle_velocity);
    velocity.x = vehicle_velocity.x * 0.75f + velocity.x;
    velocity.y = velocity.y + vehicle_velocity.y * 0.75f;
    velocity.z = velocity.z + vehicle_velocity.z * 0.75f;
    havok_body_set_linear_velocity(debris->havok_handle, velocity);
    vector bmin;
    vector bmax;
    debris->get_bbox(bmin, bmax);
    kick_point.x = fl_rand_range(bmin.x, bmax.x);
    kick_point.y = fl_rand_range(bmin.y, bmax.y);
    kick_point.z = fl_rand_range(bmin.z, bmax.z);
    vector direction(debris->pos.x - vp->pos.x, debris->pos.y - vp->pos.y, debris->pos.z - vp->pos.z);
    direction.normalize_safe();
    direction.y += 0.5f;
    return direction;
}

// The debris pieces of a detached tread: at 85 % of the spline points a strip of 2 to 6 pads. Inlined in rfg.exe.
static void vehicle_detach_tread(vehicle* vp, vehicle_component* component, damaged_by_types from_type, bool from_nano)
{
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    const vehicle_subinfo* sub_info = data->sub_info;
    const vehicle_component_data* comp_data = component->get_data();
    debris_create_info info;
    info.m_mass = 60.0f;
    info.m_friction = 0.5f;
    info.m_restitution = 0.05f;
    info.m_physical_material = rfg_get_physical_material_index_by_checksum(comp_data->m_mass_type);
    info.m_effect_material = rfg_get_effect_material_index(rfg_get_physical_material_index_by_checksum(comp_data->m_mass_type));
    info.m_mesh_srid = vp->get_srid();
    info.m_submesh_id = comp_data->m_submesh_id;
    info.m_vehicle_handle = vp->get_handle();
    info.m_num_lod_infos = vp->info->m_num_lods;
    info.m_lod_info = vp->info->m_lod_info;
    info.m_is_retrievable = false;
    info.m_component_index = 0xFFFFFFFF;
    info.m_collision_type = COLLIDE_RFG_ULTRA_LITE;
    info.m_has_precalc_inertia = true;
    quaternion identity;
    identity.make_identity();
    vector bmin;
    vector bmax;
    ((rl_mesh*)sub_info->m_vehicle_mesh)->get_subpiece_local_bb(comp_data->m_submesh_id, bmin, bmax);
    const vector size(bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z);
    const vector center(size.x * 0.5f + bmin.x, size.y * 0.5f + bmin.y, size.z * 0.5f + bmin.z);
    // six pads in a row, one tread width apart
    rl_sub_instance_info pads[6];
    for (int i = 0; i < 6; i++) {
        pads[i].m_disable_deferred = 0;
        pads[i].m_is_active = 1;
        pads[i].m_submesh_id = comp_data->m_submesh_id;
        pads[i].m_transform = IDENTITY_MATRIX43;
        pads[i].m_transform.m_translation.z = sub_info->m_tread_width * (float)i + IDENTITY_MATRIX43.m_translation.z;
    }
    // every control point of every spline, at most as many as rdd.spline_debris has room for
    vector points[80];
    int point_spline[80];
    int num_points = 0;
    for (unsigned int s = 0; s < sub_info->m_num_splines; s++) {
        const vehicle_subinfo::vehicle_spline_data& spline = sub_info->m_spline_data[s];
        for (unsigned int i = 0; i < spline.num_control_points; i++) {
            point_spline[num_points + i] = (int)s;
            points[num_points + i] = spline.control_points[i];
        }
        num_points += (int)spline.num_control_points;
    }
    for (int p = 0; p < num_points; p++) {
        if (0.15f < fl_rand()) {
            const int num_pads = (int)(vrand() % 5 + 2);
            for (int i = 0; i < num_pads; i++) {
                matrix& rotation = pads[i].m_transform.m_rotation;
                rotation = IDENTITY_MATRIX;
                rotation.rotate_around_local_x((fl_rand() - 0.5f) * 0.7f);
                rotation.rotate_around_local_y((fl_rand() - 0.5f) * 0.14f);
                rotation.rotate_around_local_z((fl_rand() - 0.5f) * 0.28f);
            }
            // Original quirk kept: the chassis space point is added to the position without the vehicle's rotation.
            const vector& point = points[p];
            const vector world_point(vp->pos.x + point.x, point.y + vp->pos.y, vp->pos.z + point.z);
            info.m_position.set(vp->orient.uvec.x * 0.5f + world_point.x, world_point.y + vp->orient.uvec.y * 0.5f, world_point.z + vp->orient.uvec.z * 0.5f);
            info.m_orientation = vp->orient;
            // tilted to where it lay around the spline's center
            const vector& spline_center = sub_info->m_spline_data[point_spline[p]].center;
            vector from_center(point.x - spline_center.x, point.y - spline_center.y, point.z - spline_center.z);
            from_center.normalize();
            float cos_angle = from_center.x * vp->orient.uvec.x + vp->orient.uvec.y * from_center.y + vp->orient.uvec.z * from_center.z;
            if (cos_angle - -1.0f < 0.0f) {
                cos_angle = -1.0f;
            }
            if (0.0f <= cos_angle - 1.0f) {
                cos_angle = 1.0f;
            }
            float angle = fl_acos(cos_angle);
            if (vp->orient.fvec.y * from_center.y + from_center.x * vp->orient.fvec.x + vp->orient.fvec.z * from_center.z < 0.0f) {
                angle = -angle;
            }
            info.m_orientation.rotate_around_local_x(angle);
            rl_deformable_mesh_instance* instance = rl_deformable_mesh_instance::create();
            if (instance == NULL) {
                return;
            }
            info.m_renderable_instance_p = instance;
            instance->init((rl_mesh*)sub_info->m_vehicle_mesh, (rl_material_map*)sub_info->m_material_map, pads, num_pads);
            instance->calculate_local_aabb();
            if (from_nano) {
                material_effect_parameters params = material_effects_get_parameters(1, 2000);
                material_effects_apply(instance, material_effects_get_reconstructor_shards_id(), &params);
            }
            instance->set_flag(0x100, false);
            const float length = sub_info->m_tread_width * (float)(num_pads - 1);
            const vector half_extents(size.x * 0.5f, size.y * 0.5f, (length + size.z) * 0.5f);
            const vector box_center(center.x, center.y, length * 0.5f + center.z);
            info.m_shape = havok_cm_manager_create_box_shape(&identity, &box_center, &half_extents);
            info.m_precalc_center_of_mass = box_center;
            info.m_precalc_tensor = comp_data->m_cm_dynamics_info.m_inertia_tensor;
            info.m_precalc_tensor.rvec.x *= info.m_mass;
            info.m_precalc_tensor.rvec.y *= info.m_mass;
            info.m_precalc_tensor.rvec.z *= info.m_mass;
            info.m_precalc_tensor.uvec.x *= info.m_mass;
            info.m_precalc_tensor.uvec.y *= info.m_mass;
            info.m_precalc_tensor.uvec.z *= info.m_mass;
            info.m_precalc_tensor.fvec.x *= info.m_mass;
            info.m_precalc_tensor.fvec.y *= info.m_mass;
            info.m_precalc_tensor.fvec.z *= info.m_mass;
            havok_adjust_tensor(info.m_precalc_tensor, 0.5f, 0.1f);
            obj_debris_props props(&info);
            object_debris* debris = World.create_object<object_debris>(props, NULL);
            if (debris != NULL) {
                data->spline_debris[p] = debris->get_handle();
                if (from_nano) {
                    object_debris_mark_for_fade(debris, 0, 2000, true);
                }
                else {
                    object_debris_mark_for_fade(debris, int_rand_range(30000, 210000), 1000, false); // 30 s to 3.5 min
                }
            }
            info.m_renderable_instance_p = NULL;
            info.m_shape = NULL;
            // Original bug kept: a failed create_object is dereferenced here.
            if (from_type != DBT_MELEE && debris->havok_handle != 0xFFFFFFFF) {
                vector kick_point;
                const vector direction = vehicle_detach_kick_direction(vp, debris, kick_point);
                havok_body_apply_point_velocity(debris->havok_handle, direction, kick_point);
            }
        }
    }
}

// 0x00B9EDD0 (:2837)
void vehicle::component_detach(vehicle_component* component, bool use_impact_sound, unsigned int delay_before_first_impact_sound, damaged_by_types from_type, bool explode, bool from_nano)
{
    resource_dependent_data* data = rdd.get_data();
    const vehicle_component_data* comp_data = component->get_data();
    const int index = (int)(component - data->m_components);
    for (unsigned int i = 0; i < comp_data->m_num_children; i++) {
        vehicle_component& child = data->m_components[comp_data->m_children_indices[i]];
        if ((child.get_flags() & (VCF_DETACHED | VCF_GLASS)) == 0) {
            component_detach(&child, use_impact_sound, delay_before_first_impact_sound, from_type, explode, from_nano);
        }
    }
    if (comp_data->m_type == VCT_DOOR) {
        component->m_type_attribute.door.m_state = vehicle_component::ADI_OPENED;
    }
    if ((component->get_flags() & VCF_NO_DEBRIS) != 0) {
        const unsigned int flags = component->get_flags();
        component->set_flags(VCF_NO_CHASSIS_MESH);
        if ((flags & VCF_SHAPE_IN_PARENT) != 0) {
            vehicle_component_set_hierarchy_collision(component, this, COLLIDE_NO_COLLISION);
        }
        component->destroy_renderable_instance_absolute(this);
        component->set_flags(VCF_DETACHED);
        component->free_rigid_body_and_constraints(this);
        return;
    }
    if (comp_data->m_type == VCT_TREAD_SEGMENT) {
        component->destroy_renderable_instance(this); // inlined in rfg.exe
        component->set_flags(VCF_DETACHED);
        component->free_rigid_body_and_constraints(this);
        vehicle_detach_tread(this, component, from_type, from_nano);
        return;
    }
    component->destroy_renderable_instance_absolute(this);
    for (unsigned int i = 0; i < data->m_num_vehicle_lights; i++) {
        vehicle_light& light = data->m_vehicle_lights[i];
        if (light.parent_idx == index) {
            light.flags.broken = true;
            light.flags.dirty = true;
            light.stop_all_effects();
            light.component_mesh_index_off = -1;
            light.component_mesh_index_on = -1;
        }
    }
    component->create_renderable_instance(this);

    debris_create_info debris_info;
    debris_info.m_position = component->get_world_space_position();
    debris_info.m_orientation = component->get_world_space_rotation();
    debris_info.m_mass = component->calc_material_mass(this); // inlined in rfg.exe
    debris_info.m_friction = 0.5f;
    debris_info.m_restitution = 0.05f;
    debris_info.m_linear_damp = 0.1f;
    debris_info.m_angular_damp = 0.25f;
    debris_info.m_physical_material = rfg_get_physical_material_index_by_checksum(comp_data->m_mass_type);
    debris_info.m_effect_material = rfg_get_effect_material_index(rfg_get_physical_material_index_by_checksum(comp_data->m_mass_type));
    debris_info.m_mesh_srid = get_srid();
    debris_info.m_renderable_instance_p = component->m_renderable_instance_p;
    debris_info.m_submesh_id = comp_data->m_submesh_id;
    debris_info.m_num_lod_infos = this->info->m_num_lods;
    debris_info.m_lod_info = this->info->m_lod_info;
    debris_info.m_vehicle_handle = get_handle();
    debris_info.m_is_retrievable = false;
    debris_info.m_collision_type = COLLIDE_RFG_ULTRA_LITE;
    debris_info.m_component_index = (unsigned int)index;
    if (from_nano) {
        material_effect_parameters params = material_effects_get_parameters(1, 2000);
        material_effects_apply(debris_info.m_renderable_instance_p, material_effects_get_reconstructor_shards_id(), &params);
    }
    const float volume = comp_data->m_cm_dynamics_info.m_volume;
    if (2.5f < volume) {
        debris_info.m_collision_type = COLLIDE_RFG_LITE;
    }
    if (comp_data->m_num_children == 0) {
        debris_info.m_cm_index = comp_data->m_cm_dynamics_info.m_cm_index;
        debris_info.m_shape = NULL;
    }
    else {
        // its children go with it: a shape with their pieces on, whichever are still attached
        debris_info.m_cm_index = 0;
        if (component->havok_handle == 0xFFFFFFFF) {
            const hkpShape* base_shape = havok_cm_manager_get(comp_data->m_cm_dynamics_info.m_cm_index);
            hkArray<unsigned int,hkContainerHeapAllocator> filters;
            vehicle_component_get_hierarchy_collision(filters, component, this);
            for (int i = 0; i < filters.getSize(); i++) {
                if (filters[i] != COLLIDE_NO_COLLISION) {
                    filters[i] = debris_info.m_collision_type;
                }
            }
            debris_info.m_shape = havok_filter_shape_collection_create(base_shape, filters);
            const hkpMoppCode* code = (const hkpMoppCode*)comp_data->m_cm_dynamics_info.m_mopp_code;
            if (code != NULL) {
                hkpMoppBvTreeShape* tree = new hkpMoppBvTreeShape((hkpShapeCollection*)debris_info.m_shape, code);
                debris_info.m_shape->removeReference();
                debris_info.m_shape = tree;
            }
        }
        else {
            hkpRigidBody* body = havok_body_get_pointer(component->havok_handle);
            debris_info.m_shape = const_cast<hkpShape*>(body->getCollidable()->getShape());
            debris_info.m_shape->addReference();
            havok_filter_shape_collection* collection = havok_filter_shape_collection_get_from_rigid_body(body);
            if (collection != NULL) {
                for (unsigned int key = collection->getFirstKey(); key != 0xFFFFFFFF; key = collection->getNextKey(key)) { // HK_INVALID_SHAPE_KEY
                    if (collection->getCollisionFilterInfo(key) != COLLIDE_NO_COLLISION) {
                        collection->setCollisionFilterInfo(key, 0);
                    }
                }
            }
        }
    }
    if ((component->get_flags() & VCF_SHAPE_IN_PARENT) != 0) {
        vehicle_component_set_hierarchy_collision(component, this, COLLIDE_NO_COLLISION);
    }
    debris_info.m_has_precalc_inertia = true;
    debris_info.m_precalc_center_of_mass = comp_data->m_cm_dynamics_info.m_center_of_mass;
    debris_info.m_precalc_tensor = comp_data->m_cm_dynamics_info.m_inertia_tensor;
    debris_info.m_precalc_tensor.rvec.x *= debris_info.m_mass;
    debris_info.m_precalc_tensor.rvec.y *= debris_info.m_mass;
    debris_info.m_precalc_tensor.rvec.z *= debris_info.m_mass;
    debris_info.m_precalc_tensor.uvec.x *= debris_info.m_mass;
    debris_info.m_precalc_tensor.uvec.y *= debris_info.m_mass;
    debris_info.m_precalc_tensor.uvec.z *= debris_info.m_mass;
    debris_info.m_precalc_tensor.fvec.x *= debris_info.m_mass;
    debris_info.m_precalc_tensor.fvec.y *= debris_info.m_mass;
    debris_info.m_precalc_tensor.fvec.z *= debris_info.m_mass;
    havok_adjust_tensor(debris_info.m_precalc_tensor, 0.5f, 0.2f);
    const bool is_wheel = vehicle_component_is_wheel(comp_data); // inlined in rfg.exe
    if (use_impact_sound) {
        debris_info.m_impact_foley_id = is_wheel ? this->info->m_foley_wheel_impact_id : this->info->m_foley_component_impact_id;
        debris_info.m_delay_before_first_impact_foley = delay_before_first_impact_sound;
    }
    else {
        debris_info.m_impact_foley_id = -1;
    }
    // about one in nine of a wreck's parts (not wheels) can be collected as salvage
    if (m_flags.m_corpse != 0 && !is_wheel && vrand() % 100 < 11) {
        debris_info.m_is_persistent = true;
        debris_info.m_is_retrievable = true;
    }
    obj_debris_props props(&debris_info);
    object_debris* debris = World.create_object<object_debris>(props, NULL);
    if (debris == NULL) {
        component->destroy_renderable_instance(this); // inlined in rfg.exe
    }
    else {
        component->m_debris_object = debris->get_handle();
        if (from_nano) {
            object_debris_mark_for_fade(debris, 0, 2000, true);
        }
        else {
            object_debris_mark_for_fade(debris, int_rand_range(30000, 210000), 1000, false); // 30 s to 3.5 min
        }
        if (m_flags.m_corpse != 0 && this->info->m_effect_component_fire != 0xFFFFFFFF && vrand() % 100 <= (unsigned int)Component_fire_chance && explode) {
            debris->m_effect_instance = effects_play(this->info->m_effect_component_fire, debris->pos, debris->orient, false, debris->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            debris->m_effect_end.set_rand(2000, 10000);
        }
        if (from_type != DBT_MELEE && debris->havok_handle != 0xFFFFFFFF) {
            vector kick_point;
            vector direction = vehicle_detach_kick_direction(this, debris, kick_point);
            if (2.5f < volume) {
                havok_set_ignore_property(havok_body_get_pointer(debris->havok_handle), get_handle());
            }
            if (1500.0f < debris_info.m_mass) {
                const float scale = debris_info.m_mass / 1500.0f; // heavy pieces get thrown as hard as 1.5 t ones, per kg more
                direction.x *= scale;
                direction.y *= scale;
                direction.z *= scale;
            }
            havok_body_apply_point_velocity(debris->havok_handle, direction, kick_point);
        }
        component_rinstance_cs.enter();
        component->m_renderable_instance_p = NULL; // the debris owns it now
        component->m_renderable_instance_refs = 0;
        component_rinstance_cs.leave();
    }
    if (debris_info.m_shape != NULL) {
        debris_info.m_shape->removeReference();
    }
    // what hung on this part or below now hangs on the debris
    object* debris_object = World.object_from_handle(component->m_debris_object);
    if (debris_object != NULL) {
        constraint* cp = host_constraint_ptr;
        while (cp != NULL) {
            constraint* next = cp->host_constraint_next != host_constraint_ptr ? cp->host_constraint_next : NULL;
            for (unsigned int c = vehicle_get_component_index_from_shape_key(this, cp->host_attach_index, cp->host_alt_hk_body_index); c != 0xFFFFFFFF; c = data->m_components[c].get_data()->m_parent_index) {
                if (c == (unsigned int)index) {
                    object_constraint_transfer(true, cp, World.object_from_handle(cp->child_handle), this, debris_object, NULL);
                }
            }
            cp = next;
        }
    }
    component->set_flags(VCF_DETACHED);
    component->free_rigid_body_and_constraints(this);
}

// 0x00BA0C10 (:3468)
void vehicle_component_shatter_glass(vehicle_component* p_component, vehicle* vp, const vector* damage_hit_position, const vector* damage_hit_normal, const vector* damage_velocity)
{
    if ((p_component->get_flags() & VCF_NOT_RENDERED) != 0) {
        return;
    }
    vehicle_component_set_hierarchy_collision(p_component, vp, COLLIDE_NO_COLLISION);
    // items stuck to the pane go with it
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    constraint* cp = vp->host_constraint_ptr;
    while (cp != NULL) {
        constraint* next = cp->host_constraint_next != vp->host_constraint_ptr ? cp->host_constraint_next : NULL;
        object* child = cp->child_handle != 0xFFFFFFFF ? World.object_from_handle(cp->child_handle) : NULL;
        if (child != NULL && child->get_obj_type() == OT_ITEM && child->get_sub_type() == (object_sub_type)8 && cp->host_attach_index != -1) {
            const unsigned int index = vehicle_get_component_index_from_shape_key(vp, cp->host_attach_index, cp->host_alt_hk_body_index);
            if (data != NULL && index == (unsigned int)(p_component - data->m_components)) {
                World.destroy_object(cp, false);
            }
        }
        cp = next;
    }
    p_component->set_flags(VCF_NOT_RENDERED);
    vehicle_component_shatter_children(p_component, vp, damage_hit_position, damage_hit_normal, damage_velocity);
    p_component->destroy_renderable_instance_absolute(vp);
    vp->set_render_flags(8);
    vehicle_component_play_shatter_effect(p_component, vp, damage_velocity);
}

// 0x00B7B810 (:3587)
static void vehicle_component_calc_ws_pos_orient_recursive(vehicle_component* component, vehicle* vp, const matrix* parent_ws_orient, const vector* parent_ws_pos)
{
    const vehicle_component_data* comp_data = component->get_data();
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    if (component->havok_handle == 0xFFFFFFFF) {
        const unsigned int flags = component->get_flags();
        const bool deformed = component->m_pos_orient_deform_weight != 0;
        vector position;
        if ((flags & VCF_HAS_POSITION) == 0) {
            position = *parent_ws_pos;
            if ((flags & VCF_DEFORM_POSITION) != 0 && deformed) {
                vector offset = comp_data->m_deformed_position_offset;
                offset.unrotate(*parent_ws_orient);
                const float weight = mesh_deform_weight_convert(component->m_pos_orient_deform_weight);
                position.x += offset.x * weight;
                position.y += offset.y * weight;
                position.z += offset.z * weight;
            }
        }
        else {
            position = comp_data->m_position;
            if ((flags & VCF_DEFORM_POSITION) != 0 && deformed) {
                const float weight = mesh_deform_weight_convert(component->m_pos_orient_deform_weight);
                position.x += comp_data->m_deformed_position_offset.x * weight;
                position.y += comp_data->m_deformed_position_offset.y * weight;
                position.z += comp_data->m_deformed_position_offset.z * weight;
            }
            position.unrotate(*parent_ws_orient);
            position.x += parent_ws_pos->x;
            position.y += parent_ws_pos->y;
            position.z += parent_ws_pos->z;
        }
        matrix rotation = (flags & VCF_HAS_ROTATION) == 0 ? *parent_ws_orient : comp_data->m_rotation * *parent_ws_orient;
        if ((flags & VCF_DEFORM_ROTATION) != 0 && deformed) {
            float weight = (float)component->m_pos_orient_deform_weight * 0.003921569f; // 1 / 255
            if (weight - 0.0f < 0.0f) {
                weight = 0.0f;
            }
            if (0.0f <= weight - 1.0f) {
                weight = 1.0f;
            }
            quaternion offset;
            quat_lerp(offset, IDENTITY_QUATERNION, comp_data->m_deformed_rotation_offset, weight);
            matrix offset_rotation;
            offset.extract_matrix(offset_rotation);
            rotation = offset_rotation * rotation;
        }
        if (comp_data->m_type == VCT_DOOR && (flags & VCF_UNLATCHED) != 0 && component->m_constraint == NULL) {
            // a door without a body (too small for one) turns by hand
            const vehicle_component_data::hinge* hinge_data = (const vehicle_component_data::hinge*)comp_data->m_hinge;
            const float angle = hinge_data->m_max_angle * component->m_type_attribute.door.m_t;
            vector axis = hinge_data->m_axis;
            axis.unrotate(*parent_ws_orient);
            rotation.rotate_around_local_vector(axis, -angle);
        }
        component->set_orient_pos_in_world_space(vp, rotation, position);
    }
    for (unsigned int i = 0; i < comp_data->m_num_children; i++) {
        vehicle_component_calc_ws_pos_orient_recursive(&data->m_components[comp_data->m_children_indices[i]], vp, &component->get_world_space_rotation(), &component->get_world_space_position());
    }
}

// 0x00B7BD10 (:3704)
void vehicle_calc_ws_pos_orient_for_components(vehicle* vp, bool force_update)
{
    if (vp->m_flags.fixed_motion == 0 && !force_update) {
        hkpRigidBody* body = havok_body_get_pointer(vp->havok_handle);
        if (body == NULL || !body->isActive()) {
            return;
        }
    }
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        if (data->m_components[i].get_data()->m_parent_index == 0xFFFFFFFF) {
            vehicle_component_calc_ws_pos_orient_recursive(&data->m_components[i], vp, &vp->orient, &vp->pos);
        }
    }
}

// 0x00B7BDA0 (:3741)
void vehicle_calc_cs_pos_orient_for_components(vehicle* vp)
{
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_component& component = data->m_components[i];
        component.update_chassis_space(vp);
        vector position = component.get_chassis_space_position();
        matrix rotation = component.get_chassis_space_rotation();
        position.unrotate(vp->orient);
        position.x += vp->pos.x;
        position.y += vp->pos.y;
        position.z += vp->pos.z;
        rotation.unrotate(vp->orient);
        component.set_orient_pos_in_world_space(vp, rotation, position);
    }
}

// 0x00B7BED0 (:3770)
bool vehicle_component_is_attached(const vehicle* vp, unsigned int component_id)
{
    return (vp->rdd.get_data()->m_components[component_id].get_flags() & VCF_DETACHED) == 0;
}

// 0x00B66210 (:3790)
unsigned int vehicle_get_axle_id(const vehicle_component_data* component)
{
    const unsigned int type = component->m_type;
    if (type == VCT_FRONT_WHEEL) {
        return 0;
    }
    if (type == VCT_REAR_WHEEL) {
        return 1;
    }
    if (type == VCT_AXLE_3_WHEEL) {
        return 2;
    }
    if (type == VCT_AXLE_4_WHEEL) {
        return 3;
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return 0;
}

// 0x00B7BF00 (:3814)
unsigned int vehicle_get_component_hit_points(unsigned int v_handle, unsigned int component)
{
    vehicle* vp = vehicle_from_handle(v_handle); // Original bug kept: not checked for null
    return vp->rdd.is_loaded() ? vp->rdd.get_data()->m_components[component].m_hit_points : 0;
}

// 0x00B7BF40 (:3834)
void vehicle_set_component_hit_points(unsigned int v_handle, unsigned int component, unsigned int hit_points)
{
    vehicle* vp = vehicle_from_handle(v_handle); // Original bug kept: not checked for null
    if (vp->rdd.is_loaded()) {
        // Original bug kept: the upper bound is a lower bound, as in vehicle_component::apply_damage.
        vp->rdd.get_data()->m_components[component].m_hit_points = (unsigned short)(hit_points < 0xFFFF ? 0xFFFF : hit_points);
    }
}

// 0x00B7BF90 (:3855)
bool vehicle_get_component_unlatched(unsigned int v_handle, unsigned int component_index)
{
    if (66 <= component_index) {
        return false;
    }
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL && (vp->rdd.get_data()->m_components[component_index].get_flags() & VCF_UNLATCHED) != 0;
}

// 0x00B7BFE0 (:3886)
bool vehicle_get_component_detached(unsigned int v_handle, unsigned int component_index)
{
    if (66 <= component_index) {
        return false;
    }
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL && (vp->rdd.get_data()->m_components[component_index].get_flags() & VCF_DETACHED) != 0;
}

// 0x00BA0D40 (:3915)
void vehicle_set_component_detached(unsigned int v_handle, unsigned int component_index, bool detached)
{
    if (66 <= component_index) {
        return;
    }
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return;
    }
    // only ever detaches (the multiplayer sync of a part already gone)
    vehicle_component& component = vp->rdd.get_data()->m_components[component_index];
    if ((component.get_flags() & VCF_DETACHED) == 0 && detached) {
        component.set_flags(VCF_DETACHED);
        component.free_rigid_body_and_constraints(vp);
        component.destroy_renderable_instance(vp);
        if ((component.get_flags() & VCF_SHAPE_IN_PARENT) != 0) {
            vehicle_component_set_hierarchy_collision(&component, vp, COLLIDE_NO_COLLISION);
        }
    }
}

// 0x00BBBC90 (:3962)
void vehicle_components_process_damage(const vehicle_component_accident_info*& accidents, unsigned int& num_accidents)
{
    accidents = Accidents_this_frame;
    num_accidents = 0;
    for (unsigned int i = 0; i < Num_damaged_components_this_frame; i++) {
        const damaged_component_post_process_info& info = Damaged_components_this_frame[i];
        const bool visible = info.m_component->process_damage(info.m_vehicle, info.m_damaged_by_type, &info.m_damage_velocity, &info.m_damage_hit_position, &info.m_damage_hit_normal, info.m_weapon_info); // inlined in rfg.exe
        if (num_accidents < 128) {
            vehicle* other = World.object_from_handle<vehicle>(info.m_component->m_last_hit_by_handle);
            if (other != NULL) {
                unsigned int a = 0;
                for (; a < num_accidents; a++) {
                    if ((Accidents_this_frame[a].m_vehicle_a == info.m_vehicle && Accidents_this_frame[a].m_vehicle_b == other) || (Accidents_this_frame[a].m_vehicle_a == other && Accidents_this_frame[a].m_vehicle_b == info.m_vehicle)) {
                        break;
                    }
                }
                if (num_accidents <= a) {
                    Accidents_this_frame[num_accidents].m_vehicle_a = info.m_vehicle;
                    Accidents_this_frame[num_accidents].m_vehicle_b = other;
                    Accidents_this_frame[num_accidents].m_visible_damage = visible;
                    num_accidents++;
                }
            }
        }
    }
    Num_damaged_components_this_frame = 0;
}

// 0x00B66270 (:4012)
bool vehicle_component_enable_collision_with_humans(vehicle_component* component, bool enable)
{
    hkpRigidBody* body = havok_body_get_pointer(component->havok_handle);
    if (body == NULL) {
        return false;
    }
    havok_body_set_collision_type(body, enable ? component->m_original_collision_filter : (component->m_original_collision_filter & 0xFFFF0035) | 0x35, false, false);
    return true;
}
