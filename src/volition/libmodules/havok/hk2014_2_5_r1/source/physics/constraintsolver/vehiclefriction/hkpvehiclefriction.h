// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefriction.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;

// size 0xD8
class hkpVehicleFrictionDescription : public hkReferencedObject
{
public:
    enum
    {
        hkFricionEllipseLineSegments = 16,
    };

    // size 0xB0
    struct alignas(16) Cinfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Cinfo);

        hkVector4f m_wheelPosition[2]; // 0x00
        hkVector4f m_chassisCenterOfMass; // 0x20
        hkVector4f m_directionRight; // 0x30
        hkVector4f m_directionFront; // 0x40
        hkVector4f m_directionUp; // 0x50
        hkMatrix3f m_chassisFrictionInertiaInv; // 0x60
        float m_chassisMassInv; // 0x90
        float m_wheelAxleAngularInertia[2]; // 0x94
        float m_wheelRadius[2]; // 0x9C
        float m_frictionEqualizer; // 0xA4
    };

    // size 0x64
    struct AxisDescription
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(AxisDescription);
        static const hkClass& staticClass();

        static const hkInternalClassMember Members[];
        float m_frictionCircleYtab[16]; // 0x00
        float m_xStep; // 0x40
        float m_xStart; // 0x44
        float m_wheelSurfaceInertia; // 0x48
        float m_wheelSurfaceInertiaInv; // 0x4C
        float m_wheelChassisMassRatio; // 0x50
        float m_wheelRadius; // 0x54
        float m_wheelRadiusInv; // 0x58
        float m_wheelDownForceFactor; // 0x5C
        float m_wheelDownForceSumFactor; // 0x60
    };

    static const hkClass& staticClass();
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B71490, delete(void* p, unsigned int nbytes) 0x00B714B0
    hkpVehicleFrictionDescription(hkFinishLoadedObjectFlag);
    hkpVehicleFrictionDescription(); // 0x00B631C0 (.h:120)
    virtual ~hkpVehicleFrictionDescription(); // vtbl+0x00 0x00B631D0

    static const hkInternalClassMember Members[];
    float m_wheelDistance; // 0x08
    float m_chassisMassInv; // 0x0C
    AxisDescription m_axleDescr[2]; // 0x10
};

// size 0x48
struct hkpVehicleFrictionStatus
{
    // size 0x24
    struct AxisStatus
    {
        static const hkClass& staticClass();
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(AxisStatus);
        AxisStatus(hkFinishLoadedObjectFlag);
        AxisStatus(); // 0x00B631E0 (.h:136)
        void init(); // 0x00B631F0 (.h:165)

        static const hkInternalClassMember Members[];
        float m_forward_slip_velocity; // 0x00
        float m_side_slip_velocity; // 0x04
        float m_skid_energy_density; // 0x08
        float m_side_force; // 0x0C
        float m_delayed_forward_impulse; // 0x10
        float m_sideRhs; // 0x14
        float m_forwardRhs; // 0x18
        float m_relativeSideForce; // 0x1C
        float m_relativeForwardForce; // 0x20
    };

    static const hkClass& staticClass();
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpVehicleFrictionStatus);
    hkpVehicleFrictionStatus(hkFinishLoadedObjectFlag);
    hkpVehicleFrictionStatus(); // 0x00B63220 (.h:183)

    static const hkInternalClassMember Members[];
    AxisStatus m_axis[2]; // 0x00
};
