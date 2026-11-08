// Original: volition/render_lib/code/rl_particle_filler_billboard.cpp
//
// The billboard filler of rl_particle_renderer (VFX.md FX2): four vertices per particle (and per tail point) with
// corner uvs (0,0), (0,1), (1,0), (1,1); the vertex shader expands them to camera-facing quads. Parametric particles
// write the particle itself to all four corners (the shader evaluates its curves).

#include "volition/render_lib/code/rl_particle_renderer.h"

#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"

// A color as a D3DCOLOR (ARGB bytes, each channel rounded).
static unsigned int billboard_pack_color(float red, float green, float blue, float alpha)
{
    // Magic numbers: 0..1 to 0..255, rounded.
    return ((((unsigned int)(int)(alpha * 255.0f + 0.5f) & 0xFF) << 8 | ((unsigned int)(int)(red * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(green * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(blue * 255.0f + 0.5f) & 0xFF);
}

// A local space position into the world by the instance's emission transform.
static vector billboard_local_to_world(const rl_renderable_particles_instance* instance_p, const vector& position)
{
    const matrix& rotation = instance_p->get_local_emission_rotation();
    const vector& origin = instance_p->get_local_emission_position();
    vector result;
    result.x = rotation.rvec.x * position.x + rotation.uvec.x * position.y + rotation.fvec.x * position.z + origin.x;
    result.y = origin.y + rotation.rvec.y * position.x + rotation.uvec.y * position.y + rotation.fvec.y * position.z;
    result.z = origin.z + rotation.rvec.z * position.x + rotation.uvec.z * position.y + rotation.fvec.z * position.z;
    return result;
}

// The four corners of one parametric particle (the parametric vertex is the particle's base data).
static void billboard_write_parametric(const rl_particle_parametric& particle, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    rl_particle_renderer::parametric_vertex vertex;
    vertex.position = particle.position;
    vertex.velocity = particle.velocity;
    vertex.rotation_angle = particle.rotation_angle;
    vertex.time_created = particle.time_created;
    vertex.random.x = particle.random.x;
    vertex.random.y = particle.random.y;
    vertex.random.z = particle.random.z;
    vertex.random.w = particle.random.w;
    vertex.lifetime = particle.lifetime;
    rl_particle_renderer::parametric_vertex* vertices_p = (rl_particle_renderer::parametric_vertex*)vertex_p;
    for (int i = 0; i < 4; i++)
    {
        vertices_p[i] = vertex;
    }
    num_verts += 4;
    vertex_p += current_vertex_stride * 4;
    // Magic number: two triangles of the shared quad index buffer.
    num_prims += 6;
}

// 0x004E9F90 (:19): one billboard; saturation, additive key and animation come from the particle.
void rl_particle_renderer::billboard_add_particle(const rl_particle_iterative& particle, float rotation_angle, const vector& position_in, const rl_color_float& current_color, float current_width, float current_length, bool rotation_enabled, bool local_space, rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    const vector position = local_space ? billboard_local_to_world(instance_p, position_in) : position_in;
    billboard_vertex vertex;
    vertex.pos = position;
    vertex.rgba = billboard_pack_color(current_color.red, current_color.green, current_color.blue, current_color.alpha);
    vertex.saturation = particle.current_color_saturation;
    vertex.rotation = rotation_angle;
    vertex.current_width = current_width;
    vertex.current_length = current_length;
    vertex.additive_key = particle.current_additive_key;
    vertex.anim_time = get_uv_anim_time(instance_p, particle);
    billboard_vertex* vertices_p = (billboard_vertex*)vertex_p;
    // The corners: uvs (0,0), (0,1), (1,0), (1,1).
    for (int i = 0; i < 4; i++)
    {
        vertices_p[i] = vertex;
        vertices_p[i].uvs.x = (i & 2) != 0 ? 1.0f : 0.0f;
        vertices_p[i].uvs.y = (i & 1) != 0 ? 1.0f : 0.0f;
    }
    num_verts += 4;
    vertex_p += current_vertex_stride * 4;
    num_prims += 6;
}

// 0x004EA410 (:78)
void rl_particle_renderer::billboard_add_particle_parametric(const rl_particle_parametric& particle, const rl_renderable_particles_instance* emitter_inst_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    billboard_write_parametric(particle, vertex_p, num_verts, num_prims, current_vertex_stride);
}

// 0x004EA510 (:115): the particles in order (or reversed); an iterative particle's tail points come first, each with
// the particle's alpha scaled by its position along the tail (1/tail_length, 2/tail_length, ...).
void rl_particle_renderer::filler_billboard(rl_renderable_particles_instance* instance_p, const bool is_iterative, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    const rl_renderable_particles* renderable_p = instance_p->get_renderable_particles();
    int end = is_iterative ? instance_p->get_particle_iterative_list().size() : instance_p->get_particle_parametric_list().size();
    const int tail_length = renderable_p->get_tail_length();
    const bool local_space = renderable_p->get_local_space_emission();
    int index = 0;
    int step = 1;
    if (renderable_p->get_is_reverse_render_order())
    {
        index = end - 1;
        step = -1;
        end = -1;
    }
    if (!is_iterative)
    {
        for (; index != end; index += step)
        {
            billboard_write_parametric(instance_p->get_particle_parametric_list()[index], vertex_p, num_verts, num_prims, current_vertex_stride);
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
            for (int i = 0; i < num_tails; i++)
            {
                const rl_particle_base::tail_data& tail = tails_p[i];
                const rl_color_float color(tail.current_color.red, tail.current_color.green, tail.current_color.blue, tail.current_color.alpha * tail_fade);
                billboard_add_particle(particle, tail.rotation_angle, tail.position, color, tail.current_size, tail.current_length, false, local_space, instance_p, vertex_p, num_verts, num_prims, current_vertex_stride);
                tail_fade = tail_fade + tail_step;
            }
        }
        billboard_add_particle(particle, particle.rotation_angle, particle.position, particle.current_color, particle.current_size, particle.current_length, false, local_space, instance_p, vertex_p, num_verts, num_prims, current_vertex_stride);
    }
}
