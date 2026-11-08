// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/tyremarks/hkptyremarksinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleData;
class hkpVehicleInstance;

// size 0x20
struct hkpTyremarkPoint
{
    static const hkClass& staticClass();
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpTyremarkPoint);
    hkpTyremarkPoint(hkFinishLoadedObjectFlag);
    hkpTyremarkPoint(); // 0x00F7F840
    float getTyremarkStrength() const; // 0x00F7F870

    static const hkInternalClassMember Members[];
    hkVector4f m_pointLeft; // 0x00
    hkVector4f m_pointRight; // 0x10
};

// size 0x1C
class hkpTyremarksWheel : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F7FE60, delete(void* p, unsigned int nbytes) 0x00F80160
    static const hkClass& staticClass();
    hkpTyremarksWheel(hkFinishLoadedObjectFlag);
    hkpTyremarksWheel(); // 0x00F7FB30
    virtual ~hkpTyremarksWheel(); // vtbl+0x00 0x00F801E0
    void setNumPoints(int num_points); // 0x00F7FAA0
    void addTyremarkPoint(hkpTyremarkPoint& point); // 0x00F7F8D0
    const hkpTyremarkPoint& getTyremarkPoint(int point) const; // 0x00F7F850

    static const hkInternalClassMember Members[];
    int m_currentPosition; // 0x08
    int m_numPoints; // 0x0C
    hkArray<hkpTyremarkPoint,hkContainerHeapAllocator> m_tyremarkPoints; // 0x10
};

// size 0x1C
class hkpTyremarksInfo : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B713C0, delete(void* p, unsigned int nbytes) 0x00F801A0
    static const hkClass& staticClass();
    virtual ~hkpTyremarksInfo(); // vtbl+0x00 0x00F7FC30
    virtual void updateTyremarksInfo(float timestep, const hkpVehicleInstance* vehicle); // vtbl+0x10 0x00F7F930
    hkpTyremarksInfo(hkFinishLoadedObjectFlag);
    hkpTyremarksInfo(const hkpVehicleData& data, int num_points); // 0x00F7FB60
    virtual void getWheelTyremarksStrips(const hkpVehicleInstance* vehicle, int wheel, hkVector4f* strips_out) const; // vtbl+0x14 0x00F7F880

    static const hkInternalClassMember Members[];
    float m_minTyremarkEnergy; // 0x08
    float m_maxTyremarkEnergy; // 0x0C
    hkArray<hkpTyremarksWheel*,hkContainerHeapAllocator> m_tyremarksWheel; // 0x10
};
