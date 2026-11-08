// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/geometryutilities/inertia/hkinertiatensorcomputer.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declaration with hkMassProperties and hkMassElement of Havok 2014.1
// (hkInertiaTensorComputer.h; layouts from rfg.pdb). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkBool;
struct hkGeometry;
class hkMatrix3f;
struct hkResult;
class hkRotationf;
class hkSimdFloat32;
struct hkStridedVertices;
class hkVector4f;

// size 0x50
struct hkMassProperties
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkMassProperties);

    // 0x00B68B10 (inline in Havok)
    hkMassProperties()
        : m_volume(0.0f)
        , m_mass(0.0f)
    {
        m_centerOfMass.setZero();
        m_inertiaTensor.setZero();
    }

    float m_volume; // 0x00
    float m_mass; // 0x04
    hkVector4f m_centerOfMass; // 0x10
    hkMatrix3f m_inertiaTensor; // 0x20
};

// size 0x90
struct hkMassElement
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkMassElement);

    // 0x00F1EF80 (inline in Havok)
    hkMassElement()
    {
        m_transform.setIdentity();
    }
    hkMassElement(const hkMassProperties& properties, const hkTransformf& transform)
        : m_properties(properties)
        , m_transform(transform)
    {
    }

    hkMassProperties m_properties; // 0x00
    hkTransformf m_transform; // 0x50
};

// size 0x1
class hkInertiaTensorComputer
{
public:
    typedef hkResult (*ConvexHullMassPropertiesFunction)(const hkStridedVertices&, float, hkMassProperties&);

    static hkResult computeSphereVolumeMassProperties(float radius, float mass, hkMassProperties& result); // 0x00F37DC0
    static hkResult computeBoxVolumeMassProperties(const hkVector4f& halfExtents, float mass, hkMassProperties& result); // 0x00F385D0
    static hkResult computeBoxVolumeMassPropertiesDiagonalized(const hkVector4f& halfExtents, float mass, hkVector4f& inertiaDiagonal, float& volume); // 0x00F37F10
    static hkResult computeCapsuleVolumeMassProperties(const hkVector4f& startAxis, const hkVector4f& endAxis, float radius, float mass, hkMassProperties& result); // 0x00F3A940
    static hkResult computeSphereSurfaceMassProperties(float radius, float mass, hkMassProperties& result); // 0x00F37E60
    static hkResult computeSphereSurfaceMassProperties(float radius, float mass, float surfaceThickness, hkMassProperties& result); // 0x00F383C0
    static hkResult computeBoxSurfaceMassProperties(const hkVector4f& halfExtents, float mass, float surfaceThickness, hkMassProperties& result); // 0x00F38870
    static hkResult computeTriangleSurfaceMassProperties(const hkVector4f& v0, const hkVector4f& v1, const hkVector4f& v2, float mass, float surfaceThickness, hkMassProperties& result); // 0x00F39D30
    static hkResult computeCylinderVolumeMassProperties(const hkVector4f& startAxis, const hkVector4f& endAxis, float radius, float mass, hkMassProperties& result); // 0x00F3B330
    static hkResult computeConvexHullMassProperties(const hkStridedVertices& vertices, float radius, hkMassProperties& result); // 0x00F3A4E0
public:
    static hkResult computeApproximateConvexHullMassProperties(const hkStridedVertices& vertices, float radius, hkMassProperties& result); // 0x00F39B00
public:
    static hkResult computeVertexHullVolumeMassProperties(const float* vertexIn, int striding, int numVertices, float mass, hkMassProperties& result); // 0x00F3A8B0
    static hkResult computeVertexCloudMassProperties(const float* vertexIn, int striding, int numVertices, float mass, hkMassProperties& result); // 0x00F3A2C0
    static hkResult computeGeometrySurfaceMassProperties(const hkGeometry* geom, float surfaceThickness, hkBool distributeUniformly, float mass, hkMassProperties& result); // 0x00F3A540
    static void computeGeometryVolumeMassProperties(const hkGeometry* geom, float mass, hkMassProperties& result); // 0x00F39500
    static hkResult computeGeometryVolumeMassPropertiesChecked(const hkGeometry* geom, float mass, hkMassProperties& result); // 0x00F39830
    static hkResult combineMassProperties(const hkArrayBase<hkMassElement>& elements, hkMassProperties& result); // 0x00F38650
    static void simplifyInertiaTensorToOrientedParticle(hkMatrix3f& inertia); // 0x00F387F0
    static void convertInertiaTensorToPrincipleAxis(hkMatrix3f& inertia, hkRotationf& principleAxisOut); // 0x00F39B90
    static void shiftInertiaToCom(const hkVector4f& shift, const hkSimdFloat32& mass, hkMatrix3f& inertia); // 0x00F37FB0
    static void shiftInertiaFromCom(const hkVector4f& shift, const hkSimdFloat32& mass, hkMatrix3f& inertia); // 0x00F38050

    static hkResult (*s_computeConvexHullMassPropertiesFunction)(const hkStridedVertices&, float, hkMassProperties&); // 0x03A380D4
};
