// Original: volition/vlib/code/bmpman/bmpman.h
// Implementation: volition/vlib/code/bmpman/bmpman.cpp
//
// Bitmap manager: textures are addressed by handle (index into the bitmap_entry table). bm_init loads the startup peg
// and sets the texture register/unregister callbacks of the renderer; pegs are registered when loaded (bm_load_peg,
// from peg_load_wrapper for streamed GSP_PEG primitives) and their entries become available by file name (bm_load).
#pragma once

#include "volition/vlib/code/bmpman/bmpman_defs.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"
#include "volition/vlib/code/util/hash_table.h"

struct cfile;
class mempool_base;

extern const char* Peg_platform_ext[8]; // 0x01518940
extern const char* Peg_gpu_platform_ext[8]; // 0x01518960
extern const char* Vbm_platform_ext[8]; // 0x01518980
extern const char* Vbm_gpu_platform_ext[8]; // 0x015189A0
extern const char* Bitmaps_filename[8]; // 0x015189C0
extern bitmap_entry* Bm_bitmaps; // 0x01CD294C
extern int Bm_bitmap_count; // 0x01CDAE4C
extern int Bm_entry_count; // 0x01CDD43C
extern str_data_hash_table<peg_file,2048,64,unsigned short> Bm_pegs; // 0x01D0E8C8
extern int Last_peg_approx_cpu_data_size; // 0x01CE863C
extern int Last_peg_approx_gpu_data_size; // 0x01CE8640

void bm_reserve_bitmaps_hack(int reserve_count, mempool_base* pool); // 0x005C9A30 (:404)
int bm_add_bitmap(const char* filename); // 0x005C9C00 (:441)
void bm_init(const char* startup_peg, const char* static_bitmap_name, mempool_base* pool, void (*texture_register_callback)(bitmap_entry*), void (*texture_unregister_callback)(bitmap_entry*), void (*bm_discovery_callback)(bitmap_entry*), void (*post_bitmaps_file_load_callback)(mempool_base*)); // 0x005D05D0 (:1176)
void bm_shutdown(); // 0x00563C90 (:1238)
bool bm_load_peg(const char* filename, mempool_base* pool, bool register_with_bmpman); // 0x005CFE50 (:1259)
bool bm_load_peg(const char* filename, mempool_base* pool, peg_header*& peg, bool register_with_bmpman); // 0x005CE030 (:1265)
bool bm_load_peg(const char* filename, unsigned char* const preloaded, bool register_with_bmpman); // 0x005CE190 (:1312)
bool bm_load_peg(const char* filename, unsigned char* const peg_data, const unsigned int peg_size, unsigned char* const vram_data, const unsigned int vram_size, bool register_with_bmpman); // 0x005C9D80 (:1321)
peg_header* bm_load_peg_header(const char* filename, mempool_base* pool); // 0x005CE230 (:1362)
void bm_unload_peg(const char* filename); // 0x005C75C0 (:1428)
int bm_load_peg_multiframe_begin(const char* filename, unsigned char* const preloaded); // 0x005C1F80 (:1452)
void bm_load_peg_multiframe_start_gpu_file(const char* const filename, unsigned char* gpu_data_ptr); // 0x005B5350 (:1502)
bool bm_load_peg_multiframe_pixels_are_being_read(const char* const filename); // 0x005B5380 (:1513)
int bm_load_peg_multiframe_process(const char* const filename, const int bytes_read_from_cur_file); // 0x005C9E20 (:1521)
int bm_load_peg_multiframe_end(const char* filename, int total_size); // 0x005CA0C0 (:1681)
void bm_unload_peg_multiframe(const char* filename); // 0x005C7620 (:1710)
int bm_save_peg(const char* filename, peg_header* peg); // 0x005CFE70 (:1739)
int bm_save_peg(const char* cpu_filename, const char* gpu_filename, peg_header* peg); // 0x005CE440 (:1747)
int bm_save_peg(cfile* peg_cpu_file, cfile* peg_gpu_file, peg_header* peg); // 0x005CA100 (:1780)
int bm_load(const char* filename, bool force_dynamic); // 0x005CA530 (:1875)
bool bm_is_loaded(const char* filename); // 0x005C1FE0 (:1906)
void bm_unload(int handle, bool force_unload); // 0x00563CA0 (:1924)
int bm_get_peg_align_value(); // 0x00563CB0 (:1975)
int bm_get_peg_align_value(vlib_platform platform); // 0x00563CC0 (:1980)
const char* bm_get_filename(int handle); // 0x00597570 (:2028)
void bm_get_dimensions(int handle, int& w, int& h, float& scale_x, float& scale_y); // 0x00597590 (:2041)
void bm_get_dimensions(int handle, int& w, int& h, float& scale_x, float& scale_y, int& mip_levels); // 0x00597690 (:2058)
void bm_get_frame_data(int handle, unsigned char*& data, unsigned char*& pal, int auto_animate); // 0x005B53A0 (:2077)
void bm_copy_frame_data(int src_handle, int dest_handle); // 0x00563CD0 (:2130)
bitmap_entry* bm_get_entry(int handle, bool auto_animate); // 0x00563CE0 (:2161)
bool bm_has_alpha(int handle); // 0x00563D70 (:2198)
bool bm_has_alpha(const char* filename); // 0x005CFF20 (:2210)
bool bm_is_cube_map(int handle); // 0x00563D90 (:2242)
bool bm_is_bogus(int handle); // 0x00563DC0 (:2258)
int bm_get_bogus_bitmap_handle(); // 0x00563DF0 (:2270)
bool bm_get_gpu_filename(const char* base_filename, char* gpu_filename, int max_len); // 0x005977A0 (:2276)
