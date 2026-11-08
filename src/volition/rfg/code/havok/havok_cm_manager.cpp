// Original: volition/rfg/code/havok/havok_cm_manager.cpp
//
// The collision model table (decompiled). Collision models are shared Havok shapes indexed by a cm index below
// MAX_COLLISION_MODELS (0x7FFF); free indices are kept as a stack in Collision_models_free_list, whose top is
// Num_collision_models. Index 0 is the default model, a 1x1x1 box centred 0.5 m above the origin, created by
// havok_cm_manager_init and freed by havok_cm_manager_shutdown.
// Chunk collision data (.cchk_pc) is a 12-byte header (version at +4: 2 = the chunk size is at +8, otherwise it is +8
// plus the 16-bit value at +6) followed by the model count and a Havok packfile whose top-level object is an
// hkpListShape; each child of the list becomes one collision model.
//
// Compiled in the Havok unity file (rfg_havok_unity.cpp) after havok.cpp, which defines the table's file statics.
//
// Notes (decompiled behaviour, kept as is):
// - havok_cm_manager_create_sphere_shape keeps its own reference on the sphere when it wraps it in an
//   hkpConvexTranslateShape (the translate shape adds a second one), so an offset sphere is never freed.
// - havok_cm_manager_add returns 0 (the default model) instead of an invalid index when the table is full or not
//   initialized.
// - havok_cm_manager_load_all ignores max_cm_indices.

#include "volition/rfg/code/havok/havok_cm_manager.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkproductfeatures.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkstridedvertices.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializeutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/cylinder/hkpcylindershape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector3.h"

#include "keen/base/trace.hpp"

#include <math.h>

// The Havok product configuration: the game's feature list, then the SDK's hkproductfeatures.cxx, which defines
// hkProductFeatures::initialize in this file (0x00819910, between havok_bp.cpp and this file's functions).
#include "volition/rfg/code/havok/hkproductfeatures.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkproductfeatures.cxx"
// The builtin type registry's class and type info lists (HK_CLASSES_FILE in Havok; generated from rfg.exe here,
// stub_impl.md S4 SD), compiled into this unity file as rfg.exe does (0x012F8628, near havok_mesh_shape's data).
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.cxx"

// ---- file statics (Num_collision_models, Collision_models, Allow_default_cm_removal and MAX_COLLISION_MODELS are
// havok.cpp's, earlier in the unity file)
static unsigned int Collision_models_free_list[MAX_COLLISION_MODELS]; // 0x02622728

// 0x0080F2C0 (:170)
// LTCG: rotation in ECX, translation in EAX; not called by the functions implemented here (havok_cm_manager_create_box_shape
// inlines the same test).
static bool is_identity(const quaternion& rotation, const vector& translation, float tolerance)
{
    return fabsf(1.0f - fabsf(rotation.w)) <= tolerance && fabsf(translation.x) <= tolerance &&
           fabsf(translation.y) <= tolerance && fabsf(translation.z) <= tolerance;
}

// 0x00835660 (:438)
void havok_cm_manager_init()
{
    hkFeature_serializeDeprecatedPre700();
    Num_collision_models = 0;
    for (int i = 0; i < MAX_COLLISION_MODELS; i++)
    {
        Collision_models[i] = nullptr;
    }
    for (unsigned int i = 0; i < (unsigned int)MAX_COLLISION_MODELS; i++)
    {
        Collision_models_free_list[i] = i;
    }

    // The default collision model (index 0): a 1 m box standing on the origin.
    vector position(0.0f, 0.5f, 0.0f);
    vector half_extents(0.5f, 0.5f, 0.5f);
    quaternion rotation = IDENTITY_QUATERNION;
    hkpShape* default_model = havok_cm_manager_create_box_shape(&rotation, &position, &half_extents);
    if (default_model != nullptr)
    {
        // havok_cm_manager_add, inlined
        if (Num_collision_models >= MAX_COLLISION_MODELS)
        {
            keen::debug::nativePrintArgs("havok_cm_manager_add: Too many collision models already exist, increase MAX_COLLISION_MODELS!\n");
        }
        else if (Num_collision_models < 0)
        {
            keen::debug::nativePrintArgs("havok_cm_manager_add: Trying to add a collision model before the system has been initialized... bad");
        }
        else
        {
            const unsigned int index = Collision_models_free_list[Num_collision_models];
            if (index < (unsigned int)MAX_COLLISION_MODELS)
            {
                Collision_models_free_list[Num_collision_models] = 0xFFFFFFFF;
                Num_collision_models++;
                Collision_models[index] = default_model;
            }
        }
    }
    Allow_default_cm_removal = false;
}

// 0x00824000 (:472)
void havok_cm_manager_shutdown()
{
    Allow_default_cm_removal = true;
    // havok_cm_manager_remove_from_list(0), inlined
    if (Collision_models[0] != nullptr)
    {
        Collision_models[0]->removeReference();
        Num_collision_models--;
        Collision_models[0] = nullptr;
        Collision_models_free_list[Num_collision_models] = 0;
    }
}

// 0x0080F370 (:492)
void havok_cm_manager_read_cm_header(unsigned int* cm_chunk_size, const void* source_buffer, unsigned int* data_size, const unsigned char** data_start)
{
    const unsigned char* header = (const unsigned char*)source_buffer;
    if (*(const short*)(header + 4) == 2)
    {
        *cm_chunk_size = *(const unsigned int*)(header + 8);
    }
    else
    {
        *cm_chunk_size = *(const unsigned short*)(header + 6) + *(const unsigned int*)(header + 8);
    }
    *data_size = *(const unsigned int*)(header + 8) - 0xC;
    *data_start = header + 0xC;
}

// 0x00824040 (:532)
bool havok_cm_manager_load_all(unsigned int* cm_chunk_size, const void* source_buffer, unsigned int* cm_indices, unsigned int max_cm_indices, unsigned int* out_num_collision_models, const char* asset_name)
{
    const unsigned char* header = (const unsigned char*)source_buffer;
    if (*(const short*)(header + 4) == 2)
    {
        *cm_chunk_size = *(const unsigned int*)(header + 8);
    }
    else
    {
        *cm_chunk_size = *(const unsigned short*)(header + 6) + *(const unsigned int*)(header + 8);
    }
    const unsigned int data_size = *(const unsigned int*)(header + 8) - 0xC;
    const unsigned char* data_start = header + 0xC;

    cfile* fp = cf_open((unsigned char*)data_start, data_size, "rb", PLATFORM_PC);
    const unsigned int num_models = cf_read_uint32(fp);
    if (out_num_collision_models != nullptr)
    {
        *out_num_collision_models = num_models;
    }
    const unsigned int count_size = cf_tell(fp);
    cf_close(fp);
    if (num_models == 0)
    {
        return true;
    }

    // The packfile follows the count, at the next 16-byte boundary.
    const unsigned char* packfile = data_start + count_size;
    const unsigned int misalign = (unsigned int)(size_t)packfile & 0xF;
    const int pad = misalign == 0 ? 0 : 0x10 - misalign;
    hkSerializeUtil::ErrorDetails error;
    hkObjectResource* resource = hkSerializeUtil::loadOnHeap(packfile + pad, (int)(data_size - pad - count_size), &error, hkSerializeUtil::LoadOptions(hkSerializeUtil::LOAD_DEFAULT));
    if (resource == nullptr)
    {
        return false;
    }

    // resource->stealContents<hkpListShape>(), inlined
    hkTypeInfoRegistry* loaded_registry = hkBuiltinTypeRegistry::getInstance().getLoadedObjectRegistry();
    hkpListShape* list = (hkpListShape*)resource->stealContentsPointer(hkpListShape::staticClass().getName(), loaded_registry);
    for (int i = 0; i < list->getNumChildShapes() + list->m_numDisabledChildren; i++)
    {
        const hkpShape* collision_model = list->m_childInfo[i].m_shape;
        const unsigned int cm_index = havok_cm_manager_add(collision_model);
        if (cm_indices != nullptr && cm_index != 0xFFFFFFFF)
        {
            cm_indices[i] = cm_index;
        }
        collision_model->addReference();
    }
    list->removeReference();
    resource->removeReference();
    return true;
}

// 0x0080F3C0 (:626)
const hkpShape* havok_cm_manager_get(unsigned int cm_index)
{
    if (cm_index >= (unsigned int)MAX_COLLISION_MODELS)
    {
        return nullptr;
    }
    return Collision_models[cm_index];
}

// 0x0080F3E0 (:643)
unsigned int havok_cm_manager_add(const hkpShape* collision_model)
{
    if (collision_model == nullptr)
    {
        return 0;
    }
    if (Num_collision_models >= MAX_COLLISION_MODELS)
    {
        keen::debug::nativePrintArgs("havok_cm_manager_add: Too many collision models already exist, increase MAX_COLLISION_MODELS!\n");
        return 0;
    }
    if (Num_collision_models < 0)
    {
        keen::debug::nativePrintArgs("havok_cm_manager_add: Trying to add a collision model before the system has been initialized... bad");
        return 0;
    }
    const unsigned int index = Collision_models_free_list[Num_collision_models];
    if (index >= (unsigned int)MAX_COLLISION_MODELS)
    {
        return 0xFFFFFFFF;
    }
    Collision_models_free_list[Num_collision_models] = 0xFFFFFFFF;
    Collision_models[index] = collision_model;
    Num_collision_models++;
    return index;
}

// 0x0080F450 (:688)
unsigned int havok_cm_manager_get_count()
{
    return (unsigned int)Num_collision_models;
}

// 0x008199E0 (:697)
void havok_cm_manager_remove_from_list(unsigned int cm_index)
{
    if (cm_index != 0xFFFFFFFF && Collision_models[cm_index] != nullptr)
    {
        Collision_models[cm_index]->removeReference();
        Num_collision_models--;
        Collision_models[cm_index] = nullptr;
        Collision_models_free_list[Num_collision_models] = cm_index;
    }
}

// 0x00819A20 (:729)
void havok_cm_manager_remove_all()
{
    for (unsigned int i = 0; (int)i < MAX_COLLISION_MODELS; i++)
    {
        if (Collision_models[i] != nullptr && i != 0xFFFFFFFF)
        {
            Collision_models[i]->removeReference();
            Num_collision_models--;
            Collision_models[i] = nullptr;
            Collision_models_free_list[Num_collision_models] = i;
        }
    }
}

// 0x00832750 (:750)
// An axis-aligned box at the origin is an hkpBoxShape (zero extents become 1 cm); a rotated or offset box becomes an
// hkpConvexVerticesShape of its 8 corners and 6 face planes.
hkpShape* havok_cm_manager_create_box_shape(const quaternion* q_rotation, const vector* position, const vector* half_extents)
{
    // is_identity(*q_rotation, *position, 0.001f), inlined
    if (fabsf(1.0f - fabsf(q_rotation->w)) > 0.001f || fabsf(position->x) > 0.001f || fabsf(position->y) > 0.001f || fabsf(position->z) > 0.001f)
    {
        const float hx = half_extents->x;
        const float hy = half_extents->y;
        const float hz = half_extents->z;
        hkVector4f corners[8];
        corners[0].set(hx, hy, hz, 0.0f);
        corners[1].set(-hx, hy, hz, 0.0f);
        corners[2].set(-hx, -hy, hz, 0.0f);
        corners[3].set(hx, -hy, hz, 0.0f);
        corners[4].set(hx, hy, -hz, 0.0f);
        corners[5].set(-hx, hy, -hz, 0.0f);
        corners[6].set(-hx, -hy, -hz, 0.0f);
        corners[7].set(hx, -hy, -hz, 0.0f);

        // Havok's quaternion is the conjugate of Volition's (w negated).
        hkTransformf transform;
        hkQuaternionf hk_rotation;
        hk_rotation.m_vec.set(q_rotation->x, q_rotation->y, q_rotation->z, -q_rotation->w);
        transform.m_rotation.set(hk_rotation);
        transform.m_translation.set(position->x, position->y, position->z, 0.0f);
        hkVector4f vertices[8];
        for (int i = 0; i < 8; i++)
        {
            vertices[i].setTransformedPos(transform, corners[i]);
        }

        // Face planes (normal, -dot(normal, point on face)).
        matrix orient;
        q_rotation->extract_matrix(orient);
        hkArray<hkVector4f,hkContainerHeapAllocator> planes;
        const vector* axes[3] = {&orient.rvec, &orient.uvec, &orient.fvec};
        const int plus_corner[3] = {0, 0, 2};
        const int minus_corner[3] = {1, 2, 6};
        for (int a = 0; a < 3; a++)
        {
            const vector& n = *axes[a];
            const float* p = (const float*)&vertices[plus_corner[a]];
            const float* q = (const float*)&vertices[minus_corner[a]];
            hkVector4f plane;
            plane.set(n.x, n.y, n.z, 0.0f - (p[1] * n.y + p[0] * n.x + p[2] * n.z));
            planes.pushBack(plane);
            plane.set(-n.x, -n.y, -n.z, 0.0f - (q[1] * -n.y + q[0] * -n.x + q[2] * -n.z));
            planes.pushBack(plane);
        }

        hkStridedVertices strided;
        strided.m_vertices = (const float*)vertices;
        strided.m_numVertices = 8;
        strided.m_striding = sizeof(hkVector4f);
        return new hkpConvexVerticesShape(strided, planes, hkConvexShapeDefaultRadius);
    }

    hkVector4f extents;
    float ex = half_extents->x;
    float ey = half_extents->y;
    float ez = half_extents->z;
    if (ex - 0.0f < 0.0001f && -(ex - 0.0f) < 0.0001f)
    {
        ex = 0.01f;
    }
    if (ey - 0.0f < 0.0001f && -(ey - 0.0f) < 0.0001f)
    {
        ey = 0.01f;
    }
    if (ez - 0.0f < 0.0001f && -(ez - 0.0f) < 0.0001f)
    {
        ez = 0.01f;
    }
    extents.set(ex, ey, ez, 0.0f);
    return new hkpBoxShape(extents, hkConvexShapeDefaultRadius);
}

// 0x00819A60 (:908)
hkpShape* havok_cm_manager_create_sphere_shape(const vector* position, float radius)
{
    hkpSphereShape* sphere = new hkpSphereShape(radius);
    if (sphere == nullptr)
    {
        return nullptr;
    }
    if (position->x * position->x + position->y * position->y + position->z * position->z <= 0.001f)
    {
        return sphere;
    }
    hkVector4f translation;
    translation.set(position->x, position->y, position->z, 0.0f);
    // The sphere keeps its creation reference (see the file comment).
    return new hkpConvexTranslateShape(sphere, translation, hkpShapeContainer::REFERENCE_POLICY_INCREMENT);
}

// 0x00819B90 (:934)
hkpShape* havok_cm_manager_create_capsule_shape(const vector* endA, const vector* endB, float radius)
{
    hkVector4f a;
    hkVector4f b;
    a.set(endA->x, endA->y, endA->z, 0.0f);
    b.set(endB->x, endB->y, endB->z, 0.0f);
    return new hkpCapsuleShape(a, b, radius);
}

// 0x00819C50 (:952)
hkpShape* havok_cm_manager_create_cylinder_shape(const vector* endA, const vector* endB, float radius)
{
    hkVector4f a;
    hkVector4f b;
    a.set(endA->x, endA->y, endA->z, 0.0f);
    b.set(endB->x, endB->y, endB->z, 0.0f);
    return new hkpCylinderShape(a, b, radius, hkConvexShapeDefaultRadius);
}
