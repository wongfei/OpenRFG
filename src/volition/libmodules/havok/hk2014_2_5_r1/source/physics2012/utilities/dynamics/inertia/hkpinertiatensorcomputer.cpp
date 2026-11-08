// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the mass properties of a shape hierarchy, from rfg.exe (MP7 added the inertia
// balancing of a constraint tree). Every leaf shape is
// computed with mass 1, rescaled to density 1 (mass = volume) and combined into the result in the root's frame; the
// total is then scaled to the requested mass. A hierarchy without volume is retried with a triangle thickness of 0.01.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkstridedvertices.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/cylinder/hkpcylindershape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/bv/hkpbvshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/transform/hkptransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbodycinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"

#include <emmintrin.h>

namespace
{
inline __m128 splatX(__m128 v) { return _mm_shuffle_ps(v, v, 0x00); }
inline __m128 splatY(__m128 v) { return _mm_shuffle_ps(v, v, 0x55); }
inline __m128 splatZ(__m128 v) { return _mm_shuffle_ps(v, v, 0xAA); }

// hkpMultiSphereShape (no header yet): up to 8 spheres (xyz centre, w radius)
const int MULTI_SPHERE_NUM_SPHERES = 0x10; // int m_numSpheres
const int MULTI_SPHERE_SPHERES = 0x20; // hkVector4f m_spheres[8]

// zero mass properties
void clearMassProperties(hkMassProperties& props)
{
    props.m_volume = 0.0f;
    props.m_mass = 0.0f;
    props.m_centerOfMass.m_quad = _mm_setzero_ps();
    props.m_inertiaTensor.setZero();
}
}

// 0x00F1D880
// The mass properties of the shape scaled per axis: the volume and the centre of mass scale, and the inertia is
// rebuilt from the second moments (I_xx = (s_y^2 S_yy + s_z^2 S_zz) and so on with S_xx = (I_yy + I_zz - I_xx) / 2) and
// the scaled products. The w lanes of the new columns are not written in rfg.exe (stack garbage); they are 0 here.
void scaleVolumeMassProperties(const hkVector4f& scale, hkMassProperties& massProperties)
{
    const float sx = scale(0);
    const float sy = scale(1);
    const float sz = scale(2);
    const float halfX2 = sx * sx * 0.5f;
    const float halfY2 = sy * sy * 0.5f;
    const float halfZ2 = sz * sz * 0.5f;
    const hkMatrix3f I = massProperties.m_inertiaTensor;
    const float I00 = I.m_col0(0);
    const float I11 = I.m_col1(1);
    const float I22 = I.m_col2(2);
    const float zz = ((I11 + I00) - I22) * halfZ2;
    const float yy = ((I22 + I00) - I11) * halfY2;
    const float xx = ((I11 + I22) - I00) * halfX2;
    const float xy = I.m_col1(0) * sx * sy;
    const float xz = I.m_col2(0) * sx * sz;
    const float yz = I.m_col2(1) * sy * sz;
    massProperties.m_volume = sy * sx * sz * massProperties.m_volume;
    massProperties.m_centerOfMass.m_quad = _mm_mul_ps(massProperties.m_centerOfMass.m_quad, scale.m_quad);
    massProperties.m_inertiaTensor.m_col0.set(yy + zz, xy, xz, 0.0f);
    massProperties.m_inertiaTensor.m_col1.set(xy, xx + zz, yz, 0.0f);
    massProperties.m_inertiaTensor.m_col2.set(xz, yz, xx + yy, 0.0f);
}

namespace
{
// 0x00F1DF90 (LTCG registers: EDI massProperties, ESI massPropertiesOut)
// The shape's mass properties (scaled when the scale is not 1 within HK_REAL_EPSILON on every axis, then rescaled to
// density 1) combined with the accumulated ones under the transform's rotation and translation.
void transformAndCombineVolumeMassProperties(const hkQsTransformf& transform, hkMassProperties& massProperties, hkMassProperties& massPropertiesOut)
{
    if (massProperties.m_volume == 0.0f)
    {
        return;
    }
    hkVector4f scale = transform.m_scale;
    const __m128 offOne = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(_mm_sub_ps(scale.m_quad, g_vectorfConstants[HK_QUADREAL_1])), 1), 1));
    if ((_mm_movemask_ps(_mm_cmple_ps(offOne, g_vectorfConstants[HK_QUADREAL_EPS])) & 7) != 7)
    {
        scaleVolumeMassProperties(scale, massProperties);
    }
    hkSimdFloat32 toDensityOne;
    toDensityOne.m_real = _mm_set1_ps(massProperties.m_volume / massProperties.m_mass);
    const __m128 mass = _mm_mul_ps(_mm_set1_ps(massProperties.m_mass), toDensityOne.m_real);
    massProperties.m_inertiaTensor.mul(toDensityOne);
    massProperties.m_mass = _mm_cvtss_f32(mass);

    hkInplaceArray<hkMassElement, 2, hkContainerHeapAllocator> elements;
    elements.setSizeUnchecked(2);
    elements[0].m_properties = massPropertiesOut;
    elements[1].m_properties = massProperties;
    elements[1].m_transform.getRotation().set(transform.m_rotation);
    elements[1].m_transform.getTranslation() = transform.m_translation;
    hkInertiaTensorComputer::combineMassProperties(elements, massPropertiesOut);
}
}

// 0x00F1E1F0
void computeRecursiveShapeVolumeMassProperties(const hkcdShape* shape, const hkTransformf& transform, float minTriangleThickness, hkMassProperties& massPropertiesOut)
{
    hkMassProperties props;
    clearMassProperties(props);
    const unsigned char type = shape->m_type.m_storage;
    if (type > hkcdShapeType::PLANE)
    {
        return;
    }
    switch (type)
    {
    case hkcdShapeType::SPHERE:
    {
        const hkpSphereShape* sphere = static_cast<const hkpSphereShape*>(shape);
        hkInertiaTensorComputer::computeSphereVolumeMassProperties(sphere->m_radius, 1.0f, props);
        break;
    }
    case hkcdShapeType::CYLINDER:
    {
        // the cylinder's end points move out by the convex radius along the axis
        const hkpCylinderShape* cylinder = static_cast<const hkpCylinderShape*>(shape);
        const __m128 radius = _mm_set1_ps(cylinder->m_radius);
        const __m128 a = cylinder->m_vertexA.m_quad;
        const __m128 b = cylinder->m_vertexB.m_quad;
        const __m128 axis = _mm_sub_ps(b, a);
        const __m128 len2 = _mm_add_ps(_mm_add_ps(splatY(_mm_mul_ps(axis, axis)), splatX(_mm_mul_ps(axis, axis))), splatZ(_mm_mul_ps(axis, axis)));
        const __m128 r = _mm_rsqrt_ps(len2);
        const __m128 inv = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_mul_ps(r, len2), r)), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
        const __m128 out = _mm_mul_ps(_mm_mul_ps(_mm_andnot_ps(_mm_cmple_ps(len2, _mm_setzero_ps()), inv), axis), radius);
        hkVector4f start;
        hkVector4f end;
        start.m_quad = _mm_sub_ps(a, out);
        end.m_quad = _mm_add_ps(out, b);
        const float cylinderRadius = (float)((double)cylinder->getCylinderRadius() + (double)cylinder->m_radius); // x87
        hkInertiaTensorComputer::computeCylinderVolumeMassProperties(start, end, cylinderRadius, 1.0f, props);
        break;
    }
    case hkcdShapeType::TRIANGLE:
    {
        const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(shape);
        const float thickness = (triangle->m_radius > minTriangleThickness) ? triangle->m_radius : minTriangleThickness; // maxss
        hkInertiaTensorComputer::computeTriangleSurfaceMassProperties(triangle->m_vertexA, triangle->m_vertexB, triangle->m_vertexC, 1.0f, thickness, props);
        break;
    }
    case hkcdShapeType::BOX:
    {
        const hkpBoxShape* box = static_cast<const hkpBoxShape*>(shape);
        const __m128 r = _mm_set1_ps(box->m_radius);
        hkVector4f halfExtents;
        halfExtents.m_quad = _mm_add_ps(_mm_shuffle_ps(r, _mm_unpackhi_ps(r, _mm_setzero_ps()), 0xC4), box->m_halfExtents.m_quad); // (r, r, r, 0)
        hkInertiaTensorComputer::computeBoxVolumeMassProperties(halfExtents, 1.0f, props);
        break;
    }
    case hkcdShapeType::CAPSULE:
    {
        const hkpCapsuleShape* capsule = static_cast<const hkpCapsuleShape*>(shape);
        hkInertiaTensorComputer::computeCapsuleVolumeMassProperties(capsule->m_vertexA, capsule->m_vertexB, capsule->m_radius, 1.0f, props);
        break;
    }
    case hkcdShapeType::CONVEX_VERTICES:
    {
        const hkpConvexVerticesShape* convex = static_cast<const hkpConvexVerticesShape*>(shape);
        hkArray<hkVector4f, hkContainerHeapAllocator> vertices;
        convex->getOriginalVertices(vertices);
        hkStridedVertices strided;
        strided.m_vertices = reinterpret_cast<const float*>(vertices.begin());
        strided.m_numVertices = vertices.getSize();
        strided.m_striding = sizeof(hkVector4f);
        hkInertiaTensorComputer::computeConvexHullMassProperties(strided, convex->m_radius, props);
        break;
    }
    case hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_COLLECTION:
    case hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE:
    case hkcdShapeType::LIST:
    case hkcdShapeType::MOPP:
    case hkcdShapeType::EXTENDED_MESH:
    case hkcdShapeType::COMPRESSED_MESH:
    case hkcdShapeType::STATIC_COMPOUND:
    case hkcdShapeType::BV_COMPRESSED_MESH:
    case hkcdShapeType::COLLECTION:
    case hkcdShapeType::BV_TREE:
    case hkcdShapeType::CONVEX_LIST:
    case hkcdShapeType::TRIANGLE_COLLECTION:
    {
        // every child under the same transform
        const hkpShapeContainer* container = static_cast<const hkpShape*>(shape)->getContainer();
        hkpShapeBuffer buffer;
        for (unsigned int key = container->getFirstKey(); key != 0xFFFFFFFF; key = container->getNextKey(key))
        {
            const hkpShape* child = container->getChildShape(key, buffer);
            if (child)
            {
                computeRecursiveShapeVolumeMassProperties(child, transform, minTriangleThickness, massPropertiesOut);
            }
        }
        return;
    }
    case hkcdShapeType::CONVEX_TRANSLATE:
    {
        const hkpConvexTranslateShape* translate = static_cast<const hkpConvexTranslateShape*>(shape);
        hkTransformf local;
        local.getRotation().setIdentity();
        local.getTranslation() = translate->m_translation;
        hkTransformf childTransform;
        childTransform.setMul(transform, local);
        computeRecursiveShapeVolumeMassProperties(translate->m_childShape.m_childShape, childTransform, minTriangleThickness, massPropertiesOut);
        return;
    }
    case hkcdShapeType::CONVEX_TRANSFORM:
    {
        // the child's properties in its own frame, then combined under (transform without scale) * shape transform
        const hkpConvexTransformShape* convexTransform = static_cast<const hkpConvexTransformShape*>(shape);
        hkMassProperties childProps;
        clearMassProperties(childProps);
        computeRecursiveShapeVolumeMassProperties(convexTransform->m_childShape.m_childShape, hkTransformf::getIdentity(), minTriangleThickness, childProps);
        hkQsTransformf outer;
        outer.setFromTransformNoScale(transform);
        const hkQsTransformf& inner = convexTransform->m_transform;
        hkQsTransformf combined;
        combined.m_translation._setRotatedDir(outer.m_rotation, inner.m_translation);
        combined.m_translation.m_quad = _mm_add_ps(combined.m_translation.m_quad, outer.m_translation.m_quad);
        combined.m_rotation.setMul(outer.m_rotation, inner.m_rotation);
        combined.m_scale.m_quad = _mm_mul_ps(inner.m_scale.m_quad, outer.m_scale.m_quad);
        transformAndCombineVolumeMassProperties(combined, childProps, massPropertiesOut);
        return;
    }
    case hkcdShapeType::TRANSFORM:
    {
        const hkpTransformShape* transformShape = static_cast<const hkpTransformShape*>(shape);
        hkTransformf childTransform;
        childTransform.setMul(transform, transformShape->m_transform);
        computeRecursiveShapeVolumeMassProperties(transformShape->m_childShape.m_childShape, childTransform, minTriangleThickness, massPropertiesOut);
        return;
    }
    case hkcdShapeType::MULTI_SPHERE:
    {
        // a sphere shape per sphere, at its centre
        const char* base = reinterpret_cast<const char*>(shape);
        const int numSpheres = *reinterpret_cast<const int*>(base + MULTI_SPHERE_NUM_SPHERES);
        const hkVector4f* spheres = reinterpret_cast<const hkVector4f*>(base + MULTI_SPHERE_SPHERES);
        for (int i = 0; i < numSpheres; i++)
        {
            hkTransformf sphereTransform = transform;
            const __m128 c = spheres[i].m_quad;
            const hkRotationf& rot = transform.getRotation();
            const __m128 p = _mm_add_ps(_mm_add_ps(_mm_mul_ps(splatX(c), rot.m_col0.m_quad), _mm_mul_ps(splatY(c), rot.m_col1.m_quad)), _mm_mul_ps(splatZ(c), rot.m_col2.m_quad));
            sphereTransform.getTranslation().m_quad = _mm_add_ps(p, transform.getTranslation().m_quad);
            hkpSphereShape sphere(spheres[i](3));
            computeRecursiveShapeVolumeMassProperties(&sphere, sphereTransform, minTriangleThickness, massPropertiesOut);
        }
        return;
    }
    case hkcdShapeType::BV:
    {
        const hkpBvShape* bv = static_cast<const hkpBvShape*>(shape);
        computeRecursiveShapeVolumeMassProperties(bv->m_childShape.m_childShape, transform, minTriangleThickness, massPropertiesOut);
        return;
    }
    default:
        return;
    }

    // a leaf: to density 1, combined with what was accumulated under the transform
    if (props.m_volume == 0.0f)
    {
        return;
    }
    hkSimdFloat32 volume;
    volume.m_real = _mm_set1_ps(props.m_volume);
    const __m128 mass = _mm_mul_ps(_mm_set1_ps(props.m_mass), volume.m_real);
    props.m_inertiaTensor.mul(volume);
    props.m_mass = _mm_cvtss_f32(mass);
    hkInplaceArray<hkMassElement, 2, hkContainerHeapAllocator> elements;
    elements.setSizeUnchecked(2);
    elements[0].m_properties = massPropertiesOut;
    elements[1].m_properties = props;
    elements[1].m_transform = transform;
    hkInertiaTensorComputer::combineMassProperties(elements, massPropertiesOut);
}

// 0x00F1EB00: nothing is written when the shape has no volume even with thick triangles
void hkpInertiaTensorComputer::computeShapeVolumeMassProperties(const hkcdShape* shape, float mass, hkMassProperties& result)
{
    hkMassProperties props;
    clearMassProperties(props);
    // a local identity: its translation's w is 0 (that of hkTransformf::getIdentity() is 1)
    hkTransformf identity;
    identity.getRotation().setIdentity();
    identity.getTranslation().m_quad = _mm_setzero_ps();
    computeRecursiveShapeVolumeMassProperties(shape, identity, 0.0f, props);
    if (props.m_volume == 0.0f)
    {
        computeRecursiveShapeVolumeMassProperties(shape, identity, 0.01f, props); // 0x3C23D70A
        if (props.m_volume == 0.0f)
        {
            return;
        }
    }
    hkSimdFloat32 density;
    density.m_real = _mm_div_ps(_mm_set1_ps(mass), _mm_set1_ps(props.m_volume));
    const __m128 newMass = _mm_mul_ps(_mm_set1_ps(props.m_mass), density.m_real);
    props.m_inertiaTensor.mul(density);
    result.m_mass = _mm_cvtss_f32(newMass);
    result.m_centerOfMass = props.m_centerOfMass;
    result.m_volume = props.m_volume;
    result.m_inertiaTensor = props.m_inertiaTensor;
}

namespace
{
// 0x00F1DB50 (multiplayer.md MP7): takes the constraints of body out of the list (those to a body other than null),
// recurses into each child (the constraints' other bodies), and returns the inertia the subtree can carry. A fixed or
// keyframed body carries nothing; a leaf returns its largest local inertia; any other body has its local inertia's
// diagonal raised to max(min(children's sum, largest * hint), children's largest) and returns
// max(min(largest * hint, largest + children's sum), that raised value)
hkSimdFloat32 optimizeInertiasOfConstraintTreeInt(hkArray<const hkpConstraintInstance*, hkContainerHeapAllocator>& constraints, hkpRigidBody* body, const hkSimdFloat32& inertiaFactorHint)
{
    hkInplaceArray<const hkpConstraintInstance*, 16, hkContainerHeapAllocator> children;
    for (int i = constraints.getSize() - 1; i >= 0; i--)
    {
        const hkpConstraintInstance* constraint = constraints[i];
        hkpEntity* other;
        if (constraint->m_entities[0] == body)
        {
            other = constraint->m_entities[1];
        }
        else if (constraint->m_entities[1] == body)
        {
            other = constraint->m_entities[0];
        }
        else
        {
            continue;
        }
        if (other != nullptr)
        {
            constraints.removeAt(i);
            children.pushBack(constraint);
        }
    }
    __m128 sum = _mm_setzero_ps();
    __m128 maxChild = _mm_setzero_ps();
    for (int i = children.getSize() - 1; i >= 0; i--)
    {
        const hkpConstraintInstance* constraint = children[i];
        hkpEntity* other = constraint->m_entities[0] == body ? constraint->m_entities[1] : constraint->m_entities[0];
        const hkSimdFloat32 child = optimizeInertiasOfConstraintTreeInt(constraints, static_cast<hkpRigidBody*>(other), inertiaFactorHint);
        sum = _mm_add_ps(child.m_real, sum);
        maxChild = _mm_max_ps(child.m_real, maxChild);
    }
    hkSimdFloat32 result;
    const unsigned char motionType = body->m_motion.m_type.m_storage;
    if (motionType == hkpMotion::MOTION_FIXED || motionType == hkpMotion::MOTION_KEYFRAMED)
    {
        result.m_real = g_vectorfConstants[HK_QUADREAL_0];
        return result;
    }
    hkMatrix3f inertia;
    body->m_motion.getInertiaLocal(inertia);
    const __m128 maskY = _mm_castsi128_ps(_mm_setr_epi32(0, -1, 0, 0));
    const __m128 maskZ = _mm_castsi128_ps(_mm_setr_epi32(0, 0, -1, 0));
    const __m128 c0 = inertia.m_col0.m_quad;
    const __m128 c1 = inertia.m_col1.m_quad;
    const __m128 c2 = inertia.m_col2.m_quad;
    const __m128 diag = _mm_or_ps(_mm_andnot_ps(maskZ, _mm_or_ps(_mm_andnot_ps(maskY, c0), _mm_and_ps(maskY, c1))), _mm_and_ps(c2, maskZ));
    const __m128 largest = _mm_max_ps(splatZ(diag), _mm_max_ps(splatY(diag), splatX(diag)));
    if (!(_mm_cvtss_f32(sum) != 0.0f) || _mm_cvtss_f32(sum) != _mm_cvtss_f32(sum)) // UCOMISS + JNZ: unordered counts as equal
    {
        result.m_real = largest;
        return result;
    }
    const __m128 raised = _mm_max_ps(_mm_min_ps(sum, _mm_mul_ps(largest, inertiaFactorHint.m_real)), maxChild);
    const __m128 newDiag = _mm_max_ps(diag, raised);
    hkMatrix3f newInertia;
    newInertia.m_col0.m_quad = _mm_move_ss(c0, newDiag);
    newInertia.m_col1.m_quad = _mm_or_ps(_mm_and_ps(maskY, newDiag), _mm_andnot_ps(maskY, c1));
    newInertia.m_col2.m_quad = _mm_or_ps(_mm_and_ps(maskZ, newDiag), _mm_andnot_ps(maskZ, c2));
    body->setInertiaLocal(newInertia);
    result.m_real = _mm_max_ps(_mm_min_ps(_mm_mul_ps(largest, inertiaFactorHint.m_real), _mm_add_ps(largest, sum)), raised);
    return result;
}
}

// 0x00F1DE40 (multiplayer.md MP7): the list copied into a LIFO array, then the tree from the root
void hkpInertiaTensorComputer::optimizeInertiasOfConstraintTree(hkpConstraintInstance* const * constraints, int numConstraints, hkpRigidBody* rootBody, float inertiaFactorHint)
{
    hkLocalArray<const hkpConstraintInstance*> list(numConstraints);
    for (int i = 0; i < numConstraints; i++)
    {
        list.pushBackUnchecked(constraints[i]);
    }
    hkSimdFloat32 hint;
    hint.m_real = _mm_set1_ps(inertiaFactorHint);
    optimizeInertiasOfConstraintTreeInt(list, rootBody, hint);
}

// 0x00F1EC50: the mass, centre of mass and inertia of the body (the volume is not kept)
void hkpInertiaTensorComputer::setShapeVolumeMassProperties(const hkpShape* shape, float mass, hkpRigidBodyCinfo& bodyInfo)
{
    hkMassProperties props;
    clearMassProperties(props);
    computeShapeVolumeMassProperties(shape, mass, props);
    bodyInfo.m_mass = props.m_mass;
    bodyInfo.m_centerOfMass = props.m_centerOfMass;
    bodyInfo.m_inertiaTensor = props.m_inertiaTensor;
}
