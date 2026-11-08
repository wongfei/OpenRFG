// Original: volition/render_lib/code/rl_particle_filler_oriented.cpp
//
// The oriented filler of rl_particle_renderer (VFX.md FX2): quads stretched along a direction (the particle's velocity;
// for a tail point, the step from the previous point). Corner uvs (0,0), (0,1), (1,0), (1,1).

#include "volition/render_lib/code/rl_particle_renderer.h"

#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"

static unsigned int oriented_pack_color(float red, float green, float blue, float alpha)
{
    // Magic numbers: 0..1 to 0..255, rounded.
    return ((((unsigned int)(int)(alpha * 255.0f + 0.5f) & 0xFF) << 8 | ((unsigned int)(int)(red * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(green * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(blue * 255.0f + 0.5f) & 0xFF);
}

// A local space direction (no translation) into the world.
static vector oriented_rotate(const rl_renderable_particles_instance* instance_p, const vector& direction)
{
    const matrix& rotation = instance_p->get_local_emission_rotation();
    vector result;
    result.x = rotation.rvec.x * direction.x + rotation.uvec.x * direction.y + rotation.fvec.x * direction.z;
    result.y = rotation.rvec.y * direction.x + rotation.uvec.y * direction.y + rotation.fvec.y * direction.z;
    result.z = rotation.rvec.z * direction.x + rotation.uvec.z * direction.y + rotation.fvec.z * direction.z;
    return result;
}

static vector oriented_local_to_world(const rl_renderable_particles_instance* instance_p, const vector& position)
{
    const vector rotated = oriented_rotate(instance_p, position);
    const vector& origin = instance_p->get_local_emission_position();
    return vector(rotated.x + origin.x, origin.y + rotated.y, origin.z + rotated.z);
}

// The four corners of an oriented quad.
static void oriented_write(rl_particle_renderer::oriented_vertex vertex, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    rl_particle_renderer::oriented_vertex* vertices_p = (rl_particle_renderer::oriented_vertex*)vertex_p;
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

// 0x004EC270 (:19)
void rl_particle_renderer::oriented_add_particle(const rl_particle_iterative& particle, const vector& position_in, const rl_color_float& current_color, float current_width, float current_length, const vector& velocity_in, bool local_space, rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    oriented_vertex vertex;
    vertex.pos = local_space ? oriented_local_to_world(instance_p, position_in) : position_in;
    vertex.velocity = local_space ? oriented_rotate(instance_p, velocity_in) : velocity_in;
    vertex.rgba = oriented_pack_color(current_color.red, current_color.green, current_color.blue, current_color.alpha);
    vertex.saturation = particle.current_color_saturation;
    vertex.current_width = current_width;
    vertex.current_length = current_length;
    vertex.additive_key = particle.current_additive_key;
    vertex.anim_time = get_uv_anim_time(instance_p, particle);
    oriented_write(vertex, vertex_p, num_verts, num_prims, current_vertex_stride);
}

// 0x004EC8B0 (:74): tail points first (alpha scaled along the tail), then the particle; parametric particles write the
// particle to all four corners.
void rl_particle_renderer::filler_oriented(rl_renderable_particles_instance* instance_p, const bool is_iterative, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    const rl_renderable_particles* renderable_p = instance_p->get_renderable_particles();
    int end = is_iterative ? instance_p->get_particle_iterative_list().size() : instance_p->get_particle_parametric_list().size();
    const int tail_length = renderable_p->get_tail_length();
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
        if (0 < tail_length)
        {
            const int num_tails = particle.get_particle_tail_list_size();
            const rl_particle_base::tail_data* tails_p = particle.get_particle_tail_list();
            const float tail_step = 1.0f / (float)tail_length;
            float tail_fade = tail_step;
            vector previous = particle.position;
            for (int i = 0; i < num_tails; i++)
            {
                const rl_particle_base::tail_data& tail = tails_p[i];
                const float alpha = tail.current_color.alpha * tail_fade;
                tail_fade = tail_fade + tail_step;
                const vector direction(tail.position.x - previous.x, tail.position.y - previous.y, tail.position.z - previous.z);
                previous = tail.position;
                const rl_color_float color(tail.current_color.red, tail.current_color.green, tail.current_color.blue, alpha);
                oriented_add_particle(particle, tail.position, color, tail.current_size, tail.current_length, direction, local_space, instance_p, vertex_p, num_verts, num_prims, current_vertex_stride);
            }
        }
        oriented_add_particle(particle, particle.position, particle.current_color, particle.current_size, particle.current_length, particle.velocity, local_space, instance_p, vertex_p, num_verts, num_prims, current_vertex_stride);
    }
}
