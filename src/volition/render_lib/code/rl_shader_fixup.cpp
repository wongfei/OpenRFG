// New code (not in rfg.exe): load-time fixes of shader data bugs. See rl_shader_fixup.h.

#include "volition/render_lib/code/rl_shader_fixup.h"

#include <stdlib.h>
#include <string.h>

namespace
{
    // The shaders the fix applies to (file name prefixes). The scan of every misc.vpp_pc vertex shader
    // (bug_blood_decals.md) also finds raw normal reads in rfg-decal-glasscrack_*, bw-test_decal_*, dust_skirting_t,
    // rfg-eosglass, rfg-vfx-light-cone_s and rl_terrain_landmark_lod_t; those are left as they are (not checked).
    const char* const Fixed_shaders[] = {"rfg-decal-bloodsplat_"};

    // The sky dome shader (rl_shader_fixup.h, the second fix).
    const char Skybox_dome_shader[] = "rfg-skybox_s.";

    // D3D10/11 tokenized program format values (d3d11TokenizedProgramFormat.hpp).
    const unsigned int Opcode_mad = 50;
    const unsigned int Opcode_min = 51;
    const unsigned int Opcode_customdata = 53;
    const unsigned int Opcode_dcl_temps = 104;
    const unsigned int Operand_temp = 0;
    const unsigned int Operand_input = 1;
    const unsigned int Operand_immediate32 = 4;
    const unsigned int Operand_immediate64 = 5;
    const unsigned int Program_vertex_shader = 1;
    // The inserted instruction in tokens: the opcode, rN.xyz (2), vNormal.xyzx (2) and two 4-component immediates (5 each).
    const unsigned int Decode_length = 15;
    // Magic numbers: the most normal reads of one shader (the bloodsplat shaders have 6 or 8) and container chunks.
    const unsigned int Max_reads = 64;
    const unsigned int Max_chunks = 32;

    unsigned int read_u32(const unsigned char* p)
    {
        unsigned int value;
        memcpy(&value, p, 4);
        return value;
    }

    void write_u32(unsigned char* p, unsigned int value)
    {
        memcpy(p, &value, 4);
    }

    bool is_declaration(unsigned int opcode)
    {
        // DCL_RESOURCE (88) .. DCL_GLOBAL_FLAGS (106), the shader model 5 declarations from DCL_STREAM (143) up, custom
        // data (immediate constant buffers).
        return (opcode >= 88 && opcode <= 106) || opcode >= 143 || opcode == Opcode_customdata;
    }

    struct walk_state
    {
        const unsigned int* tokens;
        unsigned int end; // the end of the current instruction
        unsigned int normal; // the NORMAL input register
        unsigned int num_reads;
        unsigned int read_token[Max_reads]; // the operand token of each normal read
        unsigned int read_index[Max_reads]; // its register index token
        bool decoded; // a mad reads the normal as its first source: the shader decodes it already
        bool bad;
    };

    // Walks the operand at tokens[i] and records it when it reads the normal. Returns the index after it.
    unsigned int walk_operand(walk_state& s, unsigned int i, bool top, unsigned int opcode, unsigned int operand)
    {
        if (i >= s.end)
        {
            s.bad = true;
            return s.end;
        }
        const unsigned int token = s.tokens[i];
        const unsigned int head = i++;
        unsigned int extended = token >> 31;
        while (extended != 0)
        {
            if (i >= s.end)
            {
                s.bad = true;
                return s.end;
            }
            extended = s.tokens[i++] >> 31;
        }
        const unsigned int type = (token >> 12) & 0xFF;
        // Number of components: 0 none, 1 one, 2 four.
        const unsigned int num_components = token & 3;
        if (type == Operand_immediate32)
        {
            i += num_components == 1 ? 1 : 4;
        }
        else if (type == Operand_immediate64)
        {
            i += num_components == 1 ? 2 : 8;
        }
        const unsigned int dimension = (token >> 20) & 3;
        const unsigned int index_at = i;
        for (unsigned int d = 0; d < dimension && !s.bad; d++)
        {
            switch ((token >> (22 + 3 * d)) & 7)
            {
            case 0: // immediate32
                i += 1;
                break;
            case 1: // immediate64
                i += 2;
                break;
            case 2: // relative
                i = walk_operand(s, i, false, opcode, operand);
                break;
            case 3: // immediate32 + relative
                i = walk_operand(s, i + 1, false, opcode, operand);
                break;
            case 4: // immediate64 + relative
                i = walk_operand(s, i + 2, false, opcode, operand);
                break;
            default:
                s.bad = true;
                break;
            }
        }
        if (s.bad || i > s.end)
        {
            s.bad = true;
            return s.end;
        }
        if (type == Operand_input && dimension == 1 && ((token >> 22) & 7) == 0 && s.tokens[index_at] == s.normal)
        {
            // Component selection: 0 mask, 1 swizzle, 2 select one. Only top-level reads of x, y and z are rewritten
            // (the shaders declare the normal .xyz, so rN.w is never written).
            const unsigned int selection = (token >> 2) & 3;
            bool reads_w = selection == 2 && ((token >> 4) & 3) == 3;
            for (unsigned int c = 0; selection == 1 && c < 4; c++)
            {
                reads_w |= ((token >> (4 + 2 * c)) & 3) == 3;
            }
            if (!top || num_components != 2 || selection == 0 || reads_w || s.num_reads == Max_reads)
            {
                s.bad = true;
                return s.end;
            }
            // The decode is a mad with the normal as its first source and 2.0f (0x40000000) in the immediate after it
            // (body shaders: mad r.yzw, v.xxyz, l(0, 2, 2, 2), l(0, -1, -1, -1)); other mads (cross products) read it too.
            if (opcode == Opcode_mad && operand == 1 && i + 4 < s.end && ((s.tokens[i] >> 12) & 0xFF) == Operand_immediate32 && (s.tokens[i] & 3) == 2)
            {
                for (unsigned int c = 1; c <= 4; c++)
                {
                    s.decoded |= s.tokens[i + c] == 0x40000000;
                }
            }
            s.read_token[s.num_reads] = head;
            s.read_index[s.num_reads] = index_at;
            s.num_reads++;
        }
        return i;
    }

    // The NORMAL (semantic index 0) register of an ISGN chunk, or ~0.
    unsigned int find_normal_register(const unsigned char* data_p, unsigned int size)
    {
        if (size < 8)
        {
            return ~0u;
        }
        const unsigned int count = read_u32(data_p);
        // Each element: name offset, semantic index, system value, component type, register, masks (24 bytes).
        if (count > (size - 8) / 24)
        {
            return ~0u;
        }
        for (unsigned int e = 0; e < count; e++)
        {
            const unsigned char* element_p = data_p + 8 + e * 24;
            const unsigned int name_offset = read_u32(element_p);
            static const char normal_name[] = "NORMAL";
            if (name_offset < size && size - name_offset >= sizeof(normal_name) && memcmp(data_p + name_offset, normal_name, sizeof(normal_name)) == 0 &&
                read_u32(element_p + 4) == 0)
            {
                return read_u32(element_p + 16);
            }
        }
        return ~0u;
    }

    unsigned int rotate_left(unsigned int x, unsigned int c)
    {
        return (x << c) | (x >> (32 - c));
    }

    void md5_block(unsigned int state[4], const unsigned char* block_p)
    {
        static const unsigned int k[64] = {
            0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501, 0x698098D8, 0x8B44F7AF, 0xFFFF5BB1,
            0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821, 0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453,
            0xD8A1E681, 0xE7D3FBC8, 0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A, 0xFFFA3942,
            0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70, 0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05,
            0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665, 0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D,
            0x85845DD1, 0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391};
        static const unsigned int shift[16] = {7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};
        unsigned int x[16];
        memcpy(x, block_p, sizeof(x));
        unsigned int a = state[0];
        unsigned int b = state[1];
        unsigned int c = state[2];
        unsigned int d = state[3];
        for (unsigned int i = 0; i < 64; i++)
        {
            unsigned int f;
            unsigned int g;
            if (i < 16)
            {
                f = (b & c) | (~b & d);
                g = i;
            }
            else if (i < 32)
            {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) & 15;
            }
            else if (i < 48)
            {
                f = b ^ c ^ d;
                g = (3 * i + 5) & 15;
            }
            else
            {
                f = c ^ (b | ~d);
                g = (7 * i) & 15;
            }
            f += a + k[i] + x[g];
            a = d;
            d = c;
            c = b;
            b += rotate_left(f, shift[(i >> 4) * 4 + (i & 3)]);
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }

    // The program chunk (SHDR / SHEX) of a DXBC container: its tokens and their count, or false when the container is
    // not well formed.
    bool find_program(const unsigned char* blob_p, unsigned int blob_size, unsigned int* tokens_offset_p, unsigned int* count_p)
    {
        // The container: "DXBC", the checksum (16 bytes), 1, the total size, the chunk count and the chunk offsets; each
        // chunk is its four character code, its size and its data.
        if (blob_size < 32 || memcmp(blob_p, "DXBC", 4) != 0 || read_u32(blob_p + 24) != blob_size)
        {
            return false;
        }
        const unsigned int num_chunks = read_u32(blob_p + 28);
        if (num_chunks == 0 || num_chunks > Max_chunks || 32 + num_chunks * 4 > blob_size)
        {
            return false;
        }
        for (unsigned int c = 0; c < num_chunks; c++)
        {
            const unsigned int offset = read_u32(blob_p + 32 + c * 4);
            if (offset > blob_size - 8 || read_u32(blob_p + offset + 4) > blob_size - 8 - offset)
            {
                return false;
            }
            const unsigned char* chunk_p = blob_p + offset;
            const unsigned int size = read_u32(chunk_p + 4);
            if ((memcmp(chunk_p, "SHDR", 4) == 0 || memcmp(chunk_p, "SHEX", 4) == 0) && size >= 8 && (size & 3) == 0 &&
                read_u32(chunk_p + 12) == size / 4)
            {
                *tokens_offset_p = offset + 8;
                *count_p = size / 4;
                return true;
            }
        }
        return false;
    }

    // The sky dome fix (rl_shader_fixup.h): in the one min r.xy, r.xyxx, l(1.0, 10.0, 0.0, 0.0) of the vertex shader,
    // the height clamp 10.0 becomes the float below it. The blob keeps its size; only the token and the checksum change.
    unsigned char* fixup_skybox_dome(const unsigned char* blob_p, unsigned int blob_size, unsigned int* patched_size_p)
    {
        unsigned int tokens_offset;
        unsigned int count;
        if (!find_program(blob_p, blob_size, &tokens_offset, &count) || (read_u32(blob_p + tokens_offset) >> 16) != Program_vertex_shader)
        {
            return nullptr;
        }
        // Magic numbers: the instruction is 10 tokens (opcode, rN.xy and rN.xyxx at 2 each, a four component immediate32
        // at 5); 0x00004002 is that immediate's operand token, then 1.0f, 10.0f, 0.0f, 0.0f.
        const unsigned int Min_length = 10;
        const unsigned int Clamp_immediate[5] = {0x00004002, 0x3F800000, 0x41200000, 0x00000000, 0x00000000};
        unsigned int clamp_at = 0;
        unsigned int num_found = 0;
        for (unsigned int i = 2; i < count;)
        {
            const unsigned int token = read_u32(blob_p + tokens_offset + i * 4);
            const unsigned int opcode = token & 0x7FF;
            const unsigned int length = opcode == Opcode_customdata ? (i + 1 < count ? read_u32(blob_p + tokens_offset + (i + 1) * 4) : 0) : (token >> 24) & 0x7F;
            if (length == 0 || length > count - i)
            {
                return nullptr;
            }
            if (opcode == Opcode_min && length == Min_length && memcmp(blob_p + tokens_offset + (i + 5) * 4, Clamp_immediate, sizeof(Clamp_immediate)) == 0)
            {
                // The 10.0f token.
                clamp_at = tokens_offset + (i + 7) * 4;
                num_found++;
            }
            i += length;
        }
        if (num_found != 1)
        {
            return nullptr;
        }
        unsigned char* out_p = (unsigned char*)malloc(blob_size);
        memcpy(out_p, blob_p, blob_size);
        // Magic number: 0x411FFFFF = 9.99999905f, the largest float below 10.
        write_u32(out_p + clamp_at, 0x411FFFFF);
        unsigned int checksum[4];
        rl_shader_fixup_dxbc_checksum(out_p, blob_size, checksum);
        memcpy(out_p + 4, checksum, sizeof(checksum));
        *patched_size_p = blob_size;
        return out_p;
    }
}

void rl_shader_fixup_dxbc_checksum(const unsigned char* blob_p, unsigned int blob_size, unsigned int checksum[4])
{
    // Magic numbers: the MD5 initial state; the hash covers the container from offset 20 (after the magic and itself).
    unsigned int state[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    const unsigned char* data_p = blob_p + 20;
    const unsigned int size = blob_size - 20;
    const unsigned int full = size & ~63u;
    for (unsigned int offset = 0; offset < full; offset += 64)
    {
        md5_block(state, data_p + offset);
    }
    // The container's padding differs from MD5: the bit count goes first in the last block and (size * 2) | 1 last.
    const unsigned int rest = size - full;
    unsigned char block[64];
    memset(block, 0, sizeof(block));
    if (rest < 56)
    {
        write_u32(block, size << 3);
        memcpy(block + 4, data_p + full, rest);
        block[4 + rest] = 0x80;
    }
    else
    {
        memcpy(block, data_p + full, rest);
        block[rest] = 0x80;
        md5_block(state, block);
        memset(block, 0, sizeof(block));
        write_u32(block, size << 3);
    }
    write_u32(block + 60, (size << 1) | 1);
    md5_block(state, block);
    memcpy(checksum, state, sizeof(state));
}

unsigned char* rl_shader_fixup_vertex_shader(const char* shader_fname, const unsigned char* blob_p, unsigned int blob_size, unsigned int* patched_size_p, unsigned int* num_reads_p, const char** fix_p)
{
    *patched_size_p = 0;
    *num_reads_p = 0;
    *fix_p = "";
    if (_strnicmp(shader_fname, Skybox_dome_shader, sizeof(Skybox_dome_shader) - 1) == 0)
    {
        *fix_p = "dome height clamp below 10";
        return fixup_skybox_dome(blob_p, blob_size, patched_size_p);
    }
    *fix_p = "normal decoded";
    bool listed = false;
    for (const char* prefix : Fixed_shaders)
    {
        listed |= _strnicmp(shader_fname, prefix, strlen(prefix)) == 0;
    }
    if (!listed)
    {
        return nullptr;
    }

    // The container: "DXBC", the checksum (16 bytes), 1, the total size, the chunk count and the chunk offsets; each
    // chunk is its four character code, its size and its data.
    if (blob_size < 32 || memcmp(blob_p, "DXBC", 4) != 0 || read_u32(blob_p + 24) != blob_size)
    {
        return nullptr;
    }
    const unsigned int num_chunks = read_u32(blob_p + 28);
    if (num_chunks == 0 || num_chunks > Max_chunks || 32 + num_chunks * 4 > blob_size)
    {
        return nullptr;
    }
    unsigned int shader_chunk = num_chunks;
    unsigned int normal = ~0u;
    for (unsigned int c = 0; c < num_chunks; c++)
    {
        const unsigned int offset = read_u32(blob_p + 32 + c * 4);
        if (offset > blob_size - 8 || read_u32(blob_p + offset + 4) > blob_size - 8 - offset)
        {
            return nullptr;
        }
        const unsigned char* chunk_p = blob_p + offset;
        if (memcmp(chunk_p, "ISGN", 4) == 0)
        {
            normal = find_normal_register(chunk_p + 8, read_u32(chunk_p + 4));
        }
        else if (memcmp(chunk_p, "SHDR", 4) == 0 || memcmp(chunk_p, "SHEX", 4) == 0)
        {
            shader_chunk = c;
        }
    }
    if (normal == ~0u || shader_chunk == num_chunks)
    {
        return nullptr;
    }
    const unsigned char* code_chunk_p = blob_p + read_u32(blob_p + 32 + shader_chunk * 4);
    const unsigned int code_size = read_u32(code_chunk_p + 4);
    if (code_size < 8 || (code_size & 3) != 0)
    {
        return nullptr;
    }

    // The program: the version token (program type in the high 16 bits), the length in tokens, the instructions.
    const unsigned int count = code_size / 4;
    unsigned int* tokens = (unsigned int*)malloc((count + Decode_length) * 4);
    memcpy(tokens, code_chunk_p + 8, code_size);
    walk_state* s = (walk_state*)calloc(1, sizeof(walk_state));
    s->tokens = tokens;
    s->normal = normal;
    s->bad = (tokens[0] >> 16) != Program_vertex_shader || tokens[1] != count;
    unsigned int insert_at = 0;
    unsigned int temps_at = 0;
    for (unsigned int i = 2; i < count && !s->bad;)
    {
        const unsigned int token = tokens[i];
        const unsigned int opcode = token & 0x7FF;
        const unsigned int length = opcode == Opcode_customdata ? (i + 1 < count ? tokens[i + 1] : 0) : (token >> 24) & 0x7F;
        if (length == 0 || length > count - i)
        {
            s->bad = true;
            break;
        }
        const unsigned int end = i + length;
        if (is_declaration(opcode))
        {
            // Every declaration comes before the first instruction.
            s->bad = insert_at != 0;
            if (opcode == Opcode_dcl_temps && length == 2)
            {
                temps_at = i + 1;
            }
        }
        else
        {
            if (insert_at == 0)
            {
                insert_at = i;
            }
            s->end = end;
            unsigned int j = i + 1;
            for (unsigned int extended = token >> 31; extended != 0 && j < end;)
            {
                extended = tokens[j++] >> 31;
            }
            for (unsigned int operand = 0; j < end && !s->bad; operand++)
            {
                j = walk_operand(*s, j, true, opcode, operand);
            }
            s->bad |= j != end;
        }
        i = end;
    }
    const bool patch = !s->bad && !s->decoded && s->num_reads != 0 && temps_at != 0 && insert_at != 0;
    if (!patch)
    {
        free(s);
        free(tokens);
        return nullptr;
    }

    // The new temp register rN, N the old temp count.
    const unsigned int temp = tokens[temps_at];
    tokens[temps_at] = temp + 1;
    for (unsigned int r = 0; r < s->num_reads; r++)
    {
        tokens[s->read_token[r]] = (tokens[s->read_token[r]] & ~(0xFFu << 12)) | (Operand_temp << 12);
        tokens[s->read_index[r]] = temp;
    }
    *num_reads_p = s->num_reads;
    free(s);
    // mad rN.xyz, vNormal.xyzx, l(2, 2, 2, 2), l(-1, -1, -1, -1): the UNORM [0, 1] normal to [-1, 1].
    // Magic numbers: the operand tokens (rN.xyz: four components, mask xyz, temp, one immediate index; vNormal.xyzx:
    // four components, swizzle xyzx, input, one immediate index; a four component immediate32) and 2.0f / -1.0f.
    const unsigned int decode[Decode_length] = {
        Opcode_mad | (Decode_length << 24), 0x00100072, temp, 0x00101246, normal, 0x00004002, 0x40000000, 0x40000000, 0x40000000, 0x40000000,
        0x00004002, 0xBF800000, 0xBF800000, 0xBF800000, 0xBF800000};
    memmove(tokens + insert_at + Decode_length, tokens + insert_at, (count - insert_at) * 4);
    memcpy(tokens + insert_at, decode, sizeof(decode));
    tokens[1] = count + Decode_length;

    // The container rebuilt with the chunks in their order, the program chunk grown.
    const unsigned int header_size = 32 + num_chunks * 4;
    unsigned int total = header_size;
    for (unsigned int c = 0; c < num_chunks; c++)
    {
        total += 8 + (c == shader_chunk ? (count + Decode_length) * 4 : read_u32(blob_p + read_u32(blob_p + 32 + c * 4) + 4));
    }
    unsigned char* out_p = (unsigned char*)malloc(total);
    memcpy(out_p, blob_p, 32);
    write_u32(out_p + 24, total);
    unsigned int at = header_size;
    for (unsigned int c = 0; c < num_chunks; c++)
    {
        const unsigned char* chunk_p = blob_p + read_u32(blob_p + 32 + c * 4);
        write_u32(out_p + 32 + c * 4, at);
        memcpy(out_p + at, chunk_p, 4);
        if (c == shader_chunk)
        {
            write_u32(out_p + at + 4, (count + Decode_length) * 4);
            memcpy(out_p + at + 8, tokens, (count + Decode_length) * 4);
            at += 8 + (count + Decode_length) * 4;
        }
        else
        {
            const unsigned int size = read_u32(chunk_p + 4);
            memcpy(out_p + at + 4, chunk_p + 4, 4 + size);
            at += 8 + size;
        }
    }
    free(tokens);
    unsigned int checksum[4];
    rl_shader_fixup_dxbc_checksum(out_p, total, checksum);
    memcpy(out_p + 4, checksum, sizeof(checksum));
    *patched_size_p = total;
    return out_p;
}
