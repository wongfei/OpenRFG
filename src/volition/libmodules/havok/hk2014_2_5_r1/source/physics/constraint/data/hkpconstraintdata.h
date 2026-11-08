// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintinfo.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
struct hkResult;
class hkTransformf;
class hkVector4f;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;
struct hkpConstraintRuntime;
class hkpSolverResults;

// size 0xC
class hkpConstraintData : public hkReferencedObject
{
public:
    enum ConstraintType
    {
        CONSTRAINT_TYPE_BALLANDSOCKET = 0,
        CONSTRAINT_TYPE_HINGE = 1,
        CONSTRAINT_TYPE_LIMITEDHINGE = 2,
        CONSTRAINT_TYPE_POINTTOPATH = 3,
        CONSTRAINT_TYPE_PRISMATIC = 6,
        CONSTRAINT_TYPE_RAGDOLL = 7,
        CONSTRAINT_TYPE_STIFFSPRING = 8,
        CONSTRAINT_TYPE_WHEEL = 9,
        CONSTRAINT_TYPE_GENERIC = 10,
        CONSTRAINT_TYPE_CONTACT = 11,
        CONSTRAINT_TYPE_BREAKABLE = 12,
        CONSTRAINT_TYPE_MALLEABLE = 13,
        CONSTRAINT_TYPE_POINTTOPLANE = 14,
        CONSTRAINT_TYPE_PULLEY = 15,
        CONSTRAINT_TYPE_ROTATIONAL = 16,
        CONSTRAINT_TYPE_HINGE_LIMITS = 18,
        CONSTRAINT_TYPE_RAGDOLL_LIMITS = 19,
        CONSTRAINT_TYPE_CUSTOM = 20,
        CONSTRAINT_TYPE_RACK_AND_PINION = 21,
        CONSTRAINT_TYPE_COG_WHEEL = 22,
        CONSTRAINT_TYPE_FIXED = 23,
        CONSTRAINT_TYPE_DEFORMABLE_FIXED = 24,
        CONSTRAINT_TYPE_LINEAR_SLACK = 25,
        CONSTRAINT_TYPE_ANGULAR_FRICTION = 26,
        CONSTRAINT_TYPE_6DOF = 27,
        BEGIN_CONSTRAINT_CHAIN_TYPES = 100,
        CONSTRAINT_TYPE_STIFF_SPRING_CHAIN = 100,
        CONSTRAINT_TYPE_BALL_SOCKET_CHAIN = 101,
        CONSTRAINT_TYPE_POWERED_CHAIN = 102,
    };

    // size 0x1C
    struct ConstraintInfo : public hkpConstraintInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ConstraintInfo);
        ConstraintInfo(); // 0x00DF5820

        hkpConstraintAtom* m_atoms; // 0x10
        unsigned int m_sizeOfAllAtoms; // 0x14
        unsigned int m_extraSchemaSize; // 0x18
    };

    // size 0x8
    struct RuntimeInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(RuntimeInfo);

        int m_sizeOfExternalRuntime; // 0x00
        int m_numSolverResults; // 0x04
    };

    // size 0x1
    struct UpdateAtomsResult
    {
        enum Enum
        {
            RUNTIME_UNCHANGED = 0,
            RUNTIME_CHANGED = 1,
            RUNTIME_SIZE_CHANGED = 2,
        };

    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00D6D300
    static const hkClass& staticClass(); // 0x01038460
    hkpConstraintData(hkFinishLoadedObjectFlag f); // 0x00D708F0
    hkpConstraintData(); // 0x00D6D0A0
    virtual ~hkpConstraintData(); // vtbl+0x00 0x00D6D0D0
    virtual int getType() const = 0; // vtbl+0x10
    virtual void getConstraintInfo(ConstraintInfo&) const = 0; // vtbl+0x14
    virtual hkBool isValid() const = 0; // vtbl+0x18
    virtual void setMaximumLinearImpulse(float maxLinearImpulse); // vtbl+0x1C 0x00D70940
    virtual void setMaximumAngularImpulse(float maxAngularImpulse); // vtbl+0x20 0x00D70950
    virtual void setBreachImpulse(float breachImpulse); // vtbl+0x24 0x00D70960
    virtual float getMaximumLinearImpulse() const; // vtbl+0x28 0x00D70970
    virtual float getMaximumAngularImpulse() const; // vtbl+0x2C 0x00D70980
    virtual float getBreachImpulse() const; // vtbl+0x30 0x00D70990
    virtual void setBodyToNotify(int bodyIdx); // vtbl+0x34 0x00D709B0
    virtual unsigned char getNotifiedBodyIndex() const; // vtbl+0x38 0x00D709A0
    virtual void setSolvingMethod(hkpConstraintAtom::SolvingMethod method); // vtbl+0x3C 0x00D709C0
    virtual hkResult setInertiaStabilizationFactor(const float inertiaStabilizationFactor); // vtbl+0x40 0x00D709D0
    virtual hkResult getInertiaStabilizationFactor(float& inertiaStabilizationFactorOut) const; // vtbl+0x44 0x00D709E0
    virtual void getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtime, hkVector4f& impulseOut) const; // vtbl+0x48 0x00D70A10
    virtual void getRuntimeInfo(hkBool, RuntimeInfo&) const = 0; // vtbl+0x4C
    virtual hkpSolverResults* getSolverResults(hkpConstraintRuntime* runtime) const; // vtbl+0x50 0x00D70930
    virtual void addInstance(hkpConstraintRuntime* runtime, int sizeOfRuntime) const; // vtbl+0x54 0x00D70900
    virtual UpdateAtomsResult::Enum updateDirtyAtoms(); // vtbl+0x58 0x00D70920
    virtual void buildJacobian(const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out); // vtbl+0x5C 0x00D709F0
    virtual hkBool isBuildJacobianCallbackRequired() const; // vtbl+0x60 0x00D6D0C0
    virtual void buildJacobianCallback(const hkpConstraintQueryIn& in, const hkpConstraintQueryOut& out); // vtbl+0x64 0x00D70A00
    static void getConstraintInfoUtil(const hkpConstraintAtom* atoms, int sizeOfAllAtoms, ConstraintInfo& infoOut); // 0x00D70A40

    static const hkInternalClassMember Members[];
    unsigned long m_userData; // 0x08
};
