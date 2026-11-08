// Original: volition/rfg/code/effects/object_effect_profile.h
// Implementation: volition/rfg/code/effects/object_effect_profile.cpp
//
// object_effect_profile, per-effect particle statistics (simulated, filtered, extra iterated and invisible particles,
// particles per emitter) sampled each update while Vfx_profiling_enabled. Each statistic keeps its average, maximum,
// last value and the maximum it stayed at for a twentieth of the samples (peaked).
#pragma once

class object_effect;

// size 0xB4
class object_effect_profile
{
public:
    // size 0xB0
    struct data
    {
        // size 0x1C
        template<typename T>
        struct entry
        {
            entry(); // <int> 0x00744170 (.h:28)
            void add_new(T new_entry); // <int> 0x00746BD0 (.h:36)

            T avg; // 0x00
            T maximum; // 0x04
            T last; // 0x08
            T peaked; // 0x0C
            int max_repro; // 0x10
            bool first_add; // 0x14
            int num_samples; // 0x18
        };

        entry<int> particles_simulated_count; // 0x00
        entry<int> extra_iterated_particle_count; // 0x1C
        entry<int> particles_filtered; // 0x38
        entry<int> greater_than_256texture_particles; // 0x54
        entry<int> animated_texture_particles; // 0x70
        int num_unique_animated_textures; // 0x8C
        int particles_to_emitter_ratio; // 0x90
        entry<int> simulating_alpha0_particles; // 0x94
    };

    object_effect_profile(); // 0x0074AC80 (:11)
    ~object_effect_profile(); // 0x00744160 (:16)
    void init(object_effect* object_effect_p); // 0x0074ACA0 (:20)
    void update(); // 0x0074ACD0 (:26)

private:
    data m_profile_data; // 0x00
    object_effect* m_object_effect_p; // 0xB0
};
static_assert(sizeof(object_effect_profile::data::entry<int>) == 0x1C, "object_effect_profile::data::entry<int>");
static_assert(sizeof(object_effect_profile::data) == 0xB0, "object_effect_profile::data");
static_assert(sizeof(object_effect_profile) == 0xB4, "object_effect_profile");

// <int> 0x00744170 (.h:28)
// avg, maximum and last start undefined, as in the original.
template<typename T>
object_effect_profile::data::entry<T>::entry()
{
    first_add = true;
    peaked = 0;
    max_repro = 0;
    num_samples = 0;
}

// <int> 0x00746BD0 (.h:36)
// A value at the maximum again counts towards peaked: once it has repeated for a twentieth of the samples, the maximum
// is the peak. The average is a running halving average.
template<typename T>
void object_effect_profile::data::entry<T>::add_new(T new_entry)
{
    ++num_samples;
    last = new_entry;
    if (!first_add)
    {
        if (new_entry == maximum)
        {
            const int repro = max_repro++;
            if (num_samples / 20 <= repro) // a twentieth of the samples
            {
                peaked = maximum;
            }
        }
        else
        {
            max_repro = 0;
        }
    }
    maximum = maximum < new_entry ? new_entry : maximum;
    avg = (avg + new_entry) / 2;
    first_add = false;
}
