// Original: volition/vlib/code/math/rand.h
// Implementation: volition/vlib/code/math/rand.cpp
//
// Random numbers: rand_instance (a Mersenne twister state; Vlib_rand_instance is the global one) and the global rand
// helpers (fl_rand, int_rand_range, ...).
#pragma once

// size 0x29C8
class rand_instance
{
public:
    rand_instance(unsigned int seed); // 0x0057A1E0 (:220)
    void rand_regenerate(unsigned int seed); // 0x0057A1A0 (:205)
    unsigned int vrand(); // 0x0057A230 (:233)
    float fl_rand(); // 0x0057A260 (:250)
    int int_rand_range(int minimum, int maximum); // 0x0057A310 (:290)
    float fl_rand_range(float minimum, float maximum); // 0x0057A2B0 (:276)
    // vlib_system_init refills the global instance's table (vlib_core.cpp, inlined there).
    friend bool vlib_system_init(const char* vlib_relative_path, bool init_cfile, int vlib_mempool_size);
private:
    void sgenrand(unsigned int seed); // 0x0057A040 (:133)
    unsigned int genrand(); // 0x0057A090 (:152)

    unsigned int Mt[624]; // 0x00
    int Mti; // 0x9C0
    int m_next_rand; // 0x9C4
    unsigned int m_rand_array[2048]; // 0x9C8
};

extern rand_instance Vlib_rand_instance; // 0x0163B2B8

void rand_init(unsigned int seed); // 0x0057A350 (:305)
void rand_regenerate(unsigned int seed); // 0x0057A390 (:311)
unsigned int vrand(); // 0x0057A3D0 (:317)
float fl_rand(); // 0x0057A400 (:323)
bool bool_rand(); // 0x0057A450 (:330)
int int_rand_range(int minimum, int maximum); // 0x0057A480 (:337)
float fl_rand_range(float minimum, float maximum); // 0x0057A4C0 (:343)
bool int_rand_chance(unsigned int pct); // 0x005A5D50 (rand.inl:22)
bool fl_rand_chance(float pct); // 0x005A5D90 (rand.inl:29)
