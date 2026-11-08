// Original: volition/audio_lib/code/audiolib_fov.cpp
//
// The listener's field of view (SFX.md SX2): 26 raycast directions around the listener (up, down, and rings of 8
// horizontal, up and down), cast by the game (gameaudio, audiolib_listener_raycast). They give the occlusion of a 3D
// sound (collisions near the directions closest to the sound, weighted by alignment and material, plus an outdoor or
// indoor share of the open field of view) and the size and shape of the room around the listener (the reverb type).
// Written from the disassembly where the decompile is garbled; float rule as audiolib_3d_calculate.cpp. Compiled in
// audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_internal.h"

#include "volition/vlib/code/math/vector3.h"

#include <math.h>
#include <string.h>

// ---- public globals (rfg.pdb)
// The fixed directions of the 26 raycasts (y up; the horizontal ring is the up and down rings' base).
vector audiolib_raycast_direction_vectors[26] = { // 0x014F9B20
    vector(0.0f, 1.0f, 0.0f), vector(0.0f, -1.0f, 0.0f),
    vector(0.0f, 0.0f, 1.0f), vector(0.707f, 0.0f, 0.707f), vector(1.0f, 0.0f, 0.0f), vector(0.707f, 0.0f, -0.707f),
    vector(0.0f, 0.0f, -1.0f), vector(-0.707f, 0.0f, -0.707f), vector(-1.0f, 0.0f, 0.0f), vector(-0.707f, 0.0f, 0.707f),
    vector(0.0f, 0.707f, 1.0f), vector(0.707f, 0.707f, 0.707f), vector(1.0f, 0.707f, 0.0f), vector(0.707f, 0.707f, -0.707f),
    vector(0.0f, 0.707f, -1.0f), vector(-0.707f, 0.707f, -0.707f), vector(-1.0f, 0.707f, 0.0f), vector(-0.707f, 0.707f, 0.707f),
    vector(0.0f, -0.353f, 1.0f), vector(0.707f, -0.353f, 0.707f), vector(1.0f, -0.353f, 0.0f), vector(0.707f, -0.353f, -0.707f),
    vector(0.0f, -0.353f, -1.0f), vector(-0.707f, -0.353f, -0.707f), vector(-1.0f, -0.353f, 0.0f), vector(-0.707f, -0.353f, 0.707f),
};

// ---- file-static globals
// Room volume thresholds (height times the two longest spans).
static float audiolib_room_volume_small = 450.0f; // 0x014F91A8
static float audiolib_room_volume_large = 800.0f; // 0x014F91AC
static float audiolib_room_volume_huge = 2000.0f; // 0x014F91B0
static float audiolib_room_volume_canyon = 7000.0f; // 0x014F91B4
static float raycast_up_diff_percent_acceptable = 0.3f; // 0x014F91B8
static float raycast_down_diff_percent_acceptable = 0.6f; // 0x014F91BC
// The neighbours of each direction (0xFF ends a list).
static unsigned char audiolib_raycast_surrounding_directions[26][9] = {0, 0x11, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 1, 0x19, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 2, 0x19, 9, 0x11, 0x12, 0xA, 0x13, 3, 0xB, 3, 0x12, 2, 0xA, 0x13, 0xB, 0x14, 4, 0xC, 4, 0x13, 3, 0xB, 0x14, 0xC, 0x15, 5, 0xD, 5, 0x14, 4, 0xC, 0x15, 0xD, 0x16, 6, 0xE, 6, 0x15, 5, 0xD, 0x16, 0xE, 0x17, 7, 0xF, 7, 0x16, 6, 0xE, 0x17, 0xF, 0x18, 8, 0x10, 8, 0x17, 7, 0xF, 0x18, 0x10, 0x19, 9, 0x11, 9, 0x18, 8, 0x10, 0x19, 0x11, 0x12, 2, 0xA, 0xA, 0, 0x11, 9, 2, 0xB, 3, 0xFF, 0xFF, 0xB, 0, 0xA, 2, 3, 0xC, 4, 0xFF, 0xFF, 0xC, 0, 0xB, 3, 4, 0xD, 5, 0xFF, 0xFF, 0xD, 0, 0xC, 4, 5, 0xE, 6, 0xFF, 0xFF, 0xE, 0, 0xD, 5, 6, 0xF, 7, 0xFF, 0xFF, 0xF, 0, 0xE, 6, 7, 0x10, 8, 0xFF, 0xFF, 0x10, 0, 0xF, 7, 8, 0x11, 9, 0xFF, 0xFF, 0x11, 0, 0x10, 8, 0xA, 2, 0xFF, 0xFF, 0, 0x12, 1, 9, 0x19, 2, 3, 0x13, 0xFF, 0xFF, 0x13, 1, 2, 0x12, 3, 4, 0x14, 0xFF, 0xFF, 0x14, 1, 3, 0x13, 4, 5, 0x15, 0xFF, 0xFF, 0x15, 1, 4, 0x14, 5, 6, 0x16, 0xFF, 0xFF, 0x16, 1, 5, 0x15, 6, 7, 0x17, 0xFF, 0xFF, 0x17, 1, 6, 0x16, 7, 8, 0x18, 0xFF, 0xFF, 0x18, 1, 7, 0x17, 8, 9, 0x19, 0xFF, 0xFF, 0x19, 1, 8, 0x18, 9, 2, 0x12, 0xFF, 0xFF}; // 0x014F91C0
// audiolib_thread_info Stream_thread_info (0x014F9C60) is used here too; placed in audiolib.cpp.
// bool Audio_fov_Inited (0x01A789C5) is used here too; placed in audiolib.cpp.

// pi as rfg.exe has it (a float, used in double).
static const double Audiolib_fov_pi = (double)3.1415927f;

// One occluding collision near the sound's direction (a 0x14 record on the stack in rfg.exe).
struct audiolib_fov_occluder
{
    int direction; // 0x00
    float distance; // 0x04
    float alignment; // 0x08
    float weight; // 0x0C
    int material; // 0x10
};

// 0x00487B40 (:402), dir in EAX; the result in XMM0
static float audiolib_figure_raycast_distance(AUDIOLIB_RAYCAST_DIRECTION dir)
{
    // The distance along a horizontal direction, from its own ray and the ones above (+8) and below (+16) it.
    const audiolib_raycast_data& level = Audiolib_state.listener_raycasts[dir];
    const audiolib_raycast_data& up = Audiolib_state.listener_raycasts[dir + 8];
    const audiolib_raycast_data& down = Audiolib_state.listener_raycasts[dir + 16];
    if (!level.valid)
    {
        // Without the level ray: the mean of the others, or one of them with 22.5 m.
        if (up.valid)
        {
            if (down.valid)
            {
                return (float)(((double)up.distances[0] - (double)down.distances[0]) * 0.5 + (double)down.distances[0]);
            }
            return (float)(((double)up.distances[0] + 22.5) * 0.5);
        }
        if (down.valid)
        {
            return (float)(((double)down.distances[0] + 22.5) * 0.5);
        }
        return -1.0f;
    }
    const float distance = level.distances[0];
    const float up_threshold = distance * raycast_up_diff_percent_acceptable;
    const float down_threshold = distance * raycast_down_diff_percent_acceptable;
    float up_diff = -1.0f;
    float down_diff = -1.0f;
    if (up.valid)
    {
        up_diff = fabsf(up.distances[0] - distance);
    }
    if (down.valid)
    {
        down_diff = fabsf(down.distances[0] - distance);
    }
    // A neighbour that differs by more than its share replaces the level ray (both: their mean).
    if (up_diff > 0.0f)
    {
        if (down_diff > 0.0f)
        {
            if (up_diff > up_threshold && down_diff > down_threshold)
            {
                return (float)(((double)down.distances[0] + (double)up.distances[0]) * 0.5);
            }
        }
        else if (up_diff > up_threshold)
        {
            return up.distances[0];
        }
    }
    else if (down_diff > 0.0f && down_diff > down_threshold)
    {
        return down.distances[0];
    }
    return distance;
}

// 0x00487D00 (:461)
void audiolib_internal_setup_fov_from_settings_to_audiolib_state()
{
    for (int i = 0; i < 26; ++i)
    {
        audiolib_raycast_data& raycast = Audiolib_state.listener_raycasts[i];
        raycast.valid = false;
        vector direction;
        if (i == AUDIOLIB_RAYCAST_DIRECTION_UP)
        {
            raycast.weight = Audiolib_settings.fov_weights.up_weight;
            direction = vector(0.0f, 1.0f, 0.0f);
        }
        else if (i == AUDIOLIB_RAYCAST_DIRECTION_DOWN)
        {
            raycast.weight = Audiolib_settings.fov_weights.dn_weight;
            direction = vector(0.0f, -1.0f, 0.0f);
        }
        else
        {
            // The rings: pitched by the up/down angle (degrees), then turned 45 degrees per step from north.
            float pitch;
            if (i < AUDIOLIB_RAYCAST_DIRECTION_NORTH_UP)
            {
                raycast.weight = Audiolib_settings.fov_weights.hz_weight;
                pitch = 0.0f;
            }
            else if (i < AUDIOLIB_RAYCAST_DIRECTION_NORTH_DOWN)
            {
                raycast.weight = Audiolib_settings.fov_weights.up_angle_weight;
                pitch = Audiolib_settings.fov_weights.up_angle_angle;
            }
            else
            {
                raycast.weight = Audiolib_settings.fov_weights.dn_angle_weight;
                pitch = Audiolib_settings.fov_weights.dn_angle_angle;
            }
            direction = vector(0.0f, 0.0f, 1.0f);
            direction.rotate_around_x_axis((float)((double)-pitch * Audiolib_fov_pi / 180.0));
            const float heading = (float)((double)(float)((i - 2) & 7) * 45.0);
            direction.rotate_around_y_axis((float)((double)heading * Audiolib_fov_pi / 180.0));
        }
        direction.normalize();
        raycast.direction = direction;
        vector zero;
        zero.zero();
        raycast.num_collisions = 0;
        for (int c = 0; c < 3; ++c)
        {
            raycast.distances[c] = 0.0f;
            raycast.positions[c] = zero;
        }
    }
}

// 0x00487F70 (:582)
bool audiolib_fov_system_init()
{
    if (Audio_fov_Inited)
    {
        return false;
    }
    audiolib_internal_setup_fov_from_settings_to_audiolib_state();
    Audio_fov_Inited = true;
    return true;
}

// 0x00487F90 (:598)
void audiolib_fov_system_shutdown()
{
    Audio_fov_Inited = false;
}

// 0x00487FA0 (:606)
audiolib_result audiolib_internal_fov_get_direction_vector(AUDIOLIB_RAYCAST_DIRECTION raycast_dir, vector* direction)
{
    if (!Audio_fov_Inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (direction == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *direction = Audiolib_state.listener_raycasts[raycast_dir].direction;
    return AUDIOLIB_ERR_NONE;
}

// The weight a collision's material adds (materials 1-5), per ring: each step is added and stored as a float, from
// the material's entry in a chain. Horizontal: 0.1, 0.05, 0.05, 0.05, 0.02 (material 5 enters at the first, 1 at the
// last). Up: 0.2, 0.1, 0.1, 0.05, 0.05. Down: as up, but material 5 zeroes the weight.
static float audiolib_fov_material_weight(float weight, int direction, int material)
{
    static const float horizontal_steps[5] = {0.1f, 0.05f, 0.05f, 0.05f, 0.02f};
    static const float vertical_steps[5] = {0.2f, 0.1f, 0.1f, 0.05f, 0.05f};
    if (material < 1 || material > 5)
    {
        return weight;
    }
    const bool down = direction == AUDIOLIB_RAYCAST_DIRECTION_DOWN || direction >= AUDIOLIB_RAYCAST_DIRECTION_NORTH_DOWN;
    const bool up = !down && (direction == AUDIOLIB_RAYCAST_DIRECTION_UP || direction >= AUDIOLIB_RAYCAST_DIRECTION_NORTH_UP);
    if (down && material == 5)
    {
        return 0.0f;
    }
    const float* steps = up || down ? vertical_steps : horizontal_steps;
    for (int step = 5 - material; step < 5; ++step)
    {
        weight = (float)((double)weight + (double)steps[step]);
    }
    return weight;
}

// 0x0048F280 (:634)
void audiolib_internal_fov_compute_occlusion(audiolib_play_data* play)
{
    audiolib_fov_occluder occluders[26];
    memset(occluders, 0, 0x208);
    // Collisions closer than the sound count: up to dist_min the whole distance less dist_min scaled by how far
    // past it the sound is, 90% for a sound inside dist_min.
    const float dist = play->dist;
    const float dist_min = play->dist_min;
    float limit;
    if (dist_min <= dist)
    {
        const float inside = dist_min / dist;
        limit = (float)((double)dist - (1.0 - (double)inside) * (double)dist_min);
    }
    else
    {
        limit = (float)((double)dist * (double)0.9f);
    }
    if ((play->params.flags & 1) == 0 || play->foley_data.do_not_apply_occlusion)
    {
        play->occlusion = 0.0f;
        return;
    }
    vector to_sound(play->curr_pos.x - Audiolib_state.listener_pos.x, play->curr_pos.y - Audiolib_state.listener_pos.y,
                    play->curr_pos.z - Audiolib_state.listener_pos.z);
    to_sound.normalize();
    // The alignment of every valid ray with the sound; the best aligned one.
    float alignments[26];
    float best_alignment = -1.0f;
    int best = -1;
    for (int i = 0; i < 26; ++i)
    {
        alignments[i] = -100000.0f;
        if (!Audiolib_state.listener_raycasts[i].valid)
        {
            continue;
        }
        const vector& d = audiolib_raycast_direction_vectors[i];
        const float alignment = (float)((double)d.y * (double)to_sound.y + (double)d.x * (double)to_sound.x + (double)d.z * (double)to_sound.z);
        alignments[i] = alignment;
        if (alignment > 0.0f && alignment > best_alignment)
        {
            best = i;
            best_alignment = alignment;
        }
    }
    if (best < 0)
    {
        play->occlusion = 0.0f;
        return;
    }
    float occlusion = 0.0f;
    int count = 0;
    // Within about 45 degrees (0.7) of the best ray.
    if (best < 26 && alignments[best] > 0.7f)
    {
        for (int k = 0; k < 9; ++k)
        {
            const unsigned int neighbour = audiolib_raycast_surrounding_directions[best][k];
            if (neighbour > 26)
            {
                break;
            }
            const float alignment = alignments[neighbour];
            if (alignment <= 0.7f)
            {
                continue;
            }
            const audiolib_raycast_data& raycast = Audiolib_state.listener_raycasts[neighbour];
            int collisions = Audiolib_settings.max_occlusions;
            if (raycast.num_collisions < collisions)
            {
                collisions = raycast.num_collisions;
            }
            for (int c = 0; c < collisions; ++c)
            {
                const float distance = raycast.distances[c];
                const int material = (int)raycast.materials[c];
                // Collisions within 45 m, with a material, in front of the sound.
                if (!raycast.valid || !(distance < 45.0f) || material == 0 || !(distance > 0.0f) || !(distance < limit))
                {
                    continue;
                }
                audiolib_fov_occluder& occluder = occluders[count];
                occluder.direction = (int)neighbour;
                occluder.distance = distance;
                occluder.material = material;
                occluder.alignment = 0.0f;
                // 0.1, more for better alignment (cos 30, 15 and 10 degrees), more again within 2 m.
                float weight = (float)(0.0 + (double)0.1f);
                if (alignment > 0.866f)
                {
                    weight = (float)((double)weight + (double)0.1f);
                    if (alignment > 0.9659f)
                    {
                        weight = (float)((double)weight + (double)0.1f);
                        if (alignment > 0.9848f && distance < 2.0f)
                        {
                            const float close = (float)fabs((2.0 - (double)distance) * (double)0.6f);
                            weight = (float)((double)close + (double)weight);
                        }
                    }
                }
                occluder.weight = audiolib_fov_material_weight(weight, (int)neighbour, material);
                occluder.alignment = (float)((double)fabsf(alignment) + 0.0);
                ++count;
            }
        }
        if (count > 0)
        {
            float sum = 0.0f;
            for (int i = 0; i < count; ++i)
            {
                sum = (float)((double)occluders[i].alignment * (double)occluders[i].weight + (double)sum);
            }
            occlusion = (float)((double)sum / (double)(float)count);
            if (occlusion - 0.0f < 0.0f)
            {
                occlusion = 0.0f;
            }
            if (0.0f <= occlusion - 5.0f)
            {
                occlusion = 5.0f;
            }
        }
    }
    // Some of the open field of view: a fifth outdoors (above 0.01), half indoors (above 0.1).
    if (Audiolib_settings.minimum_reverb_fov < Audiolib_state.listener_fov)
    {
        switch (Audiolib_state.room_dimensions.reverb_type)
        {
        case AUDIOLIB_REVERB_ROOM_TYPE_ARENA:
        case AUDIOLIB_REVERB_ROOM_TYPE_HANGAR:
        case AUDIOLIB_REVERB_ROOM_TYPE_FOREST:
        case AUDIOLIB_REVERB_ROOM_TYPE_CITY:
        case AUDIOLIB_REVERB_ROOM_TYPE_MOUNTAINS:
        case AUDIOLIB_REVERB_ROOM_TYPE_QUARRY:
        case AUDIOLIB_REVERB_ROOM_TYPE_PLAIN:
        case AUDIOLIB_REVERB_ROOM_TYPE_PARKINGLOT:
        case AUDIOLIB_REVERB_ROOM_TYPE_PLATE:
            if (occlusion > 0.01f)
            {
                occlusion = (float)((double)Audiolib_state.listener_fov * (double)0.2f + (double)occlusion);
            }
            break;
        default:
            if ((double)occlusion > (double)0.1f)
            {
                occlusion = (float)((double)Audiolib_state.listener_fov * 0.5 + (double)occlusion);
            }
            break;
        }
    }
    if (occlusion - 0.0f < 0.0f)
    {
        occlusion = 0.0f;
    }
    if (0.0f <= occlusion - 5.0f)
    {
        occlusion = 5.0f;
    }
    play->occlusion = (float)((double)occlusion + 0.0);
}

// 0x00487FE0 (:894)
void audiolib_internal_fov_determine_room_size_and_shape()
{
    audiolib_room_dimensions& room = Audiolib_state.room_dimensions;
    room.north_south = audiolib_figure_raycast_distance(AUDIOLIB_RAYCAST_DIRECTION_NORTH);
    room.north_south = room.north_south + audiolib_figure_raycast_distance(AUDIOLIB_RAYCAST_DIRECTION_SOUTH);
    room.north_east_south_west = audiolib_figure_raycast_distance(AUDIOLIB_RAYCAST_DIRECTION_NORTH_EAST);
    room.north_east_south_west = room.north_east_south_west + audiolib_figure_raycast_distance(AUDIOLIB_RAYCAST_DIRECTION_SOUTH_WEST);
    room.east_west = audiolib_figure_raycast_distance(AUDIOLIB_RAYCAST_DIRECTION_EAST);
    room.east_west = room.east_west + audiolib_figure_raycast_distance(AUDIOLIB_RAYCAST_DIRECTION_WEST);
    // Original: the north-west/south-east span measures north-east and south-west again.
    room.north_west_south_east = audiolib_figure_raycast_distance(AUDIOLIB_RAYCAST_DIRECTION_NORTH_EAST);
    room.north_west_south_east = room.north_west_south_east + audiolib_figure_raycast_distance(AUDIOLIB_RAYCAST_DIRECTION_SOUTH_WEST);
    if (Audiolib_state.listener_raycasts[0].valid && Audiolib_state.listener_raycasts[1].valid)
    {
        room.height = Audiolib_state.listener_raycasts[1].distances[0] + Audiolib_state.listener_raycasts[0].distances[0];
    }
    else
    {
        room.height = 40.0f;
    }
    // A span 1.7 times another makes it a rectangle; low rooms are pipes, 40 m and higher canyons.
    AUDIOLIB_ROOM_SHAPE shape = AUDIOLIB_ROOM_SHAPE_SQUARE;
    room.shape = shape;
    const bool long_room = (room.east_west < room.north_south && room.east_west * 1.7f < room.north_south) ||
                           (room.north_south < room.east_west && room.north_south * 1.7f < room.east_west) ||
                           (room.north_west_south_east < room.north_east_south_west && room.north_west_south_east * 1.7f < room.north_east_south_west) ||
                           (room.north_east_south_west < room.north_west_south_east && room.north_east_south_west * 1.7f < room.north_west_south_east);
    bool check_canyon = true;
    if (long_room)
    {
        shape = AUDIOLIB_ROOM_SHAPE_RECTANGLE;
        room.shape = shape;
        if (room.height < 3.0f)
        {
            shape = AUDIOLIB_ROOM_SHAPE_PIPE;
            room.shape = shape;
            check_canyon = false;
        }
    }
    else if (room.height < 3.0f)
    {
        check_canyon = false;
    }
    if (check_canyon && 40.0f <= room.height)
    {
        shape = AUDIOLIB_ROOM_SHAPE_CANYON;
        room.shape = shape;
    }
    const float longest_axis = room.north_south - room.east_west < 0.0f ? room.east_west : room.north_south;
    const float longest_diagonal = 0.0f <= room.north_east_south_west - room.north_west_south_east ? room.north_east_south_west : room.north_west_south_east;
    room.volume = longest_axis * room.height * longest_diagonal;
    room.size = AUDIOLIB_ROOM_SIZE_SMALL;
    if (audiolib_room_volume_small <= room.volume)
    {
        room.size = AUDIOLIB_ROOM_SIZE_MEDIUM;
        if (audiolib_room_volume_large <= room.volume)
        {
            if (audiolib_room_volume_huge <= room.volume)
            {
                if (audiolib_room_volume_canyon <= room.volume)
                {
                    // Too big to be a room: the reverb type stays.
                    room.valid = false;
                    room.size = AUDIOLIB_ROOM_SIZE_INVALID;
                    return;
                }
                room.size = AUDIOLIB_ROOM_SIZE_HUGE;
            }
            else
            {
                room.size = AUDIOLIB_ROOM_SIZE_LARGE;
            }
        }
    }
    switch (shape)
    {
    case AUDIOLIB_ROOM_SHAPE_SQUARE:
    case AUDIOLIB_ROOM_SHAPE_RECTANGLE:
        if (room.size == AUDIOLIB_ROOM_SIZE_MEDIUM)
        {
            room.reverb_type = AUDIOLIB_REVERB_ROOM_TYPE_STONEROOM;
        }
        else if (room.size == AUDIOLIB_ROOM_SIZE_LARGE)
        {
            room.reverb_type = AUDIOLIB_REVERB_ROOM_TYPE_LARGEHALL;
        }
        else if (room.size == AUDIOLIB_ROOM_SIZE_HUGE)
        {
            room.reverb_type = AUDIOLIB_REVERB_ROOM_TYPE_QUARRY;
        }
        else
        {
            room.reverb_type = AUDIOLIB_REVERB_ROOM_TYPE_MEDIUMROOM;
        }
        break;
    case AUDIOLIB_ROOM_SHAPE_HALLWAY:
        room.reverb_type = AUDIOLIB_REVERB_ROOM_TYPE_HALLWAY;
        break;
    case AUDIOLIB_ROOM_SHAPE_PIPE:
        room.reverb_type = AUDIOLIB_REVERB_ROOM_TYPE_SEWERPIPE;
        break;
    case AUDIOLIB_ROOM_SHAPE_CANYON:
        room.reverb_type = AUDIOLIB_REVERB_ROOM_TYPE_QUARRY;
        break;
    default:
        room.reverb_type = AUDIOLIB_REVERB_ROOM_TYPE_GENERIC;
        break;
    }
    room.valid = true;
}
