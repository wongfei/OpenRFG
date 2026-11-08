// New code (not in rfg.exe): load-time fixes of shader data bugs (bug_blood_decals.md, the sky dome apex NaN).
// Implementation: volition/render_lib/code/rl_shader_fixup.cpp
//
// Deviation: the decal vertex shaders rfg-decal-bloodsplat_c, _mc, _ms and _t read the vertex normal (NORMAL, every
// vertex format stores it as x8y8z8w8_unorm, rl_vertex_formats.cpp) as is, without the [0, 1] to [-1, 1] decode
// (mad n, n, 2, -1) that every body shader and the bloodsplat _s / _ts variants do. The decal's angle fade
// (Test_decal_blood slope fade 60..85 degrees) then compares the decal axis with normalize(n + (1, 1, 1)), and decals on
// bind-pose surfaces facing -x, -y or -z get alpha 0: blood marks on players show up only for some hit directions. The
// shader's lighting frame is built from the same raw value.
//
// The fix rewrites the DXBC blob before it is created: dcl_temps N becomes N + 1, mad rN.xyz, vNormal.xyzx, l(2), l(-1)
// is inserted before the first instruction, every read of vNormal becomes a read of rN, and the container sizes and
// checksum are rebuilt. The input signature is untouched, so the input layouts and their cache are the same.
//
// Deviation: the sky dome vertex shader rfg-skybox_s picks the hemisphere color row from the height above the dome
// centre, y = clamp(pos.y - Skybox_position.y, 0, 10) (the dome mesh rfg_skybox is a radius 10 hemisphere, its apex
// vertex at y = 10), as asin(y * 0.1) * 8 / pi + 0.4 * y. The compiled asin takes sqrt(mad(-y, 0.1, 1)). 0.1f is a bit
// above 0.1, so at y = 10 the exact product is 1.0000000149: a GPU that fuses the mad (FMA) gets -1.49e-8 and sqrt
// gives NaN at the apex vertex. Every triangle of the cap fan around it interpolates the NaN color, and looking straight
// up shows a black circle (NaN pixels). Whether the apex reaches the clamp depends on the float rounding of the camera
// render position, as in rfg.exe, which has the same shader.
//
// The fix changes the clamp's 10.0 immediate in the one min r.xy, r.xyxx, l(1.0, 10.0, 0.0, 0.0) to 9.99999905 (the
// float below 10): sqrt then sees at least 5.96e-8 fused or not, and the apex row is 3.9995 instead of 4 (the top
// color to 0.05%). Same blob size, new checksum.
#pragma once

// A patched copy of a vertex shader blob (malloc, the caller frees it) and its size, or nullptr when the shader is not
// one of the fixed ones, already decodes its normal, or the blob is not what the patch expects (the original blob is
// then used as is). shader_fname is the .fxo_kg file name; *num_reads_p receives the number of rewritten normal reads
// (0 for the sky dome fix) and *fix_p a short name of the fix for the log.
unsigned char* rl_shader_fixup_vertex_shader(const char* shader_fname, const unsigned char* blob_p, unsigned int blob_size, unsigned int* patched_size_p, unsigned int* num_reads_p, const char** fix_p);

// The DXBC container checksum (bytes 4-19) of a blob: MD5 over the bytes from offset 20 with the container's own
// padding (DirectXShaderCompiler ComputeHashRetail).
void rl_shader_fixup_dxbc_checksum(const unsigned char* blob_p, unsigned int blob_size, unsigned int checksum[4]);
