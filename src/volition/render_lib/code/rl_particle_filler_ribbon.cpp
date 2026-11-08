// Original: volition/render_lib/code/rl_particle_filler_ribbon.cpp
//
// The ribbon filler of rl_particle_renderer (VFX.md FX2): one strip through all particles of the instance, newest
// (last) first, two vertices per particle. Each vertex also carries its neighbours so the vertex shader can turn the
// ribbon to the camera. The u coordinate alternates with the particle's emit number (a seam texture trick); the first
// particle gets 0.1 / 1.1 (+2 when there are more) so the shader can tell the ends apart.

#include "volition/render_lib/code/rl_particle_renderer.h"

#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"

// The u coordinate of ribbon particle index (see the file comment).
static float ribbon_u(int emit_number, int index, int num_particles)
{
    float u;
    if ((emit_number & 1) == 0)
    {
        // Magic numbers: even particles at u 1 (1.1 at the start).
        u = index == 0 ? 1.1f : 1.0f;
    }
    else
    {
        u = index == 0 ? 0.1f : 0.0f;
    }
    if (index == 0 && 1 < num_particles)
    {
        u = u + 2.0f;
    }
    return u;
}

static vector ribbon_local_to_world(const rl_renderable_particles_instance* instance_p, const vector& position)
{
    const matrix rotation(instance_p->get_local_emission_rotation());
    const vector& origin = instance_p->get_local_emission_position();
    vector result;
    result.x = rotation.rvec.x * position.x + rotation.uvec.x * position.y + rotation.fvec.x * position.z + origin.x;
    result.y = rotation.rvec.y * position.x + rotation.uvec.y * position.y + rotation.fvec.y * position.z + origin.y;
    result.z = rotation.rvec.z * position.x + rotation.uvec.z * position.y + rotation.fvec.z * position.z + origin.z;
    return result;
}

// 0x004EE880 (:8): each particle with its predecessor (index 0 pairs with index 1); the shader evaluates both.
// rfg.exe unrolls the loop four times; one particle per iteration here.
void rl_particle_renderer::filler_ribbon_parametric(rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    const pool_list<rl_particle_parametric>& particles = instance_p->get_particle_parametric_list();
    const int num_particles = particles.size();
    for (int index = num_particles - 1; -1 < index; index--)
    {
        const rl_particle_parametric& particle = particles[index];
        const rl_particle_parametric& other = index == 0 ? particles[1] : particles[index - 1];
        ribbon_parametric_vertex vertex;
        vertex.pos_1 = vector4(particle.position.x, particle.position.y, particle.position.z, particle.lifetime);
        vertex.pos_2 = vector4(other.position.x, other.position.y, other.position.z, other.lifetime);
        vertex.uvs.x = ribbon_u(particle.emit_number, index, num_particles);
        vertex.time_created.x = particle.time_created;
        vertex.time_created.y = other.time_created;
        vertex.random_1.x = particle.random.x;
        vertex.random_1.y = particle.random.y;
        vertex.random_1.z = particle.random.z;
        vertex.random_1.w = particle.random.w;
        vertex.random_2.x = other.random.x;
        vertex.random_2.y = other.random.y;
        vertex.random_2.z = other.random.z;
        vertex.random_2.w = other.random.w;
        ribbon_parametric_vertex* vertices_p = (ribbon_parametric_vertex*)vertex_p;
        vertex.uvs.y = 0.0f;
        vertices_p[0] = vertex;
        vertex.uvs.y = 1.0f;
        vertices_p[1] = vertex;
        num_verts += 2;
        vertex_p += current_vertex_stride * 2;
        num_prims += 2;
    }
}

// 0x004EF0F0 (:110): each particle with its previous and next neighbours (the ends repeat themselves); the two
// vertices are offset to either side by the particle size. The animation time is updated every second particle, and
// the first particle is transparent when the ribbon has more than two.
void rl_particle_renderer::filler_ribbon_iterative(rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    const bool local_space = instance_p->get_renderable_particles()->get_local_space_emission();
    pool_list<rl_particle_iterative>& particles = instance_p->get_particle_iterative_list();
    const int num_particles = particles.size();
    float anim_time = 0.0f;
    int count = 0;
    for (int index = num_particles - 1; -1 < index; index--)
    {
        const rl_particle_iterative& particle = particles[index];
        if ((count & 1) == 0)
        {
            anim_time = get_uv_anim_time(instance_p, particle);
        }
        const rl_particle_iterative& previous = index != 0 ? particles[index - 1] : particles[0];
        const rl_particle_iterative& next = index != num_particles - 1 ? particles[index + 1] : particle;
        ribbon_vertex vertex;
        vertex.pos = particle.position;
        vertex.vel1 = next.position;
        vertex.vel2 = previous.position;
        if (local_space)
        {
            vertex.pos = ribbon_local_to_world(instance_p, vertex.pos);
            vertex.vel2 = ribbon_local_to_world(instance_p, vertex.vel2);
            vertex.vel1 = ribbon_local_to_world(instance_p, vertex.vel1);
        }
        unsigned int alpha = (unsigned int)(int)(particle.current_color.alpha * 255.0f + 0.5f);
        if (index == 0 && 2 < num_particles)
        {
            alpha = 0;
        }
        // Magic numbers: 0..1 to 0..255, rounded (D3DCOLOR).
        vertex.rgba = (((alpha & 0xFF) << 8 | ((unsigned int)(int)(particle.current_color.red * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(particle.current_color.green * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(particle.current_color.blue * 255.0f + 0.5f) & 0xFF);
        vertex.saturation = particle.current_color_saturation;
        vertex.uvs.x = (particle.emit_number & 1) == 0 ? 1.0f : 0.0f;
        vertex.v_trans.y = (float)count;
        vertex.additive_key = particle.current_additive_key;
        vertex.anim_time = anim_time;
        ribbon_vertex* vertices_p = (ribbon_vertex*)vertex_p;
        vertex.uvs.y = 0.0f;
        vertex.v_trans.x = particle.current_size;
        vertices_p[0] = vertex;
        vertex.uvs.y = 1.0f;
        vertex.v_trans.x = -particle.current_size;
        vertices_p[1] = vertex;
        num_verts += 2;
        vertex_p += current_vertex_stride * 2;
        num_prims += 2;
        count++;
    }
}

// 0x004EFB30 (:239)
void rl_particle_renderer::filler_ribbon(rl_renderable_particles_instance* instance_p, const bool is_iterative, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    if (is_iterative)
    {
        filler_ribbon_iterative(instance_p, vertex_p, num_verts, num_prims, current_vertex_stride);
        return;
    }
    filler_ribbon_parametric(instance_p, vertex_p, num_verts, num_prims, current_vertex_stride);
}
