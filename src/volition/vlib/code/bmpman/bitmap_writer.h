// Original: volition/vlib/code/bmpman/bitmap_writer.h
// Implementation: volition/vlib/code/bmpman/bitmap_writer.cpp
//
// bitmap_writer, writes bitmaps (TGA/peg data) to a file, whole or in parts.
#pragma once


struct cfile;

// size 0x120
class bitmap_writer
{
public:
    bitmap_writer(); // 0x00563AB0 (:46)
    ~bitmap_writer(); // 0x005CFE20 (:59)
    bool open(const char* destination_file, int w, int h, bool make_targa, bool write_to_hd); // 0x005C1B50 (:78)
    void write_partial(int x, int y, int widthv, unsigned char* data); // 0x005C1D80 (:212)
    bool write(); // 0x005CDBE0 (:236)
    void close(); // 0x005CDBC0 (:68)
    bool is_open(); // 0x00563AE0 (:259)

private:
    cfile* mem_file; // 0x00
    char filename[260]; // 0x04
    int width; // 0x108
    int height; // 0x10C
    int row_size; // 0x110
    int header_size; // 0x114
    int file_size; // 0x118
    bool is_targa; // 0x11C
};

void endian_swap_2(void* dest, const void* src); // 0x00563A60 (:21)
void endian_swap_4(void* dest, const void* src); // 0x00563A80 (:29)
