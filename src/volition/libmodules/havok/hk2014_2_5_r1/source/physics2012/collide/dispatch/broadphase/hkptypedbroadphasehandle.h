// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandle.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;

// size 0xC
class hkpTypedBroadPhaseHandle : public hkpBroadPhaseHandle
{
public:
    enum
    {
        OFFSET_INVALID = 127,
    };

    static const hkClass& staticClass(); // 0x00D7D570
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpTypedBroadPhaseHandle); // new(unsigned int n, void* p) 0x00D7D5B0
    hkpTypedBroadPhaseHandle(hkFinishLoadedObjectFlag flag); // 0x00D7D5C0
    hkpTypedBroadPhaseHandle(void*, int);
    hkpTypedBroadPhaseHandle(int type); // 0x00D9EC70
    int getType() const; // 0x00710F70 (hkptypedbroadphasehandle.inl:34)
    void setOwner(void* o); // 0x00D7D8B0
    void* getOwner() const; // 0x0080CC60 (hkptypedbroadphasehandle.inl:44)
    unsigned int getCollisionFilterInfo() const; // 0x0080CC70 (hkptypedbroadphasehandle.inl:50)
    void setCollisionFilterInfo(unsigned int info); // 0x0080CC80 (hkptypedbroadphasehandle.inl:55)
public:
    void setType(int type); // 0x00DF36A0

public:
    static const hkInternalClassMember Members[]; // 0x01355370
public:
    signed char m_type; // 0x04
    signed char m_ownerOffset; // 0x05
public:
    signed char m_objectQualityType; // 0x06
    unsigned int m_collisionFilterInfo; // 0x08
};
