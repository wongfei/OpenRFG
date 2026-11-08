// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatomutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

struct hkpSimpleContactConstraintAtom;

// size 0x1
class hkpSimpleContactConstraintAtomUtil
{
public:
    static hkpSimpleContactConstraintAtom* expandOne(hkpSimpleContactConstraintAtom* oldAtom_mightGetDeallocated, hkPadSpu<bool>& atomReallocated); // 0x00E30CD0
    static void removeAtAndCopy(hkpSimpleContactConstraintAtom* atom, int index); // 0x00E1B500
    static hkpSimpleContactConstraintAtom* optimizeCapacity(hkpSimpleContactConstraintAtom* oldAtom_mightGetDeallocated, int numFreeElemsLeft, hkPadSpu<bool>& atomReallocated); // 0x00E30D80
    static hkpSimpleContactConstraintAtom* allocateAtom(int numReservedContactPoints, int numExtraUserDatasA, int numExtraUserDatasB, int maxNumContactPoints); // 0x00E30B00
    static void copyContents(hkpSimpleContactConstraintAtom* dst, const hkpSimpleContactConstraintAtom* src); // 0x00E30BB0
    static void deallocateAtom(hkpSimpleContactConstraintAtom* atom); // 0x00E1B670
    static int calculateAtomSize(int, int);

};

int atomSizeRoundUp(int size); // 0x00E30A10
