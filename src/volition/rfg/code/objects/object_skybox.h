// Original: volition/rfg/code/objects/object_skybox.h
// Implementation: volition/rfg/code/objects/object_skybox.cpp
//
// The sky box object: creates the rl_skybox_renderable_instance, star field and meteor shower and updates them with the
// time of day.
#pragma once

void object_skybox_set_matte_fog_scale(float scale); // 0x00A1C390 (:428)
void object_skybox_set_matte_fog_offset(float offset); // 0x00A1C3A0 (:433)
void object_skybox_set_sky_fog_scale(float scale); // 0x00A1C3B0 (:438)
void object_skybox_set_sky_fog_offset(float offset); // 0x00A1C3C0 (:443)
void object_skybox_set_clouds_fog_scale(float scale); // 0x00A1C3D0 (:448)
void object_skybox_set_clouds_fog_offset(float offset); // 0x00A1C3E0 (:453)
void object_skybox_set_sky_fog_camera_enabled(bool enabled); // 0x00A1C3F0 (:458)
void object_skybox_level_init_pre(); // 0x00A6C520 (:470)
void object_skybox_level_shutdown(); // 0x00A1C400 (:556)
void object_skybox_system_init(); // 0x00A6C740 (:564)
void object_skybox_system_shutdown(); // 0x00A1C410 (:646)
