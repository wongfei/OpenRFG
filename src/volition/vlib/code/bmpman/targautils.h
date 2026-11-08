// Original: volition/vlib/code/bmpman/targautils.h
// Implementation: volition/vlib/code/bmpman/targautils.cpp
//
// Targa (TGA) reading and RLE compression.
#pragma once

#include <stdint.h>

struct cfile;
class mempool_base;

char* get_next_pixel(char* base, char* cur, int width, int pitch, int pixsize); // 0x005642B0 (:206)
int64_t targa_compress(char* out, char* in, int64_t outsize, int pixsize, int64_t bytecount, int width, int pitch); // 0x00564320 (:229)
int64_t targa_compress32(char* out, char* in, int bytecount, int width, int pitch); // 0x00564630 (:363)
int targa_read_header_internal(cfile* fp, int* w, int* h, int* bpp); // 0x005C7980 (:546)
int targa_read_bitmap_internal(cfile* fp, unsigned char* image_data, unsigned char* palette, mempool_base* temp_pool); // 0x005C7B00 (:579)
int targa_read_header(const char* filename, int* w, int* h, int* bpp, const char** ident); // 0x005CF070 (:727)
int targa_read_header_from_memory(unsigned char* src, int size, int* w, int* h, int* bpp, const char** ident); // 0x005CF190 (:770)
int64_t targa_uncompress(unsigned char* dst, unsigned char* src, int srclen, int bitmap_length, int bytes_per_pixel); // 0x00564890 (:801)
int targa_read_bitmap(const char* filename, unsigned char* image_data, unsigned char* palette, mempool_base* temp_pool); // 0x005CF1F0 (:859)
int targa_read_bitmap_from_memory(unsigned char* src, int size, unsigned char* image_data, unsigned char* palette, mempool_base* temp_pool); // 0x005CF2B0 (:883)
int targa_write_get_max_space_needed(int w, int h, int bpp); // 0x00564940 (:901)
int64_t targa_write_bitmap_internal(cfile* fp, unsigned char* data, int w, int h, int bpp, int pitch, mempool_base* temp_pool, char* ident); // 0x005978F0 (:909)
int targa_write_bitmap(const char* filename, unsigned char* data, int w, int h, int bpp, mempool_base* temp_pool, int source_pitch, char* ident); // 0x005CF300 (:1001)
int64_t targa_write_bitmap_to_memory(unsigned char* dest, int size, unsigned char* data, int w, int h, int bpp, mempool_base* temp_pool, int source_pitch, char* ident); // 0x005CF3F0 (:1027)
int targa_write_bitmap_uncompressed(const char* filename, unsigned char* data, int w, int h, int bpp, int source_pitch, char* ident); // 0x005CF470 (:1058)
