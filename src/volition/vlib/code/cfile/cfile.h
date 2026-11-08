// Original: volition/vlib/code/cfile/cfile.h
// Implementation: volition/vlib/code/cfile/cfile.cpp
//
// Volition file API (cf_*). Every file is a `cfile*` (cfile_internal.h) from a fixed pool of 256.
//
// Opening (cf_open -> cf_open_internal, decompiled):
// - mode is two characters, 'r'/'w'/'a' then 't'/'b'; an 'e' anywhere marks the file as encrypted (packfile lookups).
// - Read opens walk the registered search order (cf_add_search; use_first puts a system in front):
//   CF_SEARCH_STANDARD asks the keen file system (g_pFileSystem, keen_cfile.h) on CF_IO_MEDIA_HDD,
//   CF_SEARCH_PACKFILE asks packfile_find with the media flag of the entry's media (HDD 0x8, DVD 0x10, HOST 0x4,
//   MEMORY 0x20), CF_SEARCH_VDIR asks the loose-file directory (vdir.h). The first hit wins.
// - Write and append opens always go to the standard system (cf_get_os_full_name).
// - A packfile hit on CF_IO_MEDIA_MEMORY becomes a cf_memory_handle over v_packfile::data; other packfile hits
//   open the entry through the packfile's keen file (keen_cf_open_packfile_file).
// - cf_open_lock only records a flag for cf_is_locked; the open paths of this build do not check it.
// Reading: cf_read clamps to the entry size and sets CF_ERROR_READ on a short read; compressed packfile entries can
// only be read whole (cf_read_compressed / cf_read_compressed_chain inflate them with one of the two Cfile_decompress
// streams, cf_alloc_decompressor). Entries of condensed packfiles (flag 0x2) cannot be read through cfile at all.
// The typed readers/writers (cf_read_int32, cf_write_vector, ...) swap bytes when cfile::m_flags has 0x4.
// cfile_system_init: resets the pool, vdir_system_init + vdir_restore(PLATFORM_PC) when a vdir_config is given,
// packfile_init when a packfile_config is given.
#pragma once

#include <stdint.h>
#include "volition/vlib/code/cfile/cfile_common.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

struct cfile;
struct vdir_config;
struct packfile_config;
class mempool_base;
class zlib_stream;
struct bbox;
class vector;
class quaternion;
class matrix;

void cf_set_downloadable_content_base_name(const char* name); // 0x00564AC0 (:526)
bool cf_find_file(cf_found_info* found_info, const char* filename, bool encrypted); // 0x005C21E0 (:547)
bool cf_find_file_io_standard(cf_found_info* found_info, const char* filename, cf_io_media_types media_type, bool encrypted); // 0x00597A30 (:625)
bool cfile_system_init(vdir_config* vd_config, packfile_config* pf_config, bool); // 0x005CF560 (:955)
void cfile_system_shutdown(); // 0x005C27E0 (:1026)
void cf_abort_async_read(cfile* file); // 0x00597AF0 (:1035)
void cf_reset_async_abort(cfile* file); // 0x00597B40 (:1074)
bool cf_add_search(cf_search_types search_type, cf_io_media_types media_type, bool use_first); // 0x00597B90 (:1108)
void cf_remove_search(cf_search_types search_type, cf_io_media_types media_type); // 0x00597CA0 (:1164)
cfile* cf_open(const char* filename_in, const char* mode, vlib_platform disk_platform, bool async); // 0x005C27F0 (:1204)
cfile* cf_open(unsigned char* buf, unsigned int buf_size, const char* open_mode, vlib_platform disk_platform); // 0x005B5CC0 (:1229)
cfile* cf_open_buffered(const char* filename, mempool_base* pool, const char* open_mode, vlib_platform disk_platform); // 0x005CF6A0 (:1310)
bool cf_close(cfile* cfp); // 0x005CA590 (:1342)
bool cf_exist(const char* filename_in); // 0x005C2810 (:1398)
unsigned int cf_size(const cfile* p_cfile); // 0x00564B20 (:1410)
bool cf_get_file_size(unsigned int* filesize, const char* filename, bool encrypted); // 0x005C2850 (:1436)
unsigned int cf_read(void* dest_buf, unsigned int num_bytes_to_read, cfile* p_cfile, bool abortable); // 0x005C28D0 (:1471)
unsigned int cf_read_compressed(void* dest_buf, unsigned int num_compressed_bytes_to_read, cfile* p_cfile, unsigned int expected_num_uncompressed_bytes_read); // 0x005C7EB0 (:1598)
unsigned int cf_read_compressed_chain(unsigned char** chain_dest_buf, unsigned int total_compressed_bytes_to_read, unsigned int* chain_num_bytes, unsigned int chain_num_reads, cfile* p_cfile); // 0x005C2A30 (:1604)
unsigned int cf_query_num_async_bytes_read(cfile* p_cfile); // 0x00564B30 (:1714)
unsigned int cf_write(const void* source_buf, unsigned int num_bytes, cfile* p_cfile); // 0x00564B50 (:1752)
unsigned int cf_write(cfile* p_source_file, cfile* p_cfile); // 0x00564BD0 (:1805)
bool cf_pad_align_read(unsigned int align, cfile* p_cfile); // 0x005C2BC0 (:1829)
unsigned int cf_pad_align_write(unsigned int align, cfile* p_cfile); // 0x00597D30 (:1845)
bool cf_seek(cfile* p_cfile, int relative_offset, cf_seek_mode seek_mode); // 0x005B5E10 (:1873)
unsigned int cf_tell(const cfile* p_cfile); // 0x00564C30 (:1969)
// orig_main.md OM5: file statics of cfile.cpp that vconsole.cpp (the same unity build in rfg.exe) uses.
extern bool Cfile_vdir_enabled; // 0x01CE8671
extern bool Cf_open_locked; // 0x01CE8672
int cf_sync(const cfile* p_cfile); // 0x00597E00 (:1981)
void cf_wait_async(const cfile* p_cfile); // 0x00597E40 (:2013)
bool cf_eof(const cfile* p_cfile); // 0x00564C40 (:2054)
void* cf_memory_fp(const cfile* p_cfile); // 0x00564C60 (:2062)
void* cf_cur_memory_fp(const cfile* p_cfile); // 0x00564C90 (:2077)
cf_error_codes cf_error(const cfile* p_cfile); // 0x00564CD0 (:2098)
void cf_clear_error(cfile* p_cfile); // 0x00564CE0 (:2109)
char cf_read_char(cfile* p_cfile); // 0x005C2C00 (:2119)
int8_t cf_read_int8(cfile* p_cfile); // 0x005C2C20 (:2120)
unsigned char cf_read_uint8(cfile* p_cfile); // 0x005C2C40 (:2121)
short cf_read_int16(cfile* p_cfile); // 0x005C2C60 (:2122)
unsigned short cf_read_uint16(cfile* p_cfile); // 0x005C2CA0 (:2123)
int cf_read_int32(cfile* p_cfile); // 0x005C2CE0 (:2124)
unsigned int cf_read_uint32(cfile* p_cfile); // 0x005C2D30 (:2125)
float cf_read_float(cfile* p_cfile); // 0x005C2D80 (:2126)
bool cf_read_bool(cfile* p_cfile); // 0x005C2E10 (:2133)
void cf_read_bbox(bbox& bb, cfile* p_cfile); // 0x005C7ED0 (:2139)
void cf_read_vector(vector& v, cfile* p_cfile); // 0x005C2E30 (:2150)
void cf_read_quaternion(quaternion& quat, cfile* p_cfile); // 0x005C2E60 (:2162)
void cf_read_matrix(matrix& mat, cfile* p_file); // 0x005C2EA0 (:2175)
bool cf_read_line(char* dest_buf, unsigned int dest_buf_size, cfile* p_cfile); // 0x005C2F00 (:2184)
bool cf_read_string(char* dest_buf, unsigned int dest_buf_size, cfile* p_cfile); // 0x005C2FA0 (:2239)
unsigned int cf_read_chksum_string(char* dest_buf, unsigned int dest_buf_size, unsigned int& chksum, cfile* p_cfile); // 0x005C3050 (:2277)
bool cf_read_string_null_term(char* dest_buf, unsigned int dest_buf_size, cfile* p_cfile); // 0x005C3110 (:2333)
unsigned int cf_read_chksum_string_null_term(char* dest_buf, unsigned int dest_buf_size, unsigned int& chksum, cfile* p_cfile); // 0x005C31C0 (:2381)
bool cf_write_char(char c, cfile* p_cfile); // 0x00564CF0 (:2427)
bool cf_write_int8(int8_t v, cfile* p_cfile); // 0x00564D80 (:2428)
bool cf_write_uint8(unsigned char v, cfile* p_cfile); // 0x00564E10 (:2429)
bool cf_write_int16(short v, cfile* p_cfile); // 0x00564EA0 (:2430)
bool cf_write_uint16(unsigned short v, cfile* p_cfile); // 0x00564F50 (:2431)
bool cf_write_int32(int v, cfile* p_cfile); // 0x00564FF0 (:2432)
bool cf_write_uint32(unsigned int v, cfile* p_cfile); // 0x005650B0 (:2433)
bool cf_write_float(float v, cfile* p_cfile); // 0x00565170 (:2434)
bool cf_write_bool(bool v, cfile* p_cfile); // 0x00565270 (:2439)
bool cf_write_vector(const vector& v, cfile* p_cfile); // 0x00565300 (:2446)
bool cf_write_matrix(const matrix& m, cfile* p_cfile); // 0x00565360 (:2450)
bool cf_write_quaternion(const quaternion& q, cfile* p_cfile); // 0x00565420 (:2454)
bool cf_write_bbox(const bbox& b, cfile* p_cfile); // 0x005654A0 (:2459)
bool cf_write_string(const char* str, cfile* p_cfile); // 0x00565550 (:2472)
bool cf_write_string_null_term(const char* str, cfile* p_cfile); // 0x00565600 (:2503)
bool cf_write_text(const char* text, cfile* p_cfile); // 0x00565640 (:2525)
bool cf_printf(cfile*, const char*, ...); // 0x00565680 (:2541)
unsigned int cf_find_ext(const char** files, unsigned int max_files, const char* ext); // 0x005C3280 (:2566)
unsigned int cf_find_ext_path(const char** files, unsigned int max_files, const char* ext, const char* path); // 0x005B5FA0 (:2632)
unsigned int cf_find_pattern(const char** files, unsigned int max_files, const char* pattern); // 0x005B61F0 (:2700)
const char* cf_filename(const cfile* p_cfile); // 0x00565720 (:2770)
const char* cf_full_pathname(const cfile* p_cfile); // 0x00565730 (:2782)
cf_io_media_types cf_media_type(const cfile* p_cfile); // 0x00565740 (:2794)
void cf_drop_path(char* new_filename, unsigned int new_filename_array_size, const char* filename); // 0x00565750 (:2805)
bool cf_subtract_path(char* dst, unsigned int dest_len, const char* full_path, const char* base_path); // 0x00597E70 (:2864)
bool cf_get_path(char* dest_path, unsigned int dest_path_size, const char* filename); // 0x005657B0 (:2946)
bool cf_get_os_full_name(char* fullname, unsigned int fullname_array_size, const char* filename, cf_io_media_types media_type); // 0x00565800 (:2983)
void cf_drop_ext(char* new_filename, unsigned int new_filename_array_size, const char* filename); // 0x00565830 (:3007)
bool cf_add_ext(char* new_filename, unsigned int new_filename_array_size, const char* filename, const char* ext, bool dont_replace); // 0x00598010 (:3052)
const char* cf_get_ext(const char* filename); // 0x00565890 (:3097)
bool cf_add_suffix(char* new_filename, unsigned int new_filename_array_size, const char* filename, const char* suffix); // 0x005658D0 (:3121)
unsigned int cf_refresh_file_size(const char* filename); // 0x005C34C0 (:3158)
void cf_refresh_file_size_ext(const char* ext); // 0x005C34E0 (:3168)
void cf_refresh_all_sizes(); // 0x005C34F0 (:3178)
bool cf_copy_file(const char* dst_full_path, const char* src_filename); // 0x005CF750 (:3188)
bool cf_copy_file(cfile* in, cfile* out); // 0x005CA6C0 (:3201)
unsigned int cf_decompress_chain(unsigned int compressed_bytes_read, unsigned char* compressed_buf, zlib_stream* decompressor, unsigned char** dest_buf, unsigned int* num_bytes_to_read, unsigned char*& dest_buf_pos, unsigned int& dest_buf_index, unsigned int& dest_buf_read_remaining, unsigned char* buffer_space, unsigned int buffer_size); // 0x005980D0 (:3247)
zlib_stream* cf_alloc_decompressor(); // 0x005981D0 (:3330)
void cf_release_decompressor(zlib_stream* decom_to_free); // 0x00598220 (:3354)
bool cf_running_on_disc(); // 0x00565A10 (:3367)
void cf_open_lock(bool lock); // 0x00565A20 (:3376)
bool cf_is_locked(); // 0x00565A30 (:3381)
unsigned int cf_get_async_align(); // 0x00565A40 (:3387)
