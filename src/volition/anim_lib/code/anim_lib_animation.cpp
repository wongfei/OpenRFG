// Original: volition/anim_lib/code/anim_lib_animation.cpp
//
// The animation table (Animations, looked up by file name through a hash table of handles), the trigger names and
// per-animation triggers, and the readers of the compressed key data: rotations (runs of 8-bit deltas over a 7-bit
// base, slerped between keys), translations (8-bit offsets over 16-bit run bases, lerped), bone weights and morph
// weights. Part of anim_lib_unity.cpp (player_minimal.md P2).
//
// Precision: rfg.exe evaluates these float expressions in double (CVTSS2SD / MULSD chains, rounded once when stored).
// The chains are written in double here, as the binary does; an expression that rounds once is the float operation.
// Not ported (not reached): find_animation_rotation / _scaling, interpolate_compressed_scale, the morph key helpers,
// get_compressed_translation_difference, get_animation_controller_weight, get_anim_displacement, get_heading,
// animlib_remove_anim_from_lookup_table, animlib_set_morph_set_for_animation and the system shutdown.
// The external interface is declared in anim_lib_internal.h.

#include "volition/anim_lib/code/anim_lib_internal.h"

#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/util/string_comp.h"

#include <math.h>
#include <string.h>

// Ticks per second of animation times; key times are stored in units of 80 ticks (1 / 60 s).
static const int ANIM_TICKS_PER_KEY_TIME = 80;
// (float)(1 / 16383) as rfg.exe holds it in double (__real@3f10004000000000): the rotation key scale.
static const double ROT_KEY_SCALE = 1.0 / 16384.0 + 1.0 / 268435456.0;
// (double)0.0125f (__real@3f899999a0000000): ticks to key time units.
static const double KEY_TIME_PER_TICK = (double)0.0125f;
// (double)(1 / 255.0f) (__real@3f70101020000000): the morph key scale.
static const double MORPH_KEY_SCALE = (double)(1.0f / 255.0f);

// ---- globals
anim_animation Animations[3875]; // 0x0398E338
int Num_animations; // 0x038BD6F0
int Animation_lookup_table[7750]; // 0x038CA4D8, anim handles by crc_stri(file name) % 7750, linear probing
int Animation_lookup_table_collisions; // 0x016F088C, the longest probe sequence + 1
static int offset_data[256]; // 0x038CA0D0, packed rotation key scalers (see anim_animation_system_init)
static int Num_trigger_types; // 0x03982B58
static anim_trigger Trigger_array[5850]; // 0x03982C60
static int Num_triggers; // 0x0398E330
static char Trigger_names[28][32]; // 0x039AC798

// ---- internal functions
// Defined in anim_lib_instance_manager.cpp (same unity file).
static void setup_bone_data(const anim_file* const amf, animlib_compressed_data_reader& bone_data);
static void setup_bone_weights(const anim_file* const amf, char* data_pointer, animlib_compressed_data_reader& bone_data, int num_bones);

// 0x00C891F0 (anim_lib_internal.h:153)
animlib_compressed_data_reader::animlib_compressed_data_reader()
{
    init_level = 0;
    bone_idx = -1;
}

static int get_key_time(const void* times, bool time_as_shorts, int index)
{
    return time_as_shorts ? ((const unsigned short*)times)[index] : ((const unsigned char*)times)[index];
}

// 0x00C89410 (byte times) and 0x00C89460 (short times); LTCG: num_keys EAX, keys on the stack, cur_time EDI,
// min_index EDX, max_index ESI. Binary search for the keys around cur_time; true when a key is exactly at cur_time
// (min_index == max_index).
static bool get_key_index(int num_keys, unsigned char* keys, int cur_time, int& min_index, int& max_index)
{
    min_index = 0;
    max_index = num_keys - 1;
    while (min_index < max_index - 1) {
        const int mid = (min_index + max_index) >> 1;
        const int key_time = keys[mid] * ANIM_TICKS_PER_KEY_TIME;
        if (key_time < cur_time) {
            min_index = mid;
        } else if (key_time > cur_time) {
            max_index = mid;
        } else {
            min_index = mid;
            max_index = mid;
            return true;
        }
    }
    return false;
}

// 0x00C89460 (:147)
static bool get_key_index(int num_keys, unsigned short* keys, int cur_time, int& min_index, int& max_index)
{
    min_index = 0;
    max_index = num_keys - 1;
    while (min_index < max_index - 1) {
        const int mid = (min_index + max_index) >> 1;
        const int key_time = keys[mid] * ANIM_TICKS_PER_KEY_TIME;
        if (key_time < cur_time) {
            min_index = mid;
        } else if (key_time > cur_time) {
            max_index = mid;
        } else {
            min_index = mid;
            max_index = mid;
            return true;
        }
    }
    return false;
}

// 0x00C894B0. The four 8-bit scalers of a rotation run (x in the low byte).
void get_rotation_key_scalers(afc_rotation_offset* offset, int& scalers)
{
    const unsigned char* b = (const unsigned char*)offset;
    scalers = offset_data[((((b[0] >> 7) * 2 | (b[1] >> 7)) * 2 | (b[2] >> 7)) * 2 | (b[3] >> 7)) << 4 | (b[4] >> 4)];
}

// The run of rotation key `key` and the key's position in it; each run holds count + 1 keys.
static int find_rotation_run(const afc_rotation_offset* offsets, int key, int& key_in_run)
{
    int run = 0;
    int k = key;
    int count = offsets[0].count;
    while (count < k) {
        k -= count + 1;
        run++;
        count = offsets[run].count;
    }
    key_in_run = k;
    return run;
}

// The quaternion of key `key`: the run's base (7 bits, << 8) plus the scaled sum of the run's deltas up to the key,
// in 16-bit arithmetic as the binary does, times ROT_KEY_SCALE.
static void decode_rotation_key(const afc_rotation_offset& offset, const afc_rotation_key* keys, int first_key, int key, int scalers, quaternion& q)
{
    short sx = 0;
    short sy = 0;
    short sz = 0;
    short sw = 0;
    for (int k = first_key; k <= key; k++) {
        sx = (short)(sx + keys[k].rotation.x);
        sy = (short)(sy + keys[k].rotation.y);
        sz = (short)(sz + keys[k].rotation.z);
        sw = (short)(sw + keys[k].rotation.w);
    }
    const short x = (short)(offset.x * 0x100 + (scalers & 0xFF) * sx);
    const short y = (short)(offset.y * 0x100 + ((scalers >> 8) & 0xFF) * sy);
    const short z = (short)(offset.z * 0x100 + ((scalers >> 16) & 0xFF) * sz);
    const short w = (short)(offset.w * 0x100 + ((unsigned int)scalers >> 24) * sw);
    q.x = (float)((double)x * ROT_KEY_SCALE);
    q.y = (float)((double)y * ROT_KEY_SCALE);
    q.z = (float)((double)z * ROT_KEY_SCALE);
    q.w = (float)((double)w * ROT_KEY_SCALE);
}

// The interpolation parameter between two key times (in double, rounded once).
static float key_time_parameter(int cur_time, int time0, int time1)
{
    return (float)(((double)(float)cur_time * KEY_TIME_PER_TICK - (double)(float)time0) / (double)(float)(time1 - time0));
}

// 0x00C89500; LTCG: cur_time and rotation on the stack, bone_data ECX.
static void interpolate_compressed_rotation(int cur_time, animlib_compressed_data_reader& bone_data, quaternion& rotation)
{
    const afc_rotation_key* keys = bone_data.rotation_keys;
    const afc_rotation_offset* offsets = bone_data.rotation_offsets;
    void* times = bone_data.rotation_times;
    const int num_keys = bone_data.num_rotation_keys;
    if (num_keys == 0) {
        rotation.make_identity();
        return;
    }
    int scalers;
    if (num_keys == 1) {
        scalers = offset_data[((((((const unsigned char*)offsets)[0] >> 7) * 2 | (((const unsigned char*)offsets)[1] >> 7)) * 2 | (((const unsigned char*)offsets)[2] >> 7)) * 2 | (((const unsigned char*)offsets)[3] >> 7)) << 4 | (((const unsigned char*)offsets)[4] >> 4)];
        decode_rotation_key(offsets[0], keys, 0, 0, scalers, rotation);
        return;
    }
    const bool time_as_shorts = bone_data.time_as_shorts;
    int min_index;
    int max_index;
    if (!time_as_shorts) {
        get_key_index(num_keys, (unsigned char*)times, cur_time, min_index, max_index);
    } else {
        get_key_index(num_keys, (unsigned short*)times, cur_time, min_index, max_index);
    }
    int in_run;
    const int min_run = find_rotation_run(offsets, min_index, in_run);
    get_rotation_key_scalers((afc_rotation_offset*)&offsets[min_run], scalers);
    if (min_index == max_index) {
        decode_rotation_key(offsets[min_run], keys, min_index - in_run, min_index, scalers, rotation);
        return;
    }
    quaternion q0;
    decode_rotation_key(offsets[min_run], keys, min_index - in_run, min_index, scalers, q0);
    const int max_run = find_rotation_run(offsets, max_index, in_run);
    if (max_run != min_run) {
        get_rotation_key_scalers((afc_rotation_offset*)&offsets[max_run], scalers);
    }
    quaternion q1;
    decode_rotation_key(offsets[max_run], keys, max_index - in_run, max_index, scalers, q1);

    const float t = key_time_parameter(cur_time, get_key_time(times, time_as_shorts, min_index), get_key_time(times, time_as_shorts, max_index));
    float dot = quaternion_dot(q0, q1);
    quaternion q1n;
    if (dot < 0.0f) {
        q1n = quaternion(-q1.x, -q1.y, -q1.z, -q1.w);
        dot = -dot;
    } else {
        q1n = q1;
    }
    float scale0;
    float scale1;
    if (0.05 < 1.0 - (double)dot) { // double constant 0.05
        // x87 code, every intermediate stored as float: sin is the CRT's (__CIsin)
        const float omega = fl_acos(dot);
        const float sin_omega = (float)sin((double)omega);
        const float inv_sin = (float)(1.0 / (double)sin_omega);
        const float a0 = (float)((1.0 - (double)t) * (double)omega);
        scale0 = (float)((double)(float)sin((double)a0) * (double)inv_sin);
        const float a1 = (float)((double)omega * (double)t);
        scale1 = (float)((double)(float)sin((double)a1) * (double)inv_sin);
    } else {
        scale0 = (float)(1.0 - (double)t);
        scale1 = t;
    }
    rotation.x = (float)((double)q1n.x * (double)scale1 + (double)scale0 * (double)q0.x);
    rotation.y = (float)((double)q1n.y * (double)scale1 + (double)scale0 * (double)q0.y);
    rotation.z = (float)((double)q1n.z * (double)scale1 + (double)scale0 * (double)q0.z);
    rotation.w = (float)((double)q1n.w * (double)scale1 + (double)scale0 * (double)q0.w);
}

// 0x00C89DD0; LTCG: cur_time and translation on the stack, bone_data ECX.
static void interpolate_compressed_translation(int cur_time, animlib_compressed_data_reader& bone_data, vector& translation)
{
    const int num_keys = bone_data.num_translation_keys;
    if (num_keys == 0) {
        translation.zero();
        return;
    }
    const int num_offsets = bone_data.num_translation_offsets;
    const afc_translation_key* keys = bone_data.translation_keys;
    const afc_translation_offset* offsets = bone_data.translation_offsets;
    if (num_keys == 1) {
        translation.x = (float)((double)(float)offsets[0].x * 0.015625 + (double)(float)keys[0].x / 4000.0);
        translation.y = (float)((double)(float)keys[0].y / 4000.0 + (double)(float)offsets[0].y * 0.015625);
        translation.z = (float)((double)(float)keys[0].z / 4000.0 + (double)(float)offsets[0].z * 0.015625);
        return;
    }
    const bool time_as_shorts = bone_data.time_as_shorts;
    void* times = bone_data.translation_times;
    int min_index;
    int max_index;
    if (!time_as_shorts) {
        get_key_index(num_keys, (unsigned char*)times, cur_time, min_index, max_index);
    } else {
        get_key_index(num_keys, (unsigned short*)times, cur_time, min_index, max_index);
    }
    // the run holding min_index
    int run = 0;
    int run_end = 0;
    while (run < num_offsets) {
        run_end += offsets[run].num_keys;
        if (min_index < run_end) {
            break;
        }
        run++;
    }
    const afc_translation_key& key0 = keys[min_index];
    if (min_index == max_index) {
        translation.x = (float)((double)(float)offsets[run].x * 0.015625 + (double)(float)key0.x / 4000.0);
        translation.y = (float)((double)(float)key0.y / 4000.0 + (double)(float)offsets[run].y * 0.015625);
        translation.z = (float)((double)(float)key0.z / 4000.0 + (double)(float)offsets[run].z * 0.015625);
        return;
    }
    const afc_translation_key& key1 = keys[max_index];
    const int run1 = max_index < run_end ? run : run + 1;
    const float t = key_time_parameter(cur_time, get_key_time(times, time_as_shorts, min_index), get_key_time(times, time_as_shorts, max_index));
    const float a = (float)(1.0 - (double)t);
    const float s1 = (float)((double)t * 0.015625);
    const float s0 = (float)((double)a * 0.015625);
    const float d0 = (float)((double)a / 4000.0);
    const float d1 = (float)((double)t / 4000.0);
    translation.x = (float)((double)(float)offsets[run].x * (double)s0 + (double)(float)key0.x * (double)d0 + (double)(float)offsets[run1].x * (double)s1 + (double)(float)key1.x * (double)d1);
    translation.y = (float)((double)(float)offsets[run].y * (double)s0 + (double)(float)key0.y * (double)d0 + (double)(float)offsets[run1].y * (double)s1 + (double)(float)key1.y * (double)d1);
    translation.z = (float)((double)(float)offsets[run].z * (double)s0 + (double)(float)key0.z * (double)d0 + (double)(float)offsets[run1].z * (double)s1 + (double)(float)key1.z * (double)d1);
}

// 0x00C8A430; LTCG: bone_index ECX, bone_data EAX, cur_time on the stack. The caller handles the times before the
// first and after the last key.
static float interpolate_compressed_weight(int bone_index, animlib_compressed_data_reader& bone_data, int cur_time)
{
    const int num_keys = bone_data.weight_as_shorts ? ((const unsigned short*)bone_data.num_weight_keys)[bone_index] : ((const unsigned char*)bone_data.num_weight_keys)[bone_index];
    const int first = bone_data.weight_indicies[bone_index];
    const afc_weight_key* keys = bone_data.weight_keys + first;
    const bool time_as_shorts = bone_data.time_as_shorts;
    const void* times = time_as_shorts ? (const void*)((const unsigned short*)bone_data.weight_times + first) : (const void*)((const unsigned char*)bone_data.weight_times + first);
    int hi = num_keys - 1;
    int lo = 0;
    while (lo < hi - 1) {
        const int mid = (lo + hi) >> 1;
        const int key_time = get_key_time(times, time_as_shorts, mid) * ANIM_TICKS_PER_KEY_TIME;
        if (key_time < cur_time) {
            lo = mid;
        } else if (key_time > cur_time) {
            hi = mid;
        } else {
            return keys[mid].weight;
        }
    }
    if (lo == hi) {
        return keys[lo].weight;
    }
    const float t = key_time_parameter(cur_time, get_key_time(times, time_as_shorts, lo), get_key_time(times, time_as_shorts, hi));
    const float a = (float)(1.0 - (double)t);
    return (float)((double)a * (double)keys[lo].weight + (double)keys[hi].weight * (double)t);
}

// 0x00C8A590; LTCG: num_keys EAX, keys / times / time_as_shorts on the stack, cur_time EDI.
static float interpolate_compressed_morph(int num_keys, afc_morph_key* keys, void* times, bool time_as_shorts, int cur_time)
{
    int hi = num_keys - 1;
    int lo = 0;
    while (lo < hi - 1) {
        const int mid = (lo + hi) >> 1;
        const int key_time = get_key_time(times, time_as_shorts, mid) * ANIM_TICKS_PER_KEY_TIME;
        if (key_time < cur_time) {
            lo = mid;
        } else if (key_time > cur_time) {
            hi = mid;
        } else {
            return (float)((double)keys[mid].weight * MORPH_KEY_SCALE);
        }
    }
    if (lo == hi) {
        return (float)((double)keys[lo].weight * MORPH_KEY_SCALE);
    }
    const float t = key_time_parameter(cur_time, get_key_time(times, time_as_shorts, lo), get_key_time(times, time_as_shorts, hi));
    const float a = (float)(1.0 - (double)t);
    return (float)(((double)a * (double)keys[lo].weight + (double)keys[hi].weight * (double)t) * MORPH_KEY_SCALE);
}

// 0x00C8A760. offset_data[selector] packs the four rotation key scalers (x in the low byte): per component, scaler
// bit 2 and bit 1 select 1, 2, 4 or 32. The selector is (x2 y2 z2 w2) << 4 | (x1 y1 z1 w1).
void anim_animation_system_init()
{
    memset(Animations, 0, sizeof(Animations));
    memset(Animation_lookup_table, 0xFF, sizeof(Animation_lookup_table));
    Num_triggers = 0;
    Num_animations = 0;
    Num_trigger_types = 0;
    for (int selector = 0; selector < 256; selector++) {
        int packed = 0;
        for (int c = 0; c < 4; c++) {
            const int bit2 = (selector >> (7 - c)) & 1;
            const int bit1 = (selector >> (3 - c)) & 1;
            const int scaler = bit2 ? (bit1 ? 0x20 : 4) : (bit1 ? 2 : 1);
            packed |= scaler << (c * 8);
        }
        offset_data[selector] = packed;
    }
}

// 0x00C8A9C0
int animlib_get_num_trigger_types()
{
    return Num_trigger_types;
}

// 0x00C8AA00
int animlib_get_trigger_id(const char* trigger_name)
{
    for (int i = 0; i < Num_trigger_types; i++) {
        if (v_stricmp(Trigger_names[i], trigger_name) == 0) {
            return i;
        }
    }
    return -1;
}

// 0x00C8AA80. Frees the file name's slot of the table (linear probing, as animlib_find_anim).
void animlib_remove_anim_from_lookup_table(const char* filename)
{
    const unsigned int hash = crc_stri(filename, 0);
    for (int i = 0; i < Animation_lookup_table_collisions; i++) {
        const unsigned int slot = (i + hash % 7750) % 7750;
        const int handle = Animation_lookup_table[slot];
        if (handle != -1 && Animations[handle].anim_filename_hash == hash) {
            Animation_lookup_table[slot] = -1;
            return;
        }
    }
}

// 0x00C8AA40. A new handle while the table has room, else the first freed slot.
static int animlib_get_next_anim_handle()
{
    if (Num_animations < 3875) {
        return Num_animations++;
    }
    for (int i = 0; i < 3875; i++) {
        if (Animations[i].anim_filename_hash == 0) {
            return i;
        }
    }
    return -1;
}

// 0x00C8AB00; LTCG: filename EAX, anim_handle on the stack. Linear probing; a file name already in the table is not
// added again. The first probe is examined twice (once as the home slot, once in the loop), as in the original.
static void animlib_add_anim_to_lookup_table(const char* filename, int anim_handle)
{
    const unsigned int hash = crc_stri(filename, 0);
    unsigned int slot = hash % 7750;
    int probes = 0;
    if (Animation_lookup_table[slot] != -1) {
        unsigned int next = slot;
        int handle;
        do {
            slot = next % 7750;
            handle = Animation_lookup_table[slot];
            if (handle != -1 && Animations[handle].anim_filename_hash == hash) {
                return;
            }
            probes++;
            next++;
        } while (handle != -1);
    }
    Animation_lookup_table[slot] = anim_handle;
    if (probes >= Animation_lookup_table_collisions) {
        Animation_lookup_table_collisions = probes + 1;
    }
}

// 0x00C8AB80
anim_file* anim_animation::get_animation_data() const
{
    return animation_data;
}

// 0x00C8AB90
anim_animation* get_animation_from_handle(int handle)
{
    if (handle > -1 && handle < Num_animations) {
        return &Animations[handle];
    }
    return nullptr;
}

// 0x00C8ABB0
bool anim_animation::paged_in() const
{
    return animation_data != nullptr;
}

// 0x00C8ABC0. The length in ticks.
int anim_animation::end_time() const
{
    return animation_data != nullptr ? animation_data->end_tock * ANIM_TICKS_PER_KEY_TIME : 0;
}

// 0x00C8AC60. frame is a 30 fps frame. The triggers of an animation must be contiguous in Trigger_array; a slot is
// taken (Num_triggers advanced) even when that fails, as in the original.
bool anim_animation::add_trigger(int trigger_id, int frame)
{
    if (Num_triggers > 5849) {
        return false;
    }
    const int ticktime = frame * 4800 / 30;
    for (int i = 0; i < num_triggers; i++) {
        if (triggers[i].id == trigger_id && ticktime == triggers[i].ticktime) {
            return true;
        }
    }
    anim_trigger* slot = &Trigger_array[Num_triggers];
    Num_triggers++;
    if (num_triggers == 0) {
        triggers = slot;
    }
    if (slot != triggers + num_triggers) {
        return false;
    }
    slot->id = trigger_id;
    slot->ticktime = ticktime;
    num_triggers++;
    return true;
}

// 0x00C92E20
bool animlib_register_trigger_name(const char* trigger_name)
{
    if (strlen(trigger_name) >= 32 || Num_trigger_types >= 28) {
        return false;
    }
    if (animlib_get_trigger_id(trigger_name) == -1) {
        strcpy(Trigger_names[Num_trigger_types], trigger_name);
        Num_trigger_types++;
    }
    return true;
}

// 0x00C92E90. The table keeps the file name pointer.
int animlib_get_anim_slot(const char* filename)
{
    const int found = animlib_find_anim(filename);
    if (found != -1) {
        return found;
    }
    const int handle = animlib_get_next_anim_handle();
    if (handle == -1) {
        return -1;
    }
    animlib_add_anim_to_lookup_table(filename, handle);
    if (handle < 0 || handle >= Num_animations) {
        return -1;
    }
    Animations[handle].anim_filename = filename;
    Animations[handle].anim_filename_hash = crc_stri(filename, 0);
    return handle;
}

// 0x00C92F40. The table keeps the name pointer.
bool animlib_set_morph_set_for_animation(int handle, const char* morph_set_name)
{
    if (handle < 0 || handle >= Num_animations)
    {
        return false;
    }
    Animations[handle].morph_set_name = morph_set_name;
    return true;
}

// 0x00C92F00
bool animlib_add_trigger_to_animation(int handle, int trigger_id, int frame)
{
    if (handle > -1 && handle < Num_animations && trigger_id > -1 && trigger_id < Num_trigger_types) {
        return Animations[handle].add_trigger(trigger_id, frame);
    }
    return false;
}

// Readers over the root controller bone, the bone the delta functions below read.
static void setup_root_controller_reader(const anim_file* amf, animlib_compressed_data_reader& reader)
{
    reader.bone_idx = -1;
    reader.init_level = 2;
    setup_bone_weights(amf, (char*)amf + amf->root_controller_bone.get_offset(), reader, 1);
    setup_bone_data(amf, reader);
}

// 0x00C94F60. The heading change of the root controller between two times.
float anim_animation::get_delta_heading(int prv_time, int cur_time) const
{
    if (prv_time == cur_time || animation_data == nullptr) {
        return 0.0f;
    }
    animlib_compressed_data_reader reader;
    setup_root_controller_reader(animation_data, reader);
    float heading = 0.0f;
    if (reader.num_rotation_keys > 1) {
        quaternion q0;
        quaternion q1;
        interpolate_compressed_rotation(prv_time, reader, q0);
        interpolate_compressed_rotation(cur_time, reader, q1);
        q0.inverse();
        const quaternion delta = q1 * q0;
        matrix m;
        delta.extract_matrix(m);
        m.extract_heading(heading);
    }
    return heading;
}

// 0x00C95080 (anim_lib_animation.cpp:1120). The root controller's heading at a time; animation_data is not checked, as
// the original.
float anim_animation::get_heading(int cur_time) const
{
    animlib_compressed_data_reader reader;
    setup_root_controller_reader(animation_data, reader);
    float heading = 0.0f;
    if (reader.num_rotation_keys == 0) {
        return 0.0f;
    }
    quaternion q;
    interpolate_compressed_rotation(cur_time, reader, q);
    matrix m;
    q.extract_matrix(m);
    m.extract_heading(heading);
    return heading;
}

// 0x00C95150. The root controller's translation between two times, rotated by its rotation at cur_time
// (delta_pos), and by its rotation at time 0 unless that is the identity (anim_local_delta_pos).
void anim_animation::get_delta_pos(vector& delta_pos, vector& anim_local_delta_pos, int prv_time, int cur_time) const
{
    delta_pos.zero();
    anim_local_delta_pos.zero();
    if (cur_time == prv_time) {
        return;
    }
    animlib_compressed_data_reader reader;
    setup_root_controller_reader(animation_data, reader);
    if (reader.num_translation_keys == 0) {
        return;
    }
    vector t0;
    vector t1;
    interpolate_compressed_translation(prv_time, reader, t0);
    interpolate_compressed_translation(cur_time, reader, t1);
    const vector d(t1.x - t0.x, t1.y - t0.y, t1.z - t0.z);
    delta_pos = d;
    anim_local_delta_pos = d;
    if (reader.num_rotation_keys != 0) {
        quaternion q;
        matrix m;
        interpolate_compressed_rotation(cur_time, reader, q);
        q.extract_matrix(m);
        delta_pos.rotate(m);
        interpolate_compressed_rotation(0, reader, q);
        if (q.x != 0.0f || q.y != 0.0f || q.z != 0.0f || 0.001f <= q.w - 1.0f || 0.001f <= -(q.w - 1.0f)) {
            q.extract_matrix(m);
            anim_local_delta_pos.rotate(m);
        }
    }
}

// 0x00C95360. The root controller's translation between two times, rotated by its rotation at prv_time.
void anim_animation::get_delta_pos_unrotated(vector& delta_pos, int prv_time, int cur_time) const
{
    delta_pos.zero();
    if (cur_time == prv_time) {
        return;
    }
    animlib_compressed_data_reader reader;
    setup_root_controller_reader(animation_data, reader);
    if (reader.num_translation_keys == 0) {
        return;
    }
    vector t0;
    vector t1;
    interpolate_compressed_translation(prv_time, reader, t0);
    interpolate_compressed_translation(cur_time, reader, t1);
    delta_pos.x = t1.x - t0.x;
    delta_pos.y = t1.y - t0.y;
    delta_pos.z = t1.z - t0.z;
    if (reader.num_rotation_keys != 0) {
        quaternion q;
        matrix m;
        interpolate_compressed_rotation(prv_time, reader, q);
        q.extract_matrix(m);
        delta_pos.rotate(m);
    }
}

// 0x00C954D0. Meters per second.
void anim_animation::get_velocity(vector& vel, int prv_time, int cur_time) const
{
    if (cur_time == prv_time) {
        vel.zero();
        return;
    }
    vector delta;
    vector local_delta;
    get_delta_pos(delta, local_delta, prv_time, cur_time);
    const float seconds = (float)(cur_time - prv_time) / 4800.0f;
    vel.x = delta.x / seconds;
    vel.y = delta.y / seconds;
    vel.z = delta.z / seconds;
}

// 0x00C95590. The bone's translation; for the first bone of a normal reader (the root) a horizontal translation is
// rotated by the root controller's rotation.
void anim_animation::find_animation_translation(vector& translation, int current_time, animlib_compressed_data_reader& bone_data) const
{
    interpolate_compressed_translation(current_time, bone_data, translation);
    if (bone_data.bone_idx == 0 && bone_data.init_level == 1) {
        const float dx = translation.x - 0.0f;
        const float dz = translation.z - 0.0f;
        if (0.001f <= dx || 0.001f <= -dx || 0.001f <= dz || 0.001f <= -dz) {
            animlib_compressed_data_reader reader;
            setup_root_controller_reader(animation_data, reader);
            if (reader.num_rotation_keys > 0) {
                quaternion q;
                matrix m;
                interpolate_compressed_rotation(current_time, reader, q);
                q.extract_matrix(m);
                translation.rotate(m);
            }
        }
    }
}

// 0x00C956C0. 0 when the bone is not animated, 1 without weight keys; capped at 1 above 1.01.
float anim_animation::find_animation_bone_weight(int rig_bone_index, int current_time, animlib_compressed_data_reader& bone_data) const
{
    const anim_file* amf = animation_data;
    if (rig_bone_index >= amf->num_bones) {
        return 0.0f;
    }
    const int map_offset = amf->anim_to_rig_bone_mapping.get_offset();
    if (bone_data.init_level != 2 && map_offset != -1 && map_offset != 0 && (int)&amf->anim_to_rig_bone_mapping + map_offset != 0) {
        if (bone_data.rig_to_anim_bone_mapping[rig_bone_index] == -1) {
            return 0.0f;
        }
        rig_bone_index = bone_data.rig_to_anim_bone_mapping[rig_bone_index];
    }
    if ((amf->flags & 0x40) == 0) {
        return 1.0f;
    }
    const unsigned int num_keys = (amf->flags & 0x80) ? ((const unsigned short*)bone_data.num_weight_keys)[rig_bone_index] : ((const unsigned char*)bone_data.num_weight_keys)[rig_bone_index];
    const int first = bone_data.weight_indicies[rig_bone_index];
    if (num_keys == 0) {
        return 1.0f;
    }
    float weight;
    if (num_keys == 1) {
        weight = bone_data.weight_keys[first].weight;
    } else if (current_time < get_key_time(bone_data.weight_times, bone_data.time_as_shorts, first) * ANIM_TICKS_PER_KEY_TIME) {
        weight = bone_data.weight_keys[first].weight;
    } else if (get_key_time(bone_data.weight_times, bone_data.time_as_shorts, first + num_keys - 1) * ANIM_TICKS_PER_KEY_TIME < current_time) {
        weight = bone_data.weight_keys[first + num_keys - 1].weight;
    } else {
        weight = interpolate_compressed_weight(rig_bone_index, bone_data, current_time);
    }
    if (1.01f < weight) {
        return 1.0f;
    }
    return weight;
}

// 0x00C95800. The morph target's weight (0..1); its keys are 8-bit.
float anim_animation::find_animation_morph_weight(int morph_index, int current_time) const
{
    const anim_file* amf = animation_data;
    // Without a morph table (flags & 0x10) the original reads through a null pointer; callers check the flag.
    const anim_morph* morphs = (amf->flags & 0x10) ? (const anim_morph*)(amf + 1) : nullptr;
    const int morph_offset = ((const int*)morphs->morph_offset.to_pointer())[morph_index];
    const afc_morph* morph = (const afc_morph*)((const char*)amf + morph_offset);
    if (morph == nullptr) {
        return 0.0f;
    }
    const bool time_as_shorts = ((amf->flags >> 1) & 1) != 0;
    const unsigned int num_keys = morph->num_morph_keys;
    afc_morph_key* keys = (afc_morph_key*)morph->data;
    void* times = (void*)((const char*)morph + 6 + num_keys);
    if (time_as_shorts) {
        times = (void*)(((unsigned int)times + 1) & ~1u);
    }
    if (num_keys == 0) {
        return 1.0f;
    }
    float weight;
    if (num_keys == 1) {
        weight = (float)keys[0].weight * (1.0f / 255.0f);
    } else if (current_time < get_key_time(times, time_as_shorts, 0) * ANIM_TICKS_PER_KEY_TIME) {
        weight = (float)keys[0].weight * (1.0f / 255.0f);
    } else if (current_time <= get_key_time(times, time_as_shorts, num_keys - 1) * ANIM_TICKS_PER_KEY_TIME) {
        weight = interpolate_compressed_morph(num_keys, keys, times, time_as_shorts, current_time);
    } else {
        weight = (float)keys[num_keys - 1].weight * (1.0f / 255.0f);
    }
    if (1.01f < weight) {
        return 1.0f;
    }
    return weight;
}
