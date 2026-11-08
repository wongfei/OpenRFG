// Original: volition/rfg/code/geom/static_mesh.h
// Implementation: volition/rfg/code/geom/static_mesh.cpp
//
// Static mesh streaming: static_mesh_stream_load is the GSP_STATIC_MESH load callback; meshes are kept by
// (file name, srid) (static_mesh_find). The UI's static_mesh_info (ui_character.cpp) is deferred with the UI.
#pragma once

#include "volition/rfg/code/geom/static_mesh_shared.h"

bool static_mesh_stream_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x0080B620 (:61)
void static_mesh_stream_unload(const char* filename, unsigned int srid); // 0x00809340 (:82)
void static_mesh_system_init(); // 0x007F4880 (:98)
void static_mesh_system_shutdown(); // 0x007F48A0 (:107)
static_mesh* static_mesh_find(const char* filename, unsigned int srid); // 0x008093E0 (:116)
void static_mesh_level_shutdown(); // 0x007F48C0 (:127)
