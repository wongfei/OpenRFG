// Original: volition/rfg/code/geom/foliage_mesh_shared.h
// Implementation: volition/rfg/code/geom/foliage_mesh_shared.cpp
//
// Foliage mesh loading shared with the tools: the streamed mesh with its render data.
#pragma once


struct foliage_mesh;

foliage_mesh* foliage_mesh_load(const char* filename, unsigned char* cpu_data, int cpu_size, unsigned char* gpu_data, int gpu_size); // 0x008083E0 (:121)
void foliage_mesh_unload(foliage_mesh* fmp); // 0x007FBEC0 (:145)
