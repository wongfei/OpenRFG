// Original: volition/rfg/code/geom/material_shared.h
// Implementation: volition/rfg/code/geom/material_shared.cpp
//
// Material loading shared with the tools.
#pragma once


struct material;

material* material_load(unsigned char* data, int size); // 0x008085D0 (:80)
void material_unload(material* smp); // 0x007FBF40 (:97)
