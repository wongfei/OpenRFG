// Original: volition/render_lib/code/rl_instance_stream.cpp
//
// rl_mesh_instance_stream: the per-frame array of instance transforms the batched passes draw from (one
// rl_gpu_mesh_instance per instance: the transposed 3x4 world transform and the instance parameters packed into
// bytes). add_instance is lock-free (several visibility threads fill one stream).

#include "volition/render_lib/code/rl_instance_stream.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_gpu_visibility.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_metrics.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer_metrics.h"

#include <intrin.h>
#include <math.h>

// <rl_mesh_instance_stream::factory_init> 0x005102F0 (:7)
// <rl_mesh_instance_stream::release> 0x005103F0 (:7)
// <rl_mesh_instance_stream::create> 0x00526910 (:7)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_mesh_instance_stream, 0x4A61078F, 50)

// Vertex stream slots of the instance data.
const int rl_mesh_instance_stream::RENDER_STREAM_INDEX = 4; // 0x012BECA8
const int rl_mesh_instance_stream::DYNAMIC_INSTB_INDEX = 0; // 0x012BECAC

// Magic numbers: the stream holds 0x7627 instances (0x7627 * sizeof(rl_gpu_mesh_instance) = 0x17FFEC bytes, just under
// 1.5 MB).
static const int INSTANCE_STREAM_MAX_INSTANCES = 0x7627;
static const unsigned int INSTANCE_STREAM_ALLOC_SIZE = 0x17FFEC;

// The transposed 3x4 transform and the instance parameters as bytes (w, x, y, z from the top byte down; each rounded
// from [0, 1] * 255 in the current rounding mode).
static void write_instance(rl_gpu_mesh_instance& out, const rl_mesh_instance_info& inst)
{
    const matrix43& m = inst.transform;
    out.transform[0] = m.m_rotation.rvec.x;
    out.transform[1] = m.m_rotation.uvec.x;
    out.transform[2] = m.m_rotation.fvec.x;
    out.transform[3] = m.m_translation.x;
    out.transform[4] = m.m_rotation.rvec.y;
    out.transform[5] = m.m_rotation.uvec.y;
    out.transform[6] = m.m_rotation.fvec.y;
    out.transform[7] = m.m_translation.y;
    out.transform[8] = m.m_rotation.rvec.z;
    out.transform[9] = m.m_rotation.uvec.z;
    out.transform[10] = m.m_rotation.fvec.z;
    out.transform[11] = m.m_translation.z;
    const unsigned int x = (unsigned int)(long long)lrint(inst.instance_params.x * 255.0) & 0xFF;
    const unsigned int w = (unsigned int)(long long)lrint(inst.instance_params.w * 255.0) << 8;
    const unsigned int y = (unsigned int)(long long)lrint(inst.instance_params.y * 255.0) & 0xFF;
    const unsigned int z = (unsigned int)(long long)lrint(inst.instance_params.z * 255.0) & 0xFF;
    out.instance_params = ((x | w) << 8 | y) << 8 | z;
}

// 0x004C2220 (:27)
rl_mesh_instance_stream::rl_mesh_instance_stream()
{
    m_instances = nullptr;
    m_max_instances = 0;
    m_num_instances = 0;
    m_sizeof_alloc = 0;
    m_base_ptr = nullptr;
}

// 0x004C2240 (:38)
rl_mesh_instance_stream::~rl_mesh_instance_stream()
{
    if (m_instances != nullptr)
    {
        delete[] (unsigned char*)m_instances;
    }
}

// 0x004E8680 (:46)
// allocate_instances, inlined.
bool rl_mesh_instance_stream::init()
{
    m_instances = allocate_instances(m_max_instances);
    if (m_instances == nullptr)
    {
        m_max_instances = 0;
        m_num_instances = 0;
        return false;
    }
    return true;
}

// 0x004E86D0 (:66)
// The instance's index, or -1 (metrics warning 2) when the stream is full.
int rl_mesh_instance_stream::add_instance(const rl_mesh_instance_info& inst)
{
    const unsigned int index = (unsigned int)_InterlockedExchangeAdd((volatile long*)&m_num_instances, 1);
    if ((int)index >= m_max_instances)
    {
        rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(2);
        return -1;
    }
    write_instance(m_instances[index], inst);
    return (int)index;
}

// 0x004E8830 (:102)
// Each instance on its own (the indices need not be contiguous); returns the first added index, or -1.
int rl_mesh_instance_stream::add_instances(const rl_mesh_instance_info* insts, int num_instances)
{
    int first = -1;
    for (int i = 0; i < num_instances; i++)
    {
        int index = (int)_InterlockedExchangeAdd((volatile long*)&m_num_instances, 1);
        if (index < m_max_instances)
        {
            write_instance(m_instances[index], insts[i]);
        }
        else
        {
            rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(2);
            index = -1;
        }
        if (first == -1)
        {
            first = index;
        }
    }
    return first;
}

// 0x004C2270 (:124)
void rl_mesh_instance_stream::set_as_render_instance_stream()
{
}

// 0x004C2280 (:138)
rl_gpu_mesh_instance* rl_mesh_instance_stream::allocate_instances(int& max_allocated)
{
    m_base_ptr = new unsigned char[INSTANCE_STREAM_ALLOC_SIZE];
    m_sizeof_alloc = INSTANCE_STREAM_ALLOC_SIZE;
    max_allocated = INSTANCE_STREAM_MAX_INSTANCES;
    return (rl_gpu_mesh_instance*)m_base_ptr;
}

// 0x004C22C0 (:150)
void rl_mesh_instance_stream::finalize()
{
}

// 0x004BA0E0 (.h:44) and 0x004BA0F0 (.h:44) are the factory macro's get_object_pool and get_objects_max.

// 0x004BA100 (.h:63)
const rl_gpu_mesh_instance* rl_mesh_instance_stream::get_instances() const
{
    return m_instances;
}
