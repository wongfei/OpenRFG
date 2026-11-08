// Original: volition/vfxlib/code/vfx_particle_system.h
// Implementation: volition/vfxlib/code/vfx_particle_system.cpp
//
// The particle system: the list of active emitter instances (VFX::particle_system_emitter_instance is only declared).
#pragma once

#include "volition/render_lib/code/pool_list.h"

namespace VFX {
class particle_system_emitter_instance;
}

namespace VFX
{
    // size 0x18
    class particle_system
    {
    public:
        // size 0x18
        class emitter_list : public pool_list<particle_system_emitter_instance*>
        {
        public:
            emitter_list(); // 0x00CAF4E0
            virtual ~emitter_list(); // vtbl+0x00 0x00CAF500

        };

        void update(float delta_time); // 0x00C9F060 (:38)
        void render(); // 0x00C9F070 (:46)
        int add_emitter_instance(particle_system_emitter_instance* instance); // 0x00C9F080 (:52)
        void remove_emitter_instance(int instance_handle); // 0x00C9F090 (:63)
        void set_max_emitter_instances(int);
        int get_max_emitter_instances();
    protected:
        particle_system(); // 0x00CB5B20 (:21)
        ~particle_system(); // 0x00CAF570 (:32)

    public:
        static const int max_emitter_instances_default; // 0x01343DE4
    private:
        static int max_emitter_instances; // 0x016F0A0C
    protected:
        emitter_list emitter_instances; // 0x00
    };
}
