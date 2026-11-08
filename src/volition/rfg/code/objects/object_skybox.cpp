// Original: volition/rfg/code/objects/object_skybox.cpp
//
// The sky: the sky dome mesh (rfg_skybox, rfg-skybox shader with the ten time-of-day hemisphere colors), the cloud
// layers and horizon mountains (skybox_clouds: base and overhead cloud materials, the matte layer), both preloaded from
// skybox_containers.asm_pc, combined in one rl_skybox_renderable_instance with the time of day's orbitals and set as
// the main scene's skybox. The update-render task follows the camera and feeds the time of day's colors, cloud
// lighting, cloud scrolling and fog values into the materials' constants every frame. The star field and meteor
// shower strengths follow the hour.

#include "volition/rfg/code/objects/object_skybox.h"

#include "volition/render_lib/code/rl_compound_mesh_instance.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_meteor_shower.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_shadow_system.h"
#include "volition/render_lib/code/rl_skybox_renderable_instance.h"
#include "volition/render_lib/code/rl_star_field.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/game_time_of_day.h"
#include "volition/rfg/code/geom/static_mesh.h"
#include "volition/rfg/code/geom/static_mesh_shared.h"
#include "volition/rfg/code/objects/object_skybox_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"

// ---- globals
static bool R_skybox = true; // 0x01694611: draw the sky
static bool R_sky_fog_camera = true; // 0x01694612: the sky fog offsets are relative to the camera height
static unsigned int Cloud_material_base_idx = 0xFFFFFFFF; // 0x01694694
static unsigned int Cloud_material_overhead_idx = 0xFFFFFFFF; // 0x01694698
static unsigned int Matte_layer_material_idx = 0xFFFFFFFF; // 0x0169469C
static float R_matte_fog_scale = 100.0f; // 0x0169467C
static float R_matte_fog_offset = 10.0f; // 0x01694680
static float R_sky_fog_scale = 100.0f; // 0x01694684
static float R_sky_fog_offset = 100.0f; // 0x01694688
static float R_clouds_fog_scale = 100.0f; // 0x0169468C
static float R_clouds_fog_offset = 100.0f; // 0x01694690
static rl_shader_const_ref Cloud_shader_const_layer_strengths; // 0x016AE074
static rl_shader_const_ref Cloud_shader_const_fade_height; // 0x016AE078
static rl_shader_const_ref Cloud_shader_const_full_height; // 0x016AE07C
static rl_shader_const_ref Cloud_shader_const_u_offset; // 0x016AE080
static rl_shader_const_ref Cloud_shader_const_tod_light_color_front; // 0x016AE084
static rl_shader_const_ref Cloud_shader_const_tod_light_color_rear; // 0x016AE088
static rl_shader_const_ref Cloud_shader_const_tod_light_direction; // 0x016AE08C
static rl_shader_const_ref Cloud_shader_const_sky_position; // 0x016AE090
static rl_shader_const_ref Cloud_shader_const_rimlight_scale; // 0x016AE094
static rl_shader_const_ref Cloud_shader_const_rimlight_power; // 0x016AE098
static rl_shader_const_ref Cloud_shader_const_scale; // 0x016AE09C
static rl_shader_const_ref Cloud_shader_const_offset; // 0x016AE0A0
static rl_shader_const_ref Matte_shader_const_mountain_color_front; // 0x016AE0A4
static rl_shader_const_ref Matte_shader_const_mountain_color_back; // 0x016AE0A8
static rl_shader_const_ref Matte_shader_const_mountain_fog_color; // 0x016AE0AC
static rl_shader_const_ref Matte_shader_const_mountain_fog_density; // 0x016AE0B0
static rl_shader_const_ref Matte_shader_const_light_direction; // 0x016AE0B4
static rl_shader_const_ref Matte_shader_const_scale; // 0x016AE0B8
static rl_shader_const_ref Matte_shader_const_offset; // 0x016AE0BC
static rl_shader_const_ref Skybox_shader_const_colors; // 0x016AE0C0
static rl_shader_const_ref Skybox_shader_const_sky_position; // 0x016AE0C4
static rl_shader_const_ref Skybox_shader_const_scale; // 0x016AE0C8
static rl_shader_const_ref Skybox_shader_const_offset; // 0x016AE0CC
static bool Skybox_init; // 0x0302F15B
static unsigned int Skybox_clouds_srid; // 0x0302F16C
static char Skybox_clouds_mesh_name[64]; // 0x0302F170
static static_mesh* Skybox_clouds_mesh; // 0x0302F1B0
static rl_compound_mesh_instance* Skybox_clouds_mesh_instance_p; // 0x0302F1B4
static unsigned int TOD_skybox_srid; // 0x0302F1B8
static static_mesh* TOD_skybox_mesh; // 0x0302F1BC
static char TOD_skybox_mesh_name[64]; // 0x0302F1C0
static rl_mesh_instance* TOD_skybox_mesh_instance_p; // 0x0302F200
static rl_renderable_instance* Current_skybox_mesh_instance_p; // 0x0302F204
static rl_skybox_renderable_instance* Combine_skybox_renderable_instance_p; // 0x0302F208
static game_render_task Update_render_task_skybox; // 0x030C070C

// ---- rl_skybox_renderable_instance (header inline)
// 0x00A1C310 (.h:67)
void rl_skybox_renderable_instance::set_render_orbitals_before(int before)
{
    m_render_orbitals_before = before;
}

// ---- internal functions
// 0x00A1C320 (:132)
static void object_skybox_free_all()
{
    game_render_set_skybox(nullptr);
    if (Skybox_clouds_mesh_instance_p != nullptr)
    {
        Skybox_clouds_mesh_instance_p->release();
    }
    Skybox_clouds_mesh_instance_p = nullptr;
    if (TOD_skybox_mesh_instance_p != nullptr)
    {
        TOD_skybox_mesh_instance_p->release();
    }
    TOD_skybox_mesh_instance_p = nullptr;
    Current_skybox_mesh_instance_p = nullptr;
    if (Combine_skybox_renderable_instance_p != nullptr)
    {
        Combine_skybox_renderable_instance_p->release();
    }
    Combine_skybox_renderable_instance_p = nullptr;
}

// 0x00A6B9C0 (:153, LTCG: mesh_p in EDI): the fog constants of a sky material. The scale constant is (far / half
// width, fog_scale / half height, far / half depth, half height) of the submesh drawn with the material (half extents
// of 1 when the mesh has none); the offset constant is fog_offset, relative to the camera height with
// R_sky_fog_camera.
static void set_fog_for_material(static_mesh* mesh_p, rl_material* mat_p, float fog_scale, float fog_offset, unsigned int scale_const, unsigned int offset_const)
{
    vector half_extents(1.0f, 1.0f, 1.0f);
    vector bmin = ZERO_VECTOR;
    vector bmax = ZERO_VECTOR;
    rl_mesh* mesh = mesh_p->m_shared_header.m_mesh.to_pointer();
    const rl_material_map* material_map = mesh_p->m_shared_header.m_material_map.to_pointer();
    for (int i = 0; i < mesh->get_mesh_data()->num_sub_meshes; i++)
    {
        const rl_submesh_data& submesh = mesh->get_mesh_data()->sub_meshes[i];
        const unsigned int material_handle = material_map->get_material(submesh.render_blocks.to_pointer()[0].material_map_idx);
        if (rl_material_library_get_material(material_handle) == mat_p)
        {
            bmin = submesh.bmin;
            bmax = submesh.bmax;
            half_extents = vector((bmax.x - bmin.x) * 0.5f, (bmax.y - bmin.y) * 0.5f, (bmax.z - bmin.z) * 0.5f);
            break;
        }
    }
    const float far_clip = game_render_get_far_clip_distance();
    float scale[4];
    scale[0] = far_clip * (1.0f / half_extents.x);
    scale[1] = (1.0f / half_extents.y) * fog_scale;
    scale[2] = game_render_get_far_clip_distance() * (1.0f / half_extents.z);
    scale[3] = fabsf(bmax.y - bmin.y) * 0.5f;
    mat_p->set_constant_data(scale_const, scale, 4);
    float offset[4];
    offset[0] = fog_offset;
    if (R_sky_fog_camera)
    {
        offset[0] = fog_offset - Rfg_camera.real_pos.y;
    }
    mat_p->set_constant_data(offset_const, offset, 1);
}

// The material of a sky mesh by index.
static rl_material* skybox_material(static_mesh* mesh_p, unsigned int index)
{
    return mesh_p->m_shared_header.m_materials.to_pointer()[index].to_pointer();
}

// 0x00A6BC90 (:197): the update-render task.
static void object_skybox_update_render_all(void*, rl_renderer*)
{
    if (TOD_skybox_mesh_instance_p != nullptr && TOD_skybox_mesh_instance_p->get_is_visible())
    {
        TOD_skybox_mesh_instance_p->set_is_visible(false);
    }
    if (Skybox_clouds_mesh_instance_p != nullptr && Skybox_clouds_mesh_instance_p->get_is_visible())
    {
        Skybox_clouds_mesh_instance_p->set_is_visible(false);
    }
    float sky_position[3] = { Rfg_camera.render_pos.x, Rfg_camera.render_pos.y, Rfg_camera.render_pos.z };
    Current_skybox_mesh_instance_p = Combine_skybox_renderable_instance_p;

    // The sky dome.
    rl_color_float sky_colors[10];
    game_time_of_day_get_skybox_tod_colors(sky_colors);
    rl_material* material_p = skybox_material(TOD_skybox_mesh, 0);
    material_p->set_constant_data(Skybox_shader_const_colors.get_ref(), &sky_colors[0].red, 40);
    material_p->set_constant_data(Skybox_shader_const_sky_position.get_ref(), sky_position, 3);
    set_fog_for_material(TOD_skybox_mesh, material_p, R_sky_fog_scale, R_sky_fog_offset, Skybox_shader_const_scale.get_ref(), Skybox_shader_const_offset.get_ref());

    // The clouds: the base (horizon) layer and the overhead layer.
    vector4 horizon_strengths;
    vector4 overhead_strengths;
    game_time_of_day_get_weather_settings(horizon_strengths, overhead_strengths);
    rl_color_float horizon_front;
    rl_color_float horizon_back;
    rl_color_float overhead_front;
    rl_color_float overhead_back;
    float backlight_strength;
    float backlight_power;
    vector light_direction;
    game_time_of_day_get_cloud_lighting(horizon_front, horizon_back, overhead_front, overhead_back, backlight_strength, backlight_power, light_direction);
    float layer01_scroll;
    float layer23_scroll;
    game_time_of_day_get_cloud_scroll_values(layer01_scroll, layer23_scroll);
    float single[4];
    float u_offset[4] = { layer01_scroll, layer23_scroll, 0.0f, 0.0f };

    material_p = skybox_material(Skybox_clouds_mesh, Cloud_material_base_idx);
    material_p->set_constant_data(Cloud_shader_const_tod_light_direction.get_ref(), &light_direction.x, 3);
    material_p->set_constant_data(Cloud_shader_const_tod_light_color_front.get_ref(), &horizon_front.red, 4);
    material_p->set_constant_data(Cloud_shader_const_tod_light_color_rear.get_ref(), &horizon_back.red, 4);
    material_p->set_constant_data(Cloud_shader_const_layer_strengths.get_ref(), &horizon_strengths.x, 4);
    single[0] = backlight_strength;
    single[1] = single[2] = single[3] = 0.0f;
    material_p->set_constant_data(Cloud_shader_const_rimlight_scale.get_ref(), single, 1);
    single[0] = backlight_power;
    material_p->set_constant_data(Cloud_shader_const_rimlight_power.get_ref(), single, 1);
    material_p->set_constant_data(Cloud_shader_const_u_offset.get_ref(), u_offset, 2);
    material_p->set_constant_data(Cloud_shader_const_sky_position.get_ref(), sky_position, 3);
    set_fog_for_material(Skybox_clouds_mesh, material_p, R_clouds_fog_scale, R_clouds_fog_offset, Cloud_shader_const_scale.get_ref(), Cloud_shader_const_offset.get_ref());

    material_p = skybox_material(Skybox_clouds_mesh, Cloud_material_overhead_idx);
    material_p->set_constant_data(Cloud_shader_const_tod_light_direction.get_ref(), &light_direction.x, 3);
    material_p->set_constant_data(Cloud_shader_const_tod_light_color_front.get_ref(), &overhead_front.red, 4);
    material_p->set_constant_data(Cloud_shader_const_tod_light_color_rear.get_ref(), &overhead_back.red, 4);
    material_p->set_constant_data(Cloud_shader_const_layer_strengths.get_ref(), &overhead_strengths.x, 4);
    single[0] = backlight_strength;
    material_p->set_constant_data(Cloud_shader_const_rimlight_scale.get_ref(), single, 1);
    single[0] = backlight_power;
    material_p->set_constant_data(Cloud_shader_const_rimlight_power.get_ref(), single, 1);
    material_p->set_constant_data(Cloud_shader_const_u_offset.get_ref(), u_offset, 2);
    material_p->set_constant_data(Cloud_shader_const_sky_position.get_ref(), sky_position, 3);
    // The overhead clouds take the sky dome's fog values, not the clouds'. Kept as in the binary.
    set_fog_for_material(Skybox_clouds_mesh, material_p, R_sky_fog_scale, R_sky_fog_offset, Cloud_shader_const_scale.get_ref(), Cloud_shader_const_offset.get_ref());
    // The overhead clouds' first texture also gives the cloud shadows, scrolled like the first layer.
    int cloud_texture;
    if (material_p->get_texture_data(cloud_texture, 0))
    {
        rl_shadow_system::m_pCloudTexture = get_dev_texture(cloud_texture).pData;
        rl_shadow_system::m_cloudUVScroll.x = layer01_scroll;
        rl_shadow_system::m_cloudUVScroll.y = 0.0f;
        rl_shadow_system::m_cloudUVScroll.z = 0.0f;
        rl_shadow_system::m_cloudUVScroll.w = 0.0f;
    }
    else
    {
        rl_shadow_system::m_pCloudTexture = nullptr;
    }

    // The horizon mountains (the matte layer).
    horizon_mountain_parameters mountain;
    get_time_of_day_get_horizon_mountain_params(mountain);
    material_p = skybox_material(Skybox_clouds_mesh, Matte_layer_material_idx);
    material_p->set_constant_data(Matte_shader_const_light_direction.get_ref(), &light_direction.x, 3);
    set_fog_for_material(Skybox_clouds_mesh, material_p, R_matte_fog_scale, R_matte_fog_offset, Matte_shader_const_scale.get_ref(), Matte_shader_const_offset.get_ref());
    // The light direction is set again, toward the sun light's position.
    const vector& sun_pos = game_render_get_TOD_light()->get_position();
    vector sun_dir(sun_pos.x, sun_pos.y, sun_pos.z);
    sun_dir.normalize_safe();
    material_p->set_constant_data(Matte_shader_const_light_direction.get_ref(), &sun_dir.x, 3);
    float color[4] = { mountain.color_front.red, mountain.color_front.green, mountain.color_front.blue, mountain.enabled ? 1.0f : 0.0f };
    material_p->set_constant_data(Matte_shader_const_mountain_color_front.get_ref(), color, 4);
    color[0] = mountain.color_back.red;
    color[1] = mountain.color_back.green;
    color[2] = mountain.color_back.blue;
    color[3] = mountain.enabled ? 1.0f : 0.0f;
    material_p->set_constant_data(Matte_shader_const_mountain_color_back.get_ref(), color, 4);
    material_p->set_constant_data(Matte_shader_const_mountain_fog_color.get_ref(), &mountain.fog_color.red, 4);
    single[0] = mountain.fog_density;
    single[1] = single[2] = single[3] = 0.0f;
    material_p->set_constant_data(Matte_shader_const_mountain_fog_density.get_ref(), single, 1);

    // The stars: full from 22:00 to 3:00, fading out until 7:00 and in from 17:00.
    float star_strength = 0.0f;
    if (Current_time_of_day <= 22.0f)
    {
        if (Current_time_of_day <= 17.0f)
        {
            if (Current_time_of_day < 3.0f)
            {
                star_strength = 1.0f;
            }
            else if (Current_time_of_day < 7.0f)
            {
                star_strength = (7.0f - Current_time_of_day) * 0.25f;
            }
        }
        else
        {
            star_strength = 1.0f - (22.0f - Current_time_of_day) / 5.0f;
        }
    }
    else
    {
        star_strength = 1.0f;
    }
    rl_star_field::get_instance()->set_strength(star_strength);
    rl_meteor_shower::get_instance()->set_strength(star_strength);

    // The orbitals, drawn before the clouds.
    Combine_skybox_renderable_instance_p->clear_orbitals();
    Combine_skybox_renderable_instance_p->set_render_orbitals_before(1);
    for (int i = 0; i < game_time_of_day_get_num_orbitals(); i++)
    {
        rl_skybox_orbital_params params;
        game_time_of_day_get_orbital_params(i, params);
        Combine_skybox_renderable_instance_p->add_orbital(params);
    }

    game_render_set_skybox(R_skybox ? Current_skybox_mesh_instance_p : nullptr);
    if (Current_skybox_mesh_instance_p != nullptr)
    {
        Current_skybox_mesh_instance_p->set_position(vector(sky_position[0], sky_position[1], sky_position[2]));
        Current_skybox_mesh_instance_p->set_rotation(IDENTITY_MATRIX);
        game_render_scene_update(Current_skybox_mesh_instance_p);
    }
}

// ---- functions
// 0x00A1C390 (:428)
void object_skybox_set_matte_fog_scale(float scale)
{
    R_matte_fog_scale = scale;
}

// 0x00A1C3A0 (:433)
void object_skybox_set_matte_fog_offset(float offset)
{
    R_matte_fog_offset = offset;
}

// 0x00A1C3B0 (:438)
void object_skybox_set_sky_fog_scale(float scale)
{
    R_sky_fog_scale = scale;
}

// 0x00A1C3C0 (:443)
void object_skybox_set_sky_fog_offset(float offset)
{
    R_sky_fog_offset = offset;
}

// 0x00A1C3D0 (:448)
void object_skybox_set_clouds_fog_scale(float scale)
{
    R_clouds_fog_scale = scale;
}

// 0x00A1C3E0 (:453)
void object_skybox_set_clouds_fog_offset(float offset)
{
    R_clouds_fog_offset = offset;
}

// 0x00A1C3F0 (:458)
void object_skybox_set_sky_fog_camera_enabled(bool enabled)
{
    R_sky_fog_camera = enabled;
}

// 0x00A6C520 (:470): the sky dome and cloud instances from the preloaded meshes, combined into the scene's skybox.
void object_skybox_level_init_pre()
{
    static_mesh* mesh_p = static_mesh_find(TOD_skybox_mesh_name, TOD_skybox_srid);
    if (mesh_p == nullptr)
    {
        return;
    }
    TOD_skybox_mesh = mesh_p;
    TOD_skybox_mesh_instance_p = rl_mesh_instance::create();
    if (TOD_skybox_mesh_instance_p != nullptr)
    {
        TOD_skybox_mesh_instance_p->init(TOD_skybox_mesh->m_shared_header.m_mesh.to_pointer(), TOD_skybox_mesh->m_shared_header.m_material_map.to_pointer(), TOD_skybox_mesh->m_lod_submesh_id.to_pointer()[0]);
        TOD_skybox_mesh_instance_p->calculate_local_aabb();
        TOD_skybox_mesh_instance_p->set_gpu_occluded(false); // the sky is never occluded
    }
    mesh_p = static_mesh_find(Skybox_clouds_mesh_name, Skybox_clouds_srid);
    if (mesh_p == nullptr)
    {
        return;
    }
    Skybox_clouds_mesh = mesh_p;
    Skybox_clouds_mesh_instance_p = rl_compound_mesh_instance::create();
    if (Skybox_clouds_mesh_instance_p != nullptr)
    {
        Skybox_clouds_mesh_instance_p->set_gpu_occluded(false); // the sky is never occluded
        // One active sub instance per LOD submesh, at the instance's origin. The original has room for four (a mesh
        // with more LODs overruns the array; the sky meshes have fewer).
        rl_sub_instance_info infos[4];
        for (unsigned int i = 0; i < Skybox_clouds_mesh->m_num_lods; i++)
        {
            infos[i].m_is_active = 1;
            infos[i].m_submesh_id = Skybox_clouds_mesh->m_lod_submesh_id.to_pointer()[i];
            infos[i].m_transform.make_identity();
        }
        Skybox_clouds_mesh_instance_p->init(Skybox_clouds_mesh->m_shared_header.m_mesh.to_pointer(), Skybox_clouds_mesh->m_shared_header.m_material_map.to_pointer(), infos, Skybox_clouds_mesh->m_num_lods);
        Skybox_clouds_mesh_instance_p->calculate_local_aabb();
        Skybox_clouds_mesh_instance_p->set_gpu_occluded(false); // the sky is never occluded
    }
    const unsigned int base_crc = crc_stri("Cloud_base_material", 0);
    const unsigned int overhead_crc = crc_stri("Cloud_overhead_material", 0);
    const unsigned int matte_crc = crc_stri("m_sky_matte_01", 0);
    Matte_layer_material_idx = 0xFFFFFFFF;
    Cloud_material_overhead_idx = 0xFFFFFFFF;
    Cloud_material_base_idx = 0xFFFFFFFF;
    for (unsigned int i = 0; i < Skybox_clouds_mesh->m_shared_header.m_num_materials; i++)
    {
        const unsigned int name_crc = skybox_material(Skybox_clouds_mesh, i)->get_name_checksum();
        if (name_crc == base_crc)
        {
            Cloud_material_base_idx = i;
        }
        else if (name_crc == overhead_crc)
        {
            Cloud_material_overhead_idx = i;
        }
        else if (name_crc == matte_crc)
        {
            Matte_layer_material_idx = i;
        }
    }
    Combine_skybox_renderable_instance_p = rl_skybox_renderable_instance::create();
    Combine_skybox_renderable_instance_p->add_subpart(TOD_skybox_mesh_instance_p);
    Combine_skybox_renderable_instance_p->add_subpart(Skybox_clouds_mesh_instance_p);
    Current_skybox_mesh_instance_p = Combine_skybox_renderable_instance_p;
    Combine_skybox_renderable_instance_p->set_gpu_occluded(false); // the sky is never occluded
    game_render_set_skybox(Current_skybox_mesh_instance_p);
}

// 0x00A1C400 (:556)
void object_skybox_level_shutdown()
{
    object_skybox_free_all();
}

// 0x00A6C740 (:564): preloads the sky containers, registers the update-render task and looks up the sky shaders'
// constants.
void object_skybox_system_init()
{
    if (Skybox_init)
    {
        return;
    }
    Skybox_init = true;
    object_skybox_load_shared(&Common_mempool, &Permanent_mempool, Use_packfiles, PLATFORM_PC);
    Update_render_task_skybox.m_callback = object_skybox_update_render_all;
    game_render_update_render_task_register(&Update_render_task_skybox);
    cf_add_ext(Skybox_clouds_mesh_name, sizeof(Skybox_clouds_mesh_name), "skybox_clouds", ".csmesh_pc", false);
    Skybox_clouds_srid = stream2_container_find(Skybox_clouds_mesh_name);
    cf_add_ext(TOD_skybox_mesh_name, sizeof(TOD_skybox_mesh_name), "rfg_skybox", ".csmesh_pc", false);
    TOD_skybox_srid = stream2_container_find(TOD_skybox_mesh_name);
    rl_shader_library* library_p = rl_shader_library::get_instance();
    unsigned int shader = library_p->lookup("rfg-skybox-clouds");
    if (shader == 0xFFFFFFFF)
    {
        return;
    }
    Cloud_shader_const_layer_strengths.register_const(shader, "Layer_strengths", rl_shader_const_float);
    Cloud_shader_const_fade_height.register_const(shader, "Cloud_Fade_Height", rl_shader_const_float);
    Cloud_shader_const_full_height.register_const(shader, "Cloud_Full_Height", rl_shader_const_float);
    Cloud_shader_const_u_offset.register_const(shader, "Cloud_U_Offset", rl_shader_const_float);
    Cloud_shader_const_tod_light_color_front.register_const(shader, "TOD_Light_Color_Front", rl_shader_const_float);
    Cloud_shader_const_tod_light_color_rear.register_const(shader, "TOD_Light_Color_Rear", rl_shader_const_float);
    Cloud_shader_const_tod_light_direction.register_const(shader, "TOD_Light_Dir", rl_shader_const_float);
    Cloud_shader_const_sky_position.register_const(shader, "Skybox_position", rl_shader_const_float);
    Cloud_shader_const_rimlight_scale.register_const(shader, "Rimlight_scale", rl_shader_const_float);
    Cloud_shader_const_rimlight_power.register_const(shader, "Rimlight_power", rl_shader_const_float);
    Cloud_shader_const_scale.register_const(shader, "Scale", rl_shader_const_float);
    Cloud_shader_const_offset.register_const(shader, "Offset", rl_shader_const_float);
    shader = library_p->lookup("rfg-skybox");
    if (shader == 0xFFFFFFFF)
    {
        return;
    }
    Skybox_shader_const_colors.register_const(shader, "Hemisphere_colors", rl_shader_const_float);
    Skybox_shader_const_sky_position.register_const(shader, "Skybox_position", rl_shader_const_float);
    Skybox_shader_const_scale.register_const(shader, "Scale", rl_shader_const_float);
    Skybox_shader_const_offset.register_const(shader, "Offset", rl_shader_const_float);
    shader = library_p->lookup("rfg-skybox-matte");
    if (shader == 0xFFFFFFFF)
    {
        return;
    }
    Matte_shader_const_mountain_color_front.register_const(shader, "Mountain_color_front", rl_shader_const_float);
    Matte_shader_const_mountain_color_back.register_const(shader, "Mountain_color_back", rl_shader_const_float);
    Matte_shader_const_mountain_fog_color.register_const(shader, "Mountain_fog_color", rl_shader_const_float);
    Matte_shader_const_mountain_fog_density.register_const(shader, "Mountain_fog_density", rl_shader_const_float);
    Matte_shader_const_light_direction.register_const(shader, "TOD_Light_Dir", rl_shader_const_float);
    Matte_shader_const_scale.register_const(shader, "Scale", rl_shader_const_float);
    Matte_shader_const_offset.register_const(shader, "Offset", rl_shader_const_float);
}

// 0x00A1C410 (:646)
void object_skybox_system_shutdown()
{
    if (Skybox_init)
    {
        Skybox_init = false;
        stream2_release(Skybox_clouds_srid);
        stream2_release(TOD_skybox_srid);
    }
}
