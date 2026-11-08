// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintdata.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactpointevent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpdynamicscpidmgr.h"

class hkBool;
class hkContactPoint;
class hkFinishLoadedObjectFlag;
struct hkpBodyVelocity;
class hkpConstraintInstance;
class hkpConstraintOwner;
class hkpConstraintQueryIn;
struct hkpConstraintRuntime;
class hkpContactPointProperties;
class hkpRigidBody;
class hkpSimpleConstraintContactMgr;
struct hkpSimpleConstraintInfoInitInput;
struct hkpSimpleContactConstraintAtom;
class hkpSolverResults;

// size 0x30
class hkpSimpleContactConstraintData : public hkpConstraintData
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00E1B850
    virtual ~hkpSimpleContactConstraintData(); // vtbl+0x00 0x00E1B7D0
    int getNumContactPoints() const; // 0x00F20C60
    unsigned short getContactPointIdAt(int) const;
    hkContactPoint& getContactPoint(int id); // 0x00E1B2E0
    const hkContactPoint& getContactPoint(int) const;
    hkpContactPointProperties* getContactPointProperties(int id); // 0x00E1B300
    unsigned short allocateContactPoint(hkpConstraintOwner& constraintOwner, hkContactPoint** cpOut, hkpContactPointProperties** cpPropsOut); // 0x00E2FC10
    int freeContactPoint(hkpConstraintOwner& constraintOwner, unsigned short id); // 0x00E1A710
    virtual int getType() const; // vtbl+0x10 0x00E2F550
    virtual void getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const; // vtbl+0x14 0x00E2F570
    virtual hkBool isValid() const; // vtbl+0x18 0x00E2F540
    virtual void getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const; // vtbl+0x4C 0x00E2F510
    virtual hkpSolverResults* getSolverResults(hkpConstraintRuntime* runtime) const; // vtbl+0x50 0x00E2F530
    virtual void collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // vtbl+0x68 0x00E2FB40
    virtual void collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // vtbl+0x6C 0x00E2FE00
    hkpSimpleConstraintContactMgr* getSimpleConstraintContactMgr() const; // 0x00E2F610
public:
    hkpSimpleContactConstraintData(hkpConstraintInstance* constraint, hkpRigidBody* bodyA, hkpRigidBody* bodyB); // 0x00E2FD40
public:
    hkpSimpleContactConstraintData(hkFinishLoadedObjectFlag);

    hkpDynamicsCpIdMgr m_idMgrA; // 0x0C
    void* m_clientData; // 0x20
    hkpConstraintInstance* m_constraint; // 0x24
    hkpSimpleContactConstraintAtom* m_atom; // 0x28
    int m_atomSize; // 0x2C
};

void hkSimpleContactConstraintData_fireCallbacks(hkpSimpleContactConstraintData* constraintData, const hkpConstraintQueryIn* in, hkpSimpleContactConstraintAtom* atom, hkpContactPointEvent::Type type); // 0x00E2F620
