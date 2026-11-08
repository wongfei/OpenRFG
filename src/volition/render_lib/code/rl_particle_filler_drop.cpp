// Original: volition/render_lib/code/rl_particle_filler_drop.cpp
//
// The drop filler of rl_particle_renderer (VFX.md FX2): a streak through the particle and its tail points (newest
// first, padded to nine points with the last one), as a strip of ten vertex pairs. Each pair carries the direction to
// the previous point (vel) and the side offset (v_trans.x = +-size, v_trans.y = size at the head, 0 after); u runs
// along the streak by 1 / (tail_length / 100) / 9 per segment.

#include "volition/render_lib/code/rl_particle_renderer.h"

#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"

// The streak points (the particle and up to eight tail points).
static const int DROP_POINT_COUNT = 9;

static unsigned int drop_pack_color(const rl_color_float& color)
{
    // Magic numbers: 0..1 to 0..255, rounded.
    return ((((unsigned int)(int)(color.alpha * 255.0f + 0.5f) & 0xFF) << 8 | ((unsigned int)(int)(color.red * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(color.green * 255.0f + 0.5f) & 0xFF)) << 8 | ((unsigned int)(int)(color.blue * 255.0f + 0.5f) & 0xFF);
}

static vector drop_local_to_world(const rl_renderable_particles_instance* instance_p, const vector& position)
{
    const matrix& rotation = instance_p->get_local_emission_rotation();
    const vector& origin = instance_p->get_local_emission_position();
    vector result;
    result.x = origin.x + rotation.rvec.x * position.x + rotation.uvec.x * position.y + rotation.fvec.x * position.z;
    result.y = origin.y + rotation.rvec.y * position.x + rotation.uvec.y * position.y + rotation.fvec.y * position.z;
    result.z = origin.z + rotation.rvec.z * position.x + rotation.uvec.z * position.y + rotation.fvec.z * position.z;
    return result;
}

// 0x004EB160 (:20)
void rl_particle_renderer::drop_add_particle(const rl_particle_iterative& particle, float rotation_angle, const vector& position_in, const rl_color_float& current_color, float current_size, bool rotation_enabled, bool local_space, int tail_length, rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    vector points[DROP_POINT_COUNT];
    points[0] = local_space ? drop_local_to_world(instance_p, position_in) : position_in;
    const int num_tails = particle.get_particle_tail_list_size();
    const rl_particle_base::tail_data* tails_p = particle.get_particle_tail_list();
    int num_points = 1;
    for (int i = num_tails - 1; -1 < i; i--)
    {
        points[num_points] = local_space ? drop_local_to_world(instance_p, tails_p[i].position) : tails_p[i].position;
        num_points++;
    }
    for (; num_points < DROP_POINT_COUNT; num_points++)
    {
        points[num_points] = points[num_points - 1];
    }

    drop_vertex side_a;
    side_a.rgba = drop_pack_color(current_color);
    side_a.saturation = particle.current_color_saturation;
    side_a.additive_key = particle.current_additive_key;
    side_a.anim_time = get_uv_anim_time(instance_p, particle);
    side_a.uvs.x = 0.0f;
    side_a.uvs.y = 0.0f;
    side_a.v_trans.x = current_size;
    side_a.v_trans.y = current_size;
    side_a.pos = points[0];
    side_a.vel.x = points[0].x - points[1].x;
    side_a.vel.y = points[0].y - points[1].y;
    side_a.vel.z = points[0].z - points[1].z;
    drop_vertex side_b = side_a;
    side_b.uvs.y = 1.0f;
    side_b.v_trans.x = -current_size;

    drop_vertex* vertices_p = (drop_vertex*)vertex_p;
    vertices_p[0] = side_a;
    vertices_p[1] = side_b;

    // Magic numbers: the u step per segment (tail_length in hundredths, nine segments).
    const float u_step = (1.0f / ((float)tail_length / 100.0f)) * 0.11111111f;
    float u = u_step + 0.0f;
    if (0.0f <= u - 1.0f)
    {
        u = 1.0f;
    }
    side_a.v_trans.y = 0.0f;
    side_b.v_trans.y = 0.0f;
    side_a.uvs.x = u;
    side_b.uvs.x = u;
    vertices_p[2] = side_a;
    vertices_p[3] = side_b;
    for (int point = 1; point < DROP_POINT_COUNT; point++)
    {
        const vector& previous = point == 1 ? points[0] : points[point - 1];
        side_a.pos = points[point];
        side_a.vel.x = previous.x - points[point].x;
        side_a.vel.y = previous.y - points[point].y;
        side_a.vel.z = previous.z - points[point].z;
        u = u_step + u;
        side_a.uvs.x = u;
        side_b.pos = side_a.pos;
        side_b.vel = side_a.vel;
        side_b.uvs.x = u;
        vertices_p[point * 2 + 2] = side_a;
        vertices_p[point * 2 + 3] = side_b;
    }
    // Magic numbers: 20 vertices, 22 strip indices (with the degenerate joins between drops).
    num_verts += 0x14;
    vertex_p += current_vertex_stride * 0x14;
    num_prims += 0x16;
}

// 0x004EC190 (:203): drops are iterative only, in order (or reversed).
void rl_particle_renderer::filler_drop(rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride)
{
    const rl_renderable_particles* renderable_p = instance_p->get_renderable_particles();
    int end = instance_p->get_particle_iterative_list().size();
    const int tail_length = renderable_p->get_tail_length();
    int index = 0;
    int step = 1;
    if (renderable_p->get_is_reverse_render_order())
    {
        index = end - 1;
        step = -1;
        end = -1;
    }
    for (; index != end; index += step)
    {
        const rl_particle_iterative& particle = instance_p->get_particle_iterative_list()[index];
        drop_add_particle(particle, particle.rotation_angle, particle.position, particle.current_color, particle.current_size, renderable_p->get_is_rotation_enabled(), renderable_p->get_local_space_emission(), tail_length, instance_p, vertex_p, num_verts, num_prims, current_vertex_stride);
    }
}
