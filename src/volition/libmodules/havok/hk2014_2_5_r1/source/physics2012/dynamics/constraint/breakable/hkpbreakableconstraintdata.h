// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/breakable/hkpbreakableconstraintdata.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/bridge/hkpbridgeconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/wrapper/hkpwrappedconstraintdata.h"

class hkClass;
class hkpConstraintInstance;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;
struct hkpConstraintRuntime;

// size 0x30
class hkpBreakableConstraintData : public hkpWrappedConstraintData
{
public:
    // size 0x34
    struct Runtime
    {
        hkBool m_isBroken; // 0x00
        float m_linearVelcityA[3]; // 0x04
        float m_linearVelcityB[3]; // 0x10
        float m_angularVelcityA[3]; // 0x1C
        float m_angularVelcityB[3]; // 0x28
    };

    static const hkClass& staticClass(); // 0x00DF0800
    hkpBreakableConstraintData(hkFinishLoadedObjectFlag f); // 0x00DF5340
    hkpBreakableConstraintData(hkpConstraintData* constraintData); // 0x00DF53B0
    virtual ~hkpBreakableConstraintData(); // vtbl+0x00 0x00DF5380
    void setThreshold(float thresh); // 0x0080DAC0 (hkpbreakableconstraintdata.inl:13)
    void setRemoveWhenBroken(hkBool tf); // 0x0080DAD0 (hkpbreakableconstraintdata.inl:23)
    void setRevertBackVelocityOnBreak(hkBool b); // 0x00DF50D0
    hkBool getRemoveWhenBroken() const; // 0x00DF50C0
    hkBool getRevertBackVelocityOnBreak() const; // 0x00DF50E0
    hkBool getIsBroken(const hkpConstraintInstance*);
    void setBroken(hkpConstraintInstance* instance, hkBool broken, float currentForce); // 0x00DF51F0
    float getThreshold() const; // 0x00DF50B0
    virtual int getType() const; // vtbl+0x10 0x00DF51E0
    virtual void getConstraintInfo(hkpConstraintData::ConstraintInfo& info) const; // vtbl+0x14 0x00DF52D0
    Runtime* getRuntime(hkpConstraintRuntime* runtime); // 0x00DF5840
    virtual void getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const; // vtbl+0x4C 0x00DF51C0
    static void postFinish(void* data); // 0x00DF5300
    void buildNopJacobian(const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // 0x00DF51A0
    virtual void buildJacobian(const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // vtbl+0x5C 0x00DF55C0
    virtual hkBool isBuildJacobianCallbackRequired() const; // vtbl+0x60 0x00DF5920
    virtual void buildJacobianCallback(const hkpConstraintQueryIn& in, const hkpConstraintQueryOut& out); // vtbl+0x64 0x00DF5420
    virtual hkpWrappedConstraintData* deepClone() const; // vtbl+0x70 0x00DF56C0
    void afterReflectNew(); // 0x00DF5190

    hkpBridgeAtoms m_atoms; // 0x10
    unsigned short m_childRuntimeSize; // 0x20
    unsigned short m_childNumSolverResults; // 0x22
    float m_solverResultLimit; // 0x24
    hkBool m_removeWhenBroken; // 0x28
    hkBool m_revertBackVelocityOnBreak; // 0x29
    unsigned char m_padding[6]; // 0x2A: rfg.pdb aligns the class to 8 (the allocation is 0x30)
};
