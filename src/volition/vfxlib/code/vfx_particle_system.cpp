// Original: volition/vfxlib/code/vfx_particle_system.cpp
//
// VFX::particle_system (VFX.md FX1): an unused stub in rfg.exe; every function is empty (the emitter instances are kept
// by their effects, vfx_effect.cpp).

#include "volition/vfxlib/code/vfx_particle_system.h"

namespace VFX
{
    // 0x00CB5B20 (:21)
    particle_system::particle_system()
    {
    }

    // 0x00CAF570 (:32)
    particle_system::~particle_system()
    {
    }

    // 0x00C9F060 (:38)
    void particle_system::update(float delta_time)
    {
    }

    // 0x00C9F070 (:46)
    void particle_system::render()
    {
    }

    // 0x00C9F080 (:52)
    int particle_system::add_emitter_instance(particle_system_emitter_instance* instance)
    {
        return -1;
    }

    // 0x00C9F090 (:63)
    void particle_system::remove_emitter_instance(int instance_handle)
    {
    }

    // 0x00CAF4E0
    particle_system::emitter_list::emitter_list()
    {
    }

    // 0x00CAF500
    particle_system::emitter_list::~emitter_list()
    {
    }
}
