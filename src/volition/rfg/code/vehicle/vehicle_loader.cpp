// Original: volition/rfg/code/vehicle/vehicle_loader.cpp
//
// The GSP_VEHICLE load callback: a .ccar_pc holds a vehicle_subinfo whose pointers are offsets from its start; this
// patches them and creates the render mesh, materials, morph target, MOPPs, collision models and tags.

#include "volition/rfg/code/vehicle/vehicle_loader.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/geometryutilities/inertia/hkinertiatensorcomputer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_data.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_morph_target.h"
#include "volition/render_lib/code/rl_morph_data.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_mopp_code.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/vehicle/vehicle_deformation.h"
#include "volition/rfg/code/vehicle/vehicle_subinfo.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>
#include <stdio.h>

// ---- file-static globals
static str_data_hash_table<vehicle_subinfo*,32,64,unsigned char> Vehicle_filename_hash; // 0x03770230

extern float hkConvexShapeDefaultRadius; // hkpconvexshape.cpp

static const unsigned int VEHICLE_SUBINFO_VERSION = 0x1C;
static const unsigned int VEHICLE_CM_INDEX_NONE = 0xFFFFFFFF;

// The file's nullable offset to pointer patch (inlined in rfg.exe): -1 and 0 are null.
template<typename T>
static void vehicle_patch_offset(et_sized_native_pointer<T>& p, void* base)
{
    const int offset = (int)(T*)p;
    p = (offset == -1 || offset == 0) ? (T*)NULL : (T*)((char*)base + offset);
}

// A zero axis means no tensor came with the model.
static bool vehicle_tensor_is_degenerate(const matrix& tensor)
{
    return sqrtf(tensor.rvec.x * tensor.rvec.x + tensor.rvec.y * tensor.rvec.y + tensor.rvec.z * tensor.rvec.z) <= 0.001f
        || sqrtf(tensor.uvec.x * tensor.uvec.x + tensor.uvec.y * tensor.uvec.y + tensor.uvec.z * tensor.uvec.z) <= 0.001f
        || sqrtf(tensor.fvec.x * tensor.fvec.x + tensor.fvec.y * tensor.fvec.y + tensor.fvec.z * tensor.fvec.z) <= 0.001f;
}

// 0x00B83BE0 (:65)
static unsigned int vehicle_generate_collision_model(vehicle_subinfo* subinfo)
{
    vector bmin;
    vector bmax;
    ((rl_mesh*)subinfo->m_vehicle_mesh)->get_subpiece_local_bb(subinfo->m_chassis_submesh_id, bmin, bmax);
    // half extents; the height is cut to 80 %
    const hkVector4f half_extents((bmax.x - bmin.x) * 0.5f, (bmax.y - bmin.y) * 0.8f * 0.5f, (bmax.z - bmin.z) * 0.5f, 0.0f);
    hkpBoxShape* shape = new hkpBoxShape(half_extents, hkConvexShapeDefaultRadius);
    return havok_cm_manager_add(shape);
}

// 0x00B83D10 (:98)
static void vehicle_generate_component_cm(vehicle_component_data* component, vehicle_subinfo* sub_info_p)
{
    vector bmin;
    vector bmax;
    ((rl_mesh*)sub_info_p->m_vehicle_mesh)->get_subpiece_local_bb(component->m_submesh_id, bmin, bmax);
    vector center((bmin.x + bmax.x) * 0.5f, (bmax.y + bmin.y) * 0.5f, (bmax.z + bmin.z) * 0.5f);
    vector half_extents((bmax.x - bmin.x) * 0.5f, (bmax.y - bmin.y) * 0.5f, (bmax.z - bmin.z) * 0.5f);
    quaternion rotation = IDENTITY_QUATERNION;
    component->m_cm_dynamics_info.m_cm_index = havok_cm_manager_add(havok_cm_manager_create_box_shape(&rotation, &center, &half_extents));
}

// 0x00B83EB0 (:128) (LTCG: filename in ECX)
static bool vehicle_load_collision_models(vehicle_subinfo* subinfo, const char* filename)
{
    unsigned int cm_indices[200];
    unsigned int num_cms = 0;
    unsigned int chunk_size = 0;
    vehicle_patch_offset(subinfo->m_collision_model_data, subinfo);
    if (!havok_cm_manager_load_all(&chunk_size, (unsigned char*)subinfo->m_collision_model_data, cm_indices, 200, &num_cms, filename)) {
        char message[256];
        sprintf(message, "havok_vehicle_load_collision_model: Failed to load the collision models!");
        keen::debug::nativePrintArgs(message);
        return false;
    }

    if (subinfo->m_cm_dynamics_info.m_cm_index != VEHICLE_CM_INDEX_NONE) {
        subinfo->m_cm_dynamics_info.m_cm_index = cm_indices[subinfo->m_cm_dynamics_info.m_cm_index];
    }
    for (unsigned int i = 0; i < subinfo->m_num_components; i++) {
        vehicle_cm_dynamics_info& info = subinfo->m_components[i].m_cm_dynamics_info;
        if (info.m_cm_index == VEHICLE_CM_INDEX_NONE) {
            continue;
        }
        info.m_cm_index = cm_indices[info.m_cm_index];
        if (vehicle_tensor_is_degenerate(info.m_inertia_tensor)) {
            havok_adjust_tensor(info.m_inertia_tensor, 0.5f, 0.1f);
        }
        if (!info.m_center_of_mass.is_valid()) {
            info.m_center_of_mass.zero();
        }
    }

    if (subinfo->m_cm_dynamics_info.m_cm_index == VEHICLE_CM_INDEX_NONE) {
        subinfo->m_cm_dynamics_info.m_cm_index = vehicle_generate_collision_model(subinfo);
        havok_cm_manager_get(subinfo->m_cm_dynamics_info.m_cm_index);
    }
    if (vehicle_tensor_is_degenerate(subinfo->m_cm_dynamics_info.m_inertia_tensor)) {
        subinfo->m_cm_dynamics_info.m_inertia_tensor.make_identity();
    }

    for (unsigned int i = 0; i < subinfo->m_num_components; i++) {
        vehicle_cm_dynamics_info& info = subinfo->m_components[i].m_cm_dynamics_info;
        if (info.m_cm_index != VEHICLE_CM_INDEX_NONE) {
            continue;
        }
        // a component without a collision model: a box from its render bounds, 60 kg/m^3
        vehicle_generate_component_cm(&subinfo->m_components[i], subinfo);
        hkMassProperties mass_properties;
        hkpInertiaTensorComputer::computeShapeVolumeMassProperties(havok_cm_manager_get(info.m_cm_index), 60.0f, mass_properties);
        havok_to_game_matrix(info.m_inertia_tensor, (const hkRotationf&)mass_properties.m_inertiaTensor);
        havok_to_game_vector(info.m_center_of_mass, mass_properties.m_centerOfMass);
        if (vehicle_tensor_is_degenerate(info.m_inertia_tensor)) {
            havok_adjust_tensor(info.m_inertia_tensor, 0.5f, 0.1f);
        }
        if (!info.m_center_of_mass.is_valid()) {
            info.m_center_of_mass.zero();
        }
    }
    return true;
}

// 0x00B68B40 (:269)
static bool vehicle_unload_collision_models(vehicle_subinfo* subinfo)
{
    if (subinfo == NULL) {
        return false;
    }
    havok_cm_manager_remove_from_list(subinfo->m_cm_dynamics_info.m_cm_index);
    for (unsigned int i = 0; i < subinfo->m_num_components; i++) {
        havok_cm_manager_remove_from_list(subinfo->m_components[i].m_cm_dynamics_info.m_cm_index);
    }
    return true;
}

// A light's on and off tags (components and chassis).
static void vehicle_load_light_tags(vehicle_mesh_light& light, vehicle_subinfo* subinfo)
{
    int offset = 0;
    vehicle_patch_offset(light.off_mesh_tags_offset, subinfo);
    light.off_mesh_tags.read_tag_data((unsigned char*)(mesh_tags*)light.off_mesh_tags_offset, offset);
    offset = 0;
    vehicle_patch_offset(light.on_mesh_tags_offset, subinfo);
    light.on_mesh_tags.read_tag_data((unsigned char*)(mesh_tags*)light.on_mesh_tags_offset, offset);
}

// 0x00B843C0 (:294)
static bool vehicle_load_components(vehicle_subinfo* subinfo, const char* filename)
{
    const unsigned int num_components = subinfo->m_num_components;
    for (unsigned int i = 0; i < num_components; i++) {
        vehicle_component_data& component = subinfo->m_components[i];
        vehicle_patch_offset(component.m_children_indices, subinfo);
        // the file holds the material's string hash, the game its name checksum
        for (unsigned char m = 0; (int)m < rfg_get_physical_material_count(); m++) {
            const rfg_physical_material* material = rfg_get_physical_material_by_index(m);
            if (str_hash_string(material->name) == component.m_mass_type) {
                component.m_mass_type = material->name_checksum;
                break;
            }
        }
        int offset = 0;
        vehicle_patch_offset(component.m_tags_offset, subinfo);
        component.m_tags.read_tag_data((unsigned char*)(mesh_tags*)component.m_tags_offset, offset);
        vehicle_patch_offset(component.m_lights, subinfo);
        for (unsigned int l = 0; l < component.m_num_lights; l++) {
            vehicle_load_light_tags(component.m_lights[l], subinfo);
        }
        vehicle_patch_offset(component.m_hinge, subinfo);
        vehicle_patch_offset(component.m_deformed_morph_target, subinfo);
        // an index into the (already patched) MOPP table, not an offset
        const int mopp = (int)(const hkpMoppCode*)component.m_cm_dynamics_info.m_mopp_code;
        component.m_cm_dynamics_info.m_mopp_code = mopp == -1 ? (const hkpMoppCode*)NULL : (const hkpMoppCode*)subinfo->m_mopp_pointer_table[mopp];
    }
    // all components at durability 1: all of them become durability 3
    for (unsigned int i = 0; i < num_components; i++) {
        if (subinfo->m_components[i].m_durability != 1) {
            return true;
        }
    }
    for (unsigned int i = 0; i < num_components; i++) {
        subinfo->m_components[i].m_durability = 3;
    }
    return true;
}

// 0x00B845B0 (:418) (LTCG: subinfo in ESI)
static bool vehicle_load_chassis(vehicle_subinfo* subinfo)
{
    int offset = 0;
    vehicle_patch_offset(subinfo->m_chassis_tags_offset, subinfo);
    subinfo->m_chassis_tags.read_tag_data((unsigned char*)(mesh_tags*)subinfo->m_chassis_tags_offset, offset);
    vehicle_patch_offset(subinfo->m_deformed_morph_target, subinfo);
    for (unsigned int l = 0; l < subinfo->m_num_lights; l++) {
        vehicle_load_light_tags(subinfo->m_lights[l], subinfo);
    }
    const int mopp = (int)(const hkpMoppCode*)subinfo->m_cm_dynamics_info.m_mopp_code;
    subinfo->m_cm_dynamics_info.m_mopp_code = mopp == -1 ? (const hkpMoppCode*)NULL : (const hkpMoppCode*)subinfo->m_mopp_pointer_table[mopp];
    return true;
}

// 0x00B846D0 (:469) (LTCG: subinfo in ESI)
static bool vehicle_load_mopps(vehicle_subinfo* subinfo)
{
    vehicle_patch_offset(subinfo->m_mopp_pointer_table, subinfo);
    for (unsigned int i = 0; i < subinfo->m_num_mopps; i++) {
        et_sized_native_pointer<hkpMoppCode const>& entry = subinfo->m_mopp_pointer_table[i];
        vehicle_patch_offset(entry, subinfo);
        const havok_mopp_code_header* header = (const havok_mopp_code_header*)(const hkpMoppCode*)entry;
        if (header != NULL) {
            entry = havok_mopp_code_load_in_place(header);
            if ((const hkpMoppCode*)entry == NULL) {
                return false;
            }
        }
    }
    return true;
}

// 0x00B91530 (:505) (LTCG: subinfo in ESI, gpu_data in EDX, pFilename in ECX)
static bool vehicle_load_rl_data(vehicle_subinfo* subinfo, unsigned char* gpu_data, int& gpu_buf_offset, const char* pFilename)
{
    int cpu_offset = 0;
    vehicle_patch_offset(subinfo->m_vehicle_mesh, subinfo);
    LoadedMeshInfo name(nullptr, nullptr);
    name.name[0] = '\0';
    keen::copyString(name.name, pFilename, sizeof(name.name));
    rl_mesh_data* mesh_data = rl_mesh_data_load_validated((unsigned char*)(rl_mesh*)subinfo->m_vehicle_mesh, gpu_data, cpu_offset, gpu_buf_offset, &name);
    if (mesh_data == NULL) {
        return false;
    }
    rl_mesh* mesh = rl_mesh::create();
    subinfo->m_vehicle_mesh = mesh;
    if (mesh == NULL) {
        return false;
    }
    mesh->init(mesh_data, name, false);

    cpu_offset = 0;
    vehicle_patch_offset(subinfo->m_material_map, subinfo);
    rl_material_map_data* map_data = rl_material_map_data_load((unsigned char*)(rl_material_map*)subinfo->m_material_map, cpu_offset);
    vehicle_patch_offset(subinfo->m_materials, subinfo);
    vehicle_patch_offset(subinfo->m_texture_names, subinfo);
    for (unsigned int i = 0; i < subinfo->m_num_materials; i++) {
        cpu_offset = 0;
        vehicle_patch_offset(subinfo->m_materials[i], subinfo);
        rl_material_data* material_data = rl_material_data_load((unsigned char*)(rl_material*)subinfo->m_materials[i], cpu_offset, (const char*)subinfo->m_texture_names, true);
        if (material_data == NULL) {
            for (unsigned int j = 0; j < i; j++) {
                rl_material_library_unregister((rl_material*)subinfo->m_materials[j]);
                ((rl_material*)subinfo->m_materials[j])->release();
            }
            ((rl_mesh*)subinfo->m_vehicle_mesh)->release();
            return false;
        }
        rl_material* material = rl_material::create();
        subinfo->m_materials[i] = material;
        material->set_material_data(material_data);
        map_data->materials[i] = rl_material_library_register(material);
    }
    rl_material_map* map = rl_material_map::create();
    subinfo->m_material_map = map;
    map->set_map_data(map_data);
    return true;
}

// 0x00B84740 (:575) (LTCG: subinfo in ESI, gpu_buf_offset in EAX)
static bool vehicle_load_rl_morph_data(vehicle_subinfo* subinfo, unsigned char* gpu_buf, int& gpu_buf_offset)
{
    if ((rl_morph_container*)subinfo->m_morph_target == NULL) {
        return true;
    }
    // the morph data starts on a 64 byte boundary of the GPU file
    int remainder = gpu_buf_offset % 0x40;
    gpu_buf_offset += remainder == 0 ? 0 : 0x40 - remainder;
    int cpu_offset = 0;
    vehicle_patch_offset(subinfo->m_morph_target, subinfo);
    rl_morph_data* morph_data = rl_morph_data_load((unsigned char*)(rl_morph_container*)subinfo->m_morph_target, gpu_buf, cpu_offset, gpu_buf_offset);
    if (morph_data == NULL) {
        return false;
    }
    if (!Vehicle_deformation_enabled) {
        subinfo->m_morph_target = (rl_morph_container*)NULL;
        return true;
    }
    rl_morph_container* morph = rl_morph_container::create();
    subinfo->m_morph_target = morph;
    if (morph == NULL) {
        return false;
    }
    morph->set_morph_data(morph_data);
    return true;
}

// 0x00B847F0 (:617) (LTCG: subinfo in ECX)
static bool vehicle_load_constraints(vehicle_subinfo* subinfo)
{
    vehicle_patch_offset(subinfo->m_constraint_templates, subinfo);
    return true;
}

// 0x00B98E20 (:643)
bool vehicle_subinfo_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_size, unsigned char* gpu_data, int gpu_data_size)
{
    GAME_TRACE("vehicle", "container load: %s", filename);
    vehicle_subinfo* subinfo = (vehicle_subinfo*)cpu_data;
    if (subinfo->m_version != VEHICLE_SUBINFO_VERSION) {
        return false;
    }
    vehicle_patch_offset(subinfo->m_seat_to_door_links, subinfo);
    vehicle_patch_offset(subinfo->m_mesh_tag_deformation_offsets, subinfo);
    vehicle_patch_offset(subinfo->m_lights, subinfo);
    vehicle_patch_offset(subinfo->m_spline_data, subinfo);
    for (unsigned int i = 0; i < subinfo->m_num_splines; i++) {
        vehicle_patch_offset(subinfo->m_spline_data[i].control_points, subinfo);
        vehicle_patch_offset(subinfo->m_spline_data[i].attached_component_index, subinfo);
    }
    // each spline's center: the mean of its control points
    for (unsigned int i = 0; i < subinfo->m_num_splines; i++) {
        vehicle_subinfo::vehicle_spline_data& spline = subinfo->m_spline_data[i];
        vector sum;
        sum.zero();
        for (unsigned int p = 0; p < spline.num_control_points; p++) {
            const vector& point = spline.control_points[p];
            sum.x += point.x;
            sum.y += point.y;
            sum.z += point.z;
        }
        const float scale = 1.0f / (float)spline.num_control_points;
        spline.center.set(scale * sum.x, sum.y * scale, scale * sum.z);
    }

    int gpu_buf_offset = 0;
    if (!vehicle_load_mopps(subinfo)
        || !vehicle_load_rl_data(subinfo, gpu_data, gpu_buf_offset, filename)
        || !vehicle_load_rl_morph_data(subinfo, gpu_data, gpu_buf_offset)
        || !vehicle_load_chassis(subinfo)
        || !vehicle_load_components(subinfo, filename)
        || !vehicle_load_collision_models(subinfo, filename)) {
        return false;
    }
    vehicle_load_constraints(subinfo);
    Vehicle_filename_hash.insert_key_data(filename, subinfo);
    return true;
}

// 0x00B91710 (:788)
void vehicle_subinfo_unload(const char* filename, unsigned int srid)
{
    GAME_TRACE("vehicle", "container unload: %s", filename);
    vehicle_subinfo* subinfo = NULL;
    if (Vehicle_filename_hash.get(filename, subinfo) && subinfo != NULL) {
        ((rl_mesh*)subinfo->m_vehicle_mesh)->release();
        subinfo->m_vehicle_mesh = (rl_mesh*)NULL;
        ((rl_material_map*)subinfo->m_material_map)->release();
        subinfo->m_material_map = (rl_material_map*)NULL;
        if ((rl_morph_container*)subinfo->m_morph_target != NULL) {
            ((rl_morph_container*)subinfo->m_morph_target)->release();
            subinfo->m_morph_target = (rl_morph_container*)NULL;
        }
        for (unsigned int i = 0; i < subinfo->m_num_materials; i++) {
            rl_material_library_unregister((rl_material*)subinfo->m_materials[i]);
            ((rl_material*)subinfo->m_materials[i])->release();
            subinfo->m_materials[i] = (rl_material*)NULL;
        }
        for (unsigned int i = 0; i < subinfo->m_num_mopps; i++) {
            const hkpMoppCode* mopp = subinfo->m_mopp_pointer_table[i];
            if (mopp != NULL) {
                const_cast<hkpMoppCode*>(mopp)->removeReference();
                subinfo->m_mopp_pointer_table[i] = (const hkpMoppCode*)NULL;
            }
        }
        vehicle_unload_collision_models(subinfo); // inlined in rfg.exe
    }
    Vehicle_filename_hash.remove(filename);
}

// 0x00B91880 (:831)
vehicle_subinfo* vehicle_subinfo_find(const char* filename)
{
    vehicle_subinfo* subinfo = NULL;
    Vehicle_filename_hash.get(filename, subinfo);
    return subinfo;
}

// 0x00B68B90 (:838)
void vehicle_loader_system_init()
{
}
