// Original: volition/vlib/code/math/rand.cpp
//
// Random numbers (decompiled; header comment in rand.h). A rand_instance is a Mersenne twister (MT19937) that fills a
// table of 2048 numbers up front; the rand helpers hand them out in order and wrap around, so the sequence repeats
// every 2048 draws until rand_regenerate refills the table. The global helpers use Vlib_rand_instance (seeded by
// vlib_system_init through rand_init).
//
// Notes (decompiled behaviour, kept as is):
// - fl_rand, fl_rand_range and fl_rand_chance wrap the table index with == 2048, the others with > 2047.
// - bool_rand is false only for draws below 0x7FFFFFFF, so it is true slightly more often than false.

#include "volition/vlib/code/math/rand.h"

static const int MT_N = 624; // MT19937 state size
static const int MT_M = 397;
static const unsigned int MT_DEFAULT_SEED = 5489; // 0x1571, the generator's own default seed
static const int RAND_ARRAY_SIZE = 2048; // rand_instance::m_rand_array
static const float RAND_SCALE = 2.3283064e-10f; // 1 / 2^32

// Built by the constructor with seed 0 (0x010601D0): Mti starts at MT_N + 1, so the first draw seeds with 5489.
rand_instance Vlib_rand_instance(0); // 0x0163B2B8

// ---- rand_instance
// 0x0057A040 (:133)
void rand_instance::sgenrand(unsigned int seed)
{
    Mt[0] = seed;
    for (Mti = 1; Mti < MT_N; Mti++)
    {
        Mt[Mti] = (Mt[Mti - 1] ^ (Mt[Mti - 1] >> 30)) * 0x6C078965 + Mti;
    }
}

// 0x0057A090 (:152)
unsigned int rand_instance::genrand()
{
    static const unsigned int mag01[2] = {0, 0x9908B0DF};
    if (Mti >= MT_N)
    {
        if (Mti == MT_N + 1)
        {
            sgenrand(MT_DEFAULT_SEED);
        }
        int kk;
        for (kk = 0; kk < MT_N - MT_M; kk++)
        {
            const unsigned int y = (Mt[kk] & 0x80000000) | (Mt[kk + 1] & 0x7FFFFFFF);
            Mt[kk] = Mt[kk + MT_M] ^ (y >> 1) ^ mag01[y & 1];
        }
        for (; kk < MT_N - 1; kk++)
        {
            const unsigned int y = (Mt[kk] & 0x80000000) | (Mt[kk + 1] & 0x7FFFFFFF);
            Mt[kk] = Mt[kk + (MT_M - MT_N)] ^ (y >> 1) ^ mag01[y & 1];
        }
        const unsigned int y = (Mt[MT_N - 1] & 0x80000000) | (Mt[0] & 0x7FFFFFFF);
        Mt[MT_N - 1] = Mt[MT_M - 1] ^ (y >> 1) ^ mag01[y & 1];
        Mti = 0;
    }
    unsigned int y = Mt[Mti++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680; // (y & 0xFF3A58AD) << 7 in the binary
    y ^= (y << 15) & 0xEFC60000; // (y & 0xFFFFDF8C) << 15
    y ^= y >> 18;
    return y;
}

// 0x0057A1E0 (:220)
// Seed 0 keeps the current (or default) state.
rand_instance::rand_instance(unsigned int seed)
{
    Mti = MT_N + 1;
    m_next_rand = 0;
    rand_regenerate(seed); // inlined
}

// 0x0057A1A0 (:205)
void rand_instance::rand_regenerate(unsigned int seed)
{
    if (seed != 0)
    {
        sgenrand(seed);
    }
    m_next_rand = 0;
    for (int i = 0; i < RAND_ARRAY_SIZE; i++)
    {
        m_rand_array[i] = genrand();
    }
}

// 0x0057A230 (:233)
unsigned int rand_instance::vrand()
{
    const unsigned int value = m_rand_array[m_next_rand];
    if (++m_next_rand > RAND_ARRAY_SIZE - 1)
    {
        m_next_rand = 0;
    }
    return value;
}

// 0x0057A260 (:250)
// [0, 1)
float rand_instance::fl_rand()
{
    const unsigned int value = m_rand_array[m_next_rand];
    if (++m_next_rand == RAND_ARRAY_SIZE)
    {
        m_next_rand = 0;
    }
    return (float)value * RAND_SCALE;
}

// 0x0057A310 (:290)
// [minimum, maximum]
int rand_instance::int_rand_range(int minimum, int maximum)
{
    const unsigned int value = m_rand_array[m_next_rand];
    if (++m_next_rand > RAND_ARRAY_SIZE - 1)
    {
        m_next_rand = 0;
    }
    return minimum + (int)(value % (unsigned int)(maximum - minimum + 1));
}

// 0x0057A2B0 (:276)
// [minimum, maximum)
float rand_instance::fl_rand_range(float minimum, float maximum)
{
    const unsigned int value = m_rand_array[m_next_rand];
    if (++m_next_rand == RAND_ARRAY_SIZE)
    {
        m_next_rand = 0;
    }
    // x87 code: the unit value is rounded to float (as fl_rand), the rest stays in double and is rounded once (checked
    // by the hkdiff glass_create_shatter_effect test).
    const float unit = (float)value * RAND_SCALE;
    return (float)(((double)maximum - (double)minimum) * (double)unit + (double)minimum);
}

// ---- global helpers on Vlib_rand_instance (inlined members)
// 0x0057A350 (:305)
void rand_init(unsigned int seed)
{
    Vlib_rand_instance.rand_regenerate(seed);
}

// 0x0057A390 (:311)
void rand_regenerate(unsigned int seed)
{
    Vlib_rand_instance.rand_regenerate(seed);
}

// 0x0057A3D0 (:317)
unsigned int vrand()
{
    return Vlib_rand_instance.vrand();
}

// 0x0057A400 (:323)
float fl_rand()
{
    return Vlib_rand_instance.fl_rand();
}

// 0x0057A450 (:330)
bool bool_rand()
{
    return Vlib_rand_instance.vrand() >= 0x7FFFFFFF; // see the notes
}

// 0x0057A480 (:337)
int int_rand_range(int minimum, int maximum)
{
    return Vlib_rand_instance.int_rand_range(minimum, maximum);
}

// 0x0057A4C0 (:343)
float fl_rand_range(float minimum, float maximum)
{
    return Vlib_rand_instance.fl_rand_range(minimum, maximum);
}

// ---- rand.inl
// 0x005A5D50 (rand.inl:22)
// pct in percent.
bool int_rand_chance(unsigned int pct)
{
    return Vlib_rand_instance.vrand() % 100 <= pct;
}

// 0x005A5D90 (rand.inl:29)
bool fl_rand_chance(float pct)
{
    return Vlib_rand_instance.fl_rand() <= pct;
}
