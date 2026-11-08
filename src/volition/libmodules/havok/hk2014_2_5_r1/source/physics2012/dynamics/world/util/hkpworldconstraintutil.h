// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldconstraintutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"

struct hkConstraintInternal;
class hkpConstraintInstance;
class hkpConstraintOwner;
class hkpEntity;
struct hkpModifierConstraintAtom;
class hkpWorld;

// size 0x1
class hkpWorldConstraintUtil
{
public:
    static void addConstraint(hkpWorld* world, hkpConstraintInstance* constraint); // 0x00E0F360
    static void removeConstraint(hkpConstraintInstance* constraint); // 0x00E0EF10
    static hkpConstraintInstance* getConstraint(const hkpEntity* entityA, const hkpEntity* entityB); // 0x00E0EE90
    static void addModifier(hkpConstraintInstance* instance, hkpConstraintOwner& constraintOwner, hkpModifierConstraintAtom* newModifier); // 0x00E0F200
    static void removeAndDeleteModifier(hkpConstraintInstance* instance, hkpConstraintOwner& constraintOwner, hkpConstraintAtom::AtomType type); // 0x00E0F070
    static hkpModifierConstraintAtom* findModifier(hkpConstraintInstance* instance, hkpConstraintAtom::AtomType type); // 0x00E0F1A0
    static hkpModifierConstraintAtom* findLastModifier(hkpConstraintInstance* instance); // 0x00E0F1D0
    static void updateFatherOfMovedAtom(hkpConstraintInstance* instance, const hkpConstraintAtom* oldAtom, const hkpConstraintAtom* updatedAtom, int updatedSizeOfAtom); // 0x00E0EE10
    static hkpConstraintAtom* getTerminalAtom(const hkConstraintInternal* cInternal); // 0x00DEBAC0

};
