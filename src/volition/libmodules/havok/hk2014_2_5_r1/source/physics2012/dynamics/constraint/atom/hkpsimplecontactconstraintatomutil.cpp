// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatomutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the storage of a contact constraint's atom (see hkpsimplecontactconstraintatom.h),
// from rfg.exe. Atoms come from the heap in a few fixed size classes (atomSizeRoundUp), grow by doubling (expandOne)
// and shrink by halving (optimizeCapacity). removeAtAndCopy and deallocateAtom, inline in the SDK, are defined here.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatomutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h"

// 0x00E30A10: the allocation size classes; above 0x2000 bytes the size itself
int atomSizeRoundUp(int size)
{
    if (size <= 0x10) return 0x10;
    if (size <= 0x20) return 0x20;
    if (size <= 0x30) return 0x30;
    if (size <= 0x40) return 0x40;
    if (size <= 0x60) return 0x60;
    if (size <= 0x80) return 0x80;
    if (size <= 0xA0) return 0xA0;
    if (size <= 0xC0) return 0xC0;
    if (size <= 0x100) return 0x100;
    if (size <= 0x140) return 0x140;
    if (size <= 0x200) return 0x200;
    if (size <= 0x280) return 0x280;
    if (size <= 0x400) return 0x400;
    if (size <= 0x800) return 0x800;
    if (size <= 0x1000) return 0x1000;
    if (size <= 0x2000) return 0x2000;
    return size;
}

// 0x00E30B00: the striding is the properties (0x14) plus one word per extended user data; m_sizeOfAllAtoms is the
// size before atomSizeRoundUp. Only the info is initialized, not the points.
hkpSimpleContactConstraintAtom* hkpSimpleContactConstraintAtomUtil::allocateAtom(int numReservedContactPoints, int numExtraUserDatasA, int numExtraUserDatasB, int maxNumContactPoints)
{
    const int striding = ((numExtraUserDatasA + numExtraUserDatasB) * 4 + 0x17) & ~3;
    const int size = ((striding + int(sizeof(hkContactPoint))) * numReservedContactPoints + 0x3F) & ~0xF; // + sizeof(atom) rounded to 16
    hkpSimpleContactConstraintAtom* atom = static_cast<hkpSimpleContactConstraintAtom*>(hkMemoryRouter::getInstance().heap().blockAlloc(atomSizeRoundUp(size)));
    atom->m_numUserDatasForBodyA = (unsigned char)numExtraUserDatasA;
    atom->m_numUserDatasForBodyB = (unsigned char)numExtraUserDatasB;
    atom->m_numReservedContactPoints = (unsigned short)numReservedContactPoints;
    atom->m_maxNumContactPoints = (unsigned short)maxNumContactPoints;
    atom->m_sizeOfAllAtoms = (unsigned short)size;
    atom->m_contactPointPropertiesStriding = (unsigned char)striding;
    atom->m_info.init();
    atom->m_type = hkpConstraintAtom::TYPE_CONTACT;
    atom->m_numContactPoints = 0;
    return atom;
}

// 0x00E30BB0: the info, then each point and its properties (word by word)
void hkpSimpleContactConstraintAtomUtil::copyContents(hkpSimpleContactConstraintAtom* dst, const hkpSimpleContactConstraintAtom* src)
{
    dst->m_info = src->m_info;
    dst->m_numContactPoints = src->m_numContactPoints;
    const int striding = src->m_contactPointPropertiesStriding;
    const hkContactPoint* srcCp = src->getContactPoints();
    hkContactPoint* dstCp = dst->getContactPoints();
    const unsigned int* srcProps = reinterpret_cast<const unsigned int*>(src->getContactPointPropertiesStream());
    unsigned int* dstProps = reinterpret_cast<unsigned int*>(dst->getContactPointPropertiesStream());
    for (int i = 0; i < src->m_numContactPoints; i++)
    {
        dstCp[i] = srcCp[i];
        for (int w = 0; w < (striding >> 2); w++)
        {
            dstProps[w] = srcProps[w];
        }
        srcProps = reinterpret_cast<const unsigned int*>(reinterpret_cast<const char*>(srcProps) + striding);
        dstProps = reinterpret_cast<unsigned int*>(reinterpret_cast<char*>(dstProps) + striding);
    }
}

// 0x00E30CD0: a full atom is reallocated with twice the points (at least 4, at most the maximum); the new point's
// properties are zeroed
hkpSimpleContactConstraintAtom* hkpSimpleContactConstraintAtomUtil::expandOne(hkpSimpleContactConstraintAtom* oldAtom_mightGetDeallocated, hkPadSpu<bool>& atomReallocated)
{
    hkpSimpleContactConstraintAtom* atom = oldAtom_mightGetDeallocated;
    const int numPoints = atom->m_numContactPoints;
    if (numPoints >= atom->m_numReservedContactPoints)
    {
        int newSize = numPoints > 2 ? numPoints : 2;
        newSize *= 2;
        const int maxNum = atom->m_maxNumContactPoints;
        if (newSize >= maxNum)
        {
            newSize = maxNum;
        }
        atom = allocateAtom(newSize, oldAtom_mightGetDeallocated->m_numUserDatasForBodyA, oldAtom_mightGetDeallocated->m_numUserDatasForBodyB, maxNum);
        copyContents(atom, oldAtom_mightGetDeallocated);
        deallocateAtom(oldAtom_mightGetDeallocated);
        atomReallocated = true;
    }
    atom->m_numContactPoints = (unsigned short)(numPoints + 1);
    unsigned int* props = reinterpret_cast<unsigned int*>(atom->getContactPointPropertiesStream(numPoints));
    for (int w = (atom->m_contactPointPropertiesStriding >> 2); w > 0; w--)
    {
        *props++ = 0;
    }
    return atom;
}

// 0x00E30D80: an atom at most half used (counting numFreeElemsLeft spare points) is reallocated at half its points
hkpSimpleContactConstraintAtom* hkpSimpleContactConstraintAtomUtil::optimizeCapacity(hkpSimpleContactConstraintAtom* oldAtom_mightGetDeallocated, int numFreeElemsLeft, hkPadSpu<bool>& atomReallocated)
{
    hkpSimpleContactConstraintAtom* atom = oldAtom_mightGetDeallocated;
    if ((atom->m_numContactPoints + numFreeElemsLeft) * 2 > atom->m_numReservedContactPoints)
    {
        return atom;
    }
    hkpSimpleContactConstraintAtom* newAtom = allocateAtom(atom->m_numReservedContactPoints >> 1, atom->m_numUserDatasForBodyA, atom->m_numUserDatasForBodyB, atom->m_maxNumContactPoints);
    copyContents(newAtom, atom);
    deallocateAtom(atom);
    atomReallocated = true;
    return newAtom;
}

// 0x00E1B500 (hkpsimpleconstraintcontactmgr.cpp): the later points and their properties move down by one
void hkpSimpleContactConstraintAtomUtil::removeAtAndCopy(hkpSimpleContactConstraintAtom* atom, int index)
{
    const int striding = atom->m_contactPointPropertiesStriding;
    const int last = atom->m_numContactPoints - 1;
    atom->m_numContactPoints = (unsigned short)last;
    hkContactPoint* cp = atom->getContactPoints() + index;
    if (index >= last)
    {
        return;
    }
    unsigned int* props = reinterpret_cast<unsigned int*>(atom->getContactPointPropertiesStream(index));
    for (int n = last - index; n > 0; n--)
    {
        cp[0] = cp[1];
        unsigned int* next = reinterpret_cast<unsigned int*>(reinterpret_cast<char*>(props) + striding);
        for (int w = 0; w < (striding >> 2); w++)
        {
            props[w] = next[w];
        }
        cp++;
        props = next;
    }
}

// 0x00E1B670
void hkpSimpleContactConstraintAtomUtil::deallocateAtom(hkpSimpleContactConstraintAtom* atom)
{
    hkMemoryRouter::getInstance().heap().blockFree(atom, atomSizeRoundUp(atom->m_sizeOfAllAtoms));
}

// 0x00E30030 (hkpsimplecontactconstraintatom.h): a header, the contacts as pairs plus a single, 2d friction growing to
// 3d friction from two points, and rolling friction when the multiplier is set
void hkpSimpleContactConstraintAtom::addToConstraintInfo(hkpConstraintInfo& infoOut) const
{
    const int size = m_numContactPoints;
    infoOut.m_sizeOfSchemas += hkpJacobianSchemaInfo::Header::Sizeof + (size >> 1) * hkpJacobianSchemaInfo::PairContact::Sizeof + (size & 1) * hkpJacobianSchemaInfo::SingleContact::Sizeof + hkpJacobianSchemaInfo::Friction2D::Sizeof;
    infoOut.m_numSolverResults += size + 2;
    infoOut.m_numSolverElemTemps += size + 3;
    infoOut.m_maxSizeOfSchema = infoOut.m_sizeOfSchemas + (hkpJacobianSchemaInfo::Friction3D::Sizeof - hkpJacobianSchemaInfo::Friction2D::Sizeof);
    if (size >= 2)
    {
        infoOut.m_sizeOfSchemas += hkpJacobianSchemaInfo::Friction3D::Sizeof - hkpJacobianSchemaInfo::Friction2D::Sizeof;
        infoOut.m_numSolverResults += 1;
        infoOut.m_numSolverElemTemps += 1;
    }
    if (m_info.m_rollingFrictionMultiplier.getReal() != 0.0f)
    {
        infoOut.m_sizeOfSchemas += hkpJacobianSchemaInfo::RollingFriction2D::Sizeof;
        infoOut.m_maxSizeOfSchema += hkpJacobianSchemaInfo::RollingFriction2D::Sizeof;
        infoOut.m_numSolverResults += 2;
        infoOut.m_numSolverElemTemps += 3;
    }
}
