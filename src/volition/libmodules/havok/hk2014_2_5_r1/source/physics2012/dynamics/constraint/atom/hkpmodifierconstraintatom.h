// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"

class hkClass;
class hkContactPoint;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
struct hkpBodyVelocity;
struct hkpConstraintInfo;
struct hkpSimpleConstraintInfoInitInput;

// size 0x20
struct hkpModifierConstraintAtom : public hkpConstraintAtom
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpModifierConstraintAtom); // new(unsigned int n, void* p) 0x00DEDC40
    static const hkClass& staticClass(); // 0x00DEDD10
public:
    hkpModifierConstraintAtom(hkpConstraintAtom::AtomType, int);
public:
    hkpModifierConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 0x00DEDE00
    int addModifierDataToConstraintInfo(hkpConstraintInfo& cinfo, unsigned char& usedFlagsOut) const; // 0x00E30740
    static int addAllModifierDataToConstraintInfo(hkpModifierConstraintAtom* modifier, hkpConstraintInfo& cinfo, unsigned char& usedFlagsOut); // 0x00E307D0

    static const hkInternalClassMember Members[];
    alignas(16) unsigned short m_modifierAtomSize; // 0x10
    unsigned short m_childSize; // 0x12
    hkpConstraintAtom* m_child; // 0x14
    unsigned int m_pad[2]; // 0x18
};

// size 0x40
struct hkpMassChangerModifierConstraintAtom : public hkpModifierConstraintAtom
{
    static const hkClass& staticClass(); // 0x00DEDCF0
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMassChangerModifierConstraintAtom); // new(unsigned int n, void* p) 0x00DEDC50, new(unsigned int nbytes) 0x00A2D3B0
    hkpMassChangerModifierConstraintAtom(hkFinishLoadedObjectFlag f) : hkpModifierConstraintAtom(f) {} // 0x00DEDE10
    hkpMassChangerModifierConstraintAtom(); // 0x00A2D3D0 (.h:55)
    int numSolverResults() const; // 0x00E98640
    void collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // 0x00E30810
    void collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // 0x00E303C0
    int getConstraintInfo(hkpConstraintInfo& info) const; // 0x00E30370

    static const hkInternalClassMember Members[];
    static const unsigned short ADDITIONAL_SCHEMA_SIZE; // 0x012C70B8
    hkVector4f m_factorA; // 0x20
    hkVector4f m_factorB; // 0x30
};

// size 0x40
struct hkpCenterOfMassChangerModifierConstraintAtom : public hkpModifierConstraintAtom
{
    static const hkClass& staticClass(); // 0x00DEDCB0
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCenterOfMassChangerModifierConstraintAtom); // new(unsigned int n, void* p) 0x00DEDC60, new(unsigned int nbytes) 0x00E04270
    hkpCenterOfMassChangerModifierConstraintAtom(hkFinishLoadedObjectFlag f) : hkpModifierConstraintAtom(f) {} // 0x00DEDE20
    hkpCenterOfMassChangerModifierConstraintAtom(); // 0x00E04C00
    int numSolverResults() const; // 0x00E98650
    void collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // 0x00E30450
    void collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // 0x00E30520
    int getConstraintInfo(hkpConstraintInfo& info) const; // 0x00E30380

    static const hkInternalClassMember Members[];
    static const unsigned short ADDITIONAL_SCHEMA_SIZE; // 0x012C70BC
    hkVector4f m_displacementA; // 0x20
    hkVector4f m_displacementB; // 0x30
};

// size 0x30
struct alignas(16) hkpSoftContactModifierConstraintAtom : public hkpModifierConstraintAtom
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSoftContactModifierConstraintAtom); // new(unsigned int n, void* p) 0x00DEDC70, new(unsigned int nbytes) 0x00E04290
    static const hkClass& staticClass(); // 0x00DEDD50
    hkpSoftContactModifierConstraintAtom(hkFinishLoadedObjectFlag f) : hkpModifierConstraintAtom(f) {} // 0x00DEDE30
    hkpSoftContactModifierConstraintAtom(); // 0x00E04C20
    void collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // 0x00E305D0
    void collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // 0x00E303D0
    int numSolverResults() const; // 0x00E98660
    int getConstraintInfo(hkpConstraintInfo& info) const; // 0x00E30390

    static const hkInternalClassMember Members[];
    static const unsigned short ADDITIONAL_SCHEMA_SIZE; // 0x012C70C0
    float m_tau; // 0x20
    float m_maxAcceleration; // 0x24
};

// size 0x30
struct hkpMovingSurfaceModifierConstraintAtom : public hkpModifierConstraintAtom
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMovingSurfaceModifierConstraintAtom); // new(unsigned int n, void* p) 0x00DEDC90, new(unsigned int nbytes) 0x00E042D0
    static const hkClass& staticClass(); // 0x00DEDD30
    hkpMovingSurfaceModifierConstraintAtom(hkFinishLoadedObjectFlag f) : hkpModifierConstraintAtom(f) {} // 0x00DEDE50
    hkpMovingSurfaceModifierConstraintAtom(); // 0x00E04C70
    int numSolverResults() const; // 0x00E98680
    int getConstraintInfo(hkpConstraintInfo& info) const; // 0x00E303B0
    void collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // 0x00E30610
    void collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB); // 0x00E30670
    const hkVector4f& getVelocity() const; // 0x00E98690
    hkVector4f& getVelocity(); // 0x00E04260

    static const hkInternalClassMember Members[];
    static const unsigned short ADDITIONAL_SCHEMA_SIZE; // 0x012C70C8
    hkVector4f m_velocity; // 0x20
};

// size 0x20
struct hkpIgnoreModifierConstraintAtom : public hkpModifierConstraintAtom
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpIgnoreModifierConstraintAtom); // new(unsigned int n, void* p) 0x00DEDCA0, new(unsigned int nbytes) 0x00E042F0
    static const hkClass& staticClass(); // 0x00DEDCD0
    hkpIgnoreModifierConstraintAtom(hkFinishLoadedObjectFlag f) : hkpModifierConstraintAtom(f) {} // 0x00DEDE60
    hkpIgnoreModifierConstraintAtom(); // 0x00E04C90
    int numSolverResults() const; // 0x00E986A0
    int getConstraintInfo(hkpConstraintInfo& info) const; // 0x00E306D0

    static const hkInternalClassMember Members[];
};
