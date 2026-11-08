// Original: volition/vlib/code/util/bit_stream.h
// Implementation: volition/vlib/code/util/bit_stream.cpp
//
// Bit streams (bit_stream, bit_stream_reader, bit_stream_writer) for packed network and save data.
#pragma once


struct bit_stream;
class mempool_base;

// size 0x18
class bit_stream_writer
{
public:
    bit_stream_writer(bit_stream*, unsigned int);
    bit_stream_writer(bit_stream*, mempool_base*);
    bit_stream_writer();
    void init(bit_stream* data, mempool_base* pool, unsigned int max_stream_size); // 0x0058ED60 (:25)
    void set_mark(); // 0x0058ED90 (:38)
    void return_to_mark(); // 0x0058EDA0 (:47)
    unsigned int get_current_bit(); // 0x0058EDF0 (:161)
    void restore_to_bit(unsigned int bit); // 0x0058EE00 (:166)
    void write(unsigned int value, unsigned int num_bits); // 0x005B1500 (:60)
    void write_float(float);
    void write_buffer(const unsigned char* buffer, unsigned int num_bits); // 0x005B1600 (:122)
    bool can_write(unsigned int num_bits); // 0x0058EDB0 (:148)

private:
    bit_stream* m_data; // 0x00
    unsigned int m_current_bit; // 0x04
    unsigned int m_marked_bit; // 0x08
    unsigned int m_marked_size; // 0x0C
    unsigned int m_max_stream_size; // 0x10
    mempool_base* m_pool; // 0x14
};

// size 0x8
class bit_stream_reader
{
public:
    bit_stream_reader(const bit_stream*);
    unsigned int read(unsigned int num_bits); // 0x005B1690 (:185)
    float read_float();
    void read_buffer(unsigned char* buffer, unsigned int num_bits); // 0x005B1710 (:222)
    void set_current_bit(unsigned int);
    unsigned int get_current_bit();

private:
    const bit_stream* m_data; // 0x00
    unsigned int m_current_bit; // 0x04
};

// size 0x8
struct bit_stream
{
    unsigned int stream_size; // 0x00
    unsigned char* bits; // 0x04
};
