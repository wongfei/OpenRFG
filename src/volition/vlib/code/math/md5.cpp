// Original: volition/vlib/code/math/md5.cpp
//
// The digest at the end of a save file. Not standard MD5, and it must stay exactly this for a save to be accepted by
// rfg.exe and the other way round. Original bugs kept:
// - The working values a, b, c, d are not reloaded from the digest for each block: they run on from block to block, and
//   the digest (starting at zero) only sums them.
// - The fourth round's function is (b ^ c) | ~d; MD5's is c ^ (b | ~d).
// - Only the low 32 bits of the bit count are stored, in the padding block's last four bytes, and the tail may overlap
//   them: when 60 or more bytes are left over they are ORed in, and there is no extra block (the padding is 0x80 at the end
//   of the tail, with the block count from the size plus that one byte).

#include "volition/vlib/code/math/md5.h"

#include <string.h>

// 0x01518F88
static const unsigned int shift_amounts[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

// 0x01519088
static const unsigned int index_piece[64] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 1, 6, 11, 0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12,
    5, 8, 11, 14, 1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2, 0, 7, 14, 5, 12, 3, 10, 1, 8, 15, 6, 13, 4, 11, 2, 9,
};

// 0x01519188
static const unsigned int ac_values[64] = {
    0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
    0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
    0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
    0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
    0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
    0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
    0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
    0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391,
};

// 0x01519288
static const unsigned int initial_state[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};

// 0x005782B0 (.h:14)
md5_value::md5_value()
{
    val0 = 0;
    val1 = 0;
    val2 = 0;
    val3 = 0;
}

// 0x007C9340 (.h:15)
bool md5_value::operator!=(const md5_value& other) const
{
    return memcmp(this, &other, sizeof(md5_value)) != 0;
}

bool md5_value::operator==(const md5_value& other) const
{
    return memcmp(this, &other, sizeof(md5_value)) == 0;
}

// 0x00578300 (:89)
md5_value console_md5_mem(const unsigned char* const buffer, unsigned int size)
{
    // Magic number: the 64 byte blocks.
    unsigned char last_block[64] = {};
    const unsigned int with_end_mark = size + 1;
    const unsigned int remainder = with_end_mark & 0x3F;
    const unsigned int padding = remainder == 0 ? 0 : 0x40 - remainder;
    const unsigned int tail = 0x3F - padding;
    if (tail != 0)
    {
        memcpy(last_block, buffer + with_end_mark - 1 - tail, tail);
    }
    last_block[tail] = 0x80;
    unsigned int bit_count;
    memcpy(&bit_count, last_block + 60, sizeof(bit_count));
    bit_count |= size * 8;
    memcpy(last_block + 60, &bit_count, sizeof(bit_count));
    const unsigned int num_blocks = (padding + with_end_mark) >> 6;

    unsigned int a = initial_state[0];
    unsigned int b = initial_state[1];
    unsigned int c = initial_state[2];
    unsigned int d = initial_state[3];
    md5_value digest;
    const unsigned char* block = buffer;
    for (unsigned int block_index = 0; block_index < num_blocks; block_index++)
    {
        if (block_index == num_blocks - 1)
        {
            block = last_block;
        }
        for (unsigned int step = 0; step < 64; step++)
        {
            unsigned int function;
            if (step < 16)
            {
                function = ((c ^ d) & b) ^ d;
            }
            else if (step < 32)
            {
                function = ((b ^ c) & d) ^ c;
            }
            else if (step < 48)
            {
                function = b ^ c ^ d;
            }
            else
            {
                function = (b ^ c) | ~d; // original bug, see the file comment
            }
            unsigned int word;
            memcpy(&word, block + index_piece[step] * 4, sizeof(word));
            const unsigned int sum = word + ac_values[step] + function + a;
            const unsigned int shift = shift_amounts[step];
            const unsigned int new_b = b + ((sum << shift) | (sum >> (32 - shift)));
            a = d;
            d = c;
            c = b;
            b = new_b;
        }
        digest.val0 += a;
        digest.val1 += b;
        digest.val2 += c;
        digest.val3 += d;
        block += 0x40;
    }
    return digest;
}
