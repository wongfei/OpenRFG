// Original: volition/vlib/code/util/bitarray.h
// Implementation: volition/vlib/code/util/bitarray.cpp
//
// Bit set over a byte buffer, either supplied (init(mem, size)) or allocated from a mempool (is_allocated).
#pragma once

class mempool_base;

// size 0xC
class bit_array
{
public:
    bit_array(); // 0x0058EB50 (:19)
    ~bit_array(); // 0x0058EB70 (:28)
    void init(mempool_base* pool, int _size); // 0x0058EBB0 (:58)
    void init(unsigned char* _mem, int _size); // 0x0058EB90 (:48)
    void clear(bool as_ones); // 0x0058EBE0 (:85)
    int get(int index) const; // 0x0058EC10 (:97)
    void set(int index, int value); // 0x0058EC40 (:111)
    void set_span(int first_index, int num_bits, int value); // 0x0058EC70 (:130)
    unsigned char* get_buffer();

private:
    unsigned char* mem; // 0x00
    int size; // 0x04
    bool is_allocated; // 0x08
};
