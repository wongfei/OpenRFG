// Original: volition/render_lib/code/rl_vertex_format_defines.h
//
// Vertex component encodings (rl_compressed_vector and friends).
#pragma once


class vector;

// size 0x4
struct rl_compressed_vector_8888
{
    void set(float, float, float, float);
    void set(const vector&, float);
    void get(vector&) const;
    float get_w() const;
    void get_w(vector&, float&) const;

    unsigned int data; // 0x00
};

// size 0x4
struct rl_compressed_vector
{
    void set(float x, float y, float z, float w); // 0x004B6A70 (.h:122)
    void set(const vector& v, float w); // 0x006E6C40 (.h:119)
    void get(vector&) const;
    float get_w() const;
    void get_w(vector&, float&) const;
    void add(const rl_compressed_vector&);

    unsigned int data; // 0x00
};

// ---- rl_compressed_vector header inlines (VFX.md FX8, the glass vertices)

// Not in the binary: a component mapped from [-1, 1] to [0, 1] and clamped (the tests in double, as rfg.exe).
inline float rl_compressed_vector_unit(float v)
{
    float c = (float)((double)v * 0.5 + 0.5);
    if ((float)((double)c - 0.0) < 0.0f)
    {
        c = 0.0f;
    }
    if (!((float)((double)c - 1.0) < 0.0f))
    {
        c = 1.0f;
    }
    return c;
}

// 0x004B6A70 (.h:122)
// Each component to a byte (x in the low byte); the conversion truncates (the x87 chop mode).
inline void rl_compressed_vector::set(float x, float y, float z, float w)
{
    // Magic number: 255 per unit (a double constant).
    const unsigned int bx = (unsigned int)(long long)((double)rl_compressed_vector_unit(x) * 255.0);
    const unsigned int by = (unsigned int)(long long)((double)rl_compressed_vector_unit(y) * 255.0);
    const unsigned int bz = (unsigned int)(long long)((double)rl_compressed_vector_unit(z) * 255.0);
    const unsigned int bw = (unsigned int)(long long)((double)rl_compressed_vector_unit(w) * 255.0);
    data = ((bw << 8 | bz) << 8 | by) << 8 | bx;
}
