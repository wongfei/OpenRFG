// Original: volition/render_lib/code/rl_particle_filler_radial.cpp
//
// The radial filler of rl_particle_renderer (VFX.md FX2): quads oriented away from where the particle was emitted
// (the vertex carries the emission position). Corner uvs (0,0), (0,1), (1,0), (1,1); no tails.

#include "volition/render_lib/code/rl_particle_renderer.h"

#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"

static unsigned int radial_pack_color(const rl_color_float& color)
{
    // Magic numbers: 0..1 to 0..255, rounded.
    return ((((unsigned int)(int)(color.alpha * 255.0f + 0.5f) & 0xFF) << 8 | ((unsigned int)(int)(color.red * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(color.green * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(color.blue * 255.0f + 0.5f) & 0xFF);
}

static vector radial_local_to_world(const rl_renderable_particles_instance* instance_p, const vector& position)
{
    const matrix& rotation = instance_p->get_local_emission_rotation();
    const vector& origin = instance_p->get_local_emission_position();
    vector result;
    result.x = rotation.rvec.x * position.x + rotation.uvec.x * position.y + rotation.fvec.x * position.z + origin.x;
    result.y = rotation.rvec.y * position.x + rotation.uvec.y * position.y + rotation.fvec.y * position.z + origin.y;
    result.z = rotation.rvec.z * position.x + rotation.uvec.z * position.y + rotation.fvec.z * position.z + origin.z;
    return result;
}

// 0x004ED950 (:20)
void rl_particle_renderer::radial_add_particle(const rl_particle_iterative& particle, float rotation_angle, const vector& position_in, const rl_color_float& current_color, float current_size, bool rotation_enabled, bool local_space, rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    radial_vertex vertex;
    vertex.pos = local_space ? radial_local_to_world(instance_p, position_in) : position_in;
    vertex.emission_pos = local_space ? radial_local_to_world(instance_p, particle.emission_position) : particle.emission_position;
    vertex.rgba = radial_pack_color(current_color);
    vertex.saturation = particle.current_color_saturation;
    vertex.current_size = current_size;
    vertex.additive_key = particle.current_additive_key;
    vertex.anim_time = get_uv_anim_time(instance_p, particle);
    radial_vertex* vertices_p = (radial_vertex*)vertex_p;
    for (int i = 0; i < 4; i++)
    {
        vertex.uvs.x = (i & 2) != 0 ? 1.0f : 0.0f;
        vertex.uvs.y = (i & 1) != 0 ? 1.0f : 0.0f;
        vertices_p[i] = vertex;
    }
    num_verts += 4;
    vertex_p += current_vertex_stride * 4;
    num_prims += 6;
}

// 0x004EDFD0 (:118): in order (or reversed); parametric particles write the particle to all four corners.
void rl_particle_renderer::filler_radial(rl_renderable_particles_instance* instance_p, const bool is_iterative, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    const rl_renderable_particles* renderable_p = instance_p->get_renderable_particles();
    int end = is_iterative ? instance_p->get_particle_iterative_list().size() : instance_p->get_particle_parametric_list().size();
    const bool local_space = renderable_p->get_local_space_emission();
    int index = 0;
    int step = 1;
    if (renderable_p->get_is_reverse_render_order())
    {
        step = -1;
        index = end - 1;
        end = -1;
    }
    if (!is_iterative)
    {
        for (; index != end; index += step)
        {
            const rl_particle_parametric& particle = instance_p->get_particle_parametric_list()[index];
            parametric_vertex vertex;
            vertex.position = particle.position;
            vertex.velocity = particle.velocity;
            vertex.rotation_angle = particle.rotation_angle;
            vertex.time_created = particle.time_created;
            vertex.random.x = particle.random.x;
            vertex.random.y = particle.random.y;
            vertex.random.z = particle.random.z;
            vertex.random.w = particle.random.w;
            vertex.lifetime = particle.lifetime;
            parametric_vertex* vertices_p = (parametric_vertex*)vertex_p;
            for (int i = 0; i < 4; i++)
            {
                vertices_p[i] = vertex;
            }
            num_verts += 4;
            vertex_p += current_vertex_stride * 4;
            num_prims += 6;
        }
        return;
    }
    for (; index != end; index += step)
    {
        const rl_particle_iterative& particle = instance_p->get_particle_iterative_list()[index];
        radial_add_particle(particle, particle.rotation_angle, particle.position, particle.current_color, particle.current_size, renderable_p->get_is_rotation_enabled(), local_space, instance_p, vertex_p, num_verts, num_prims, current_vertex_stride);
    }
}
