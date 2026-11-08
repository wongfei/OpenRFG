// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7, HK8): the constraint data base, from rfg.exe. getConstraintInfoUtil came with the
// ragdoll joints (multiplayer.md MP7).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"

#include <string.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"

// 0x00D70A40: the schema sizes, solver results and solver element temps of an atom list (the header's 16 bytes
// first). m_extraSchemaSize is only added to (the ConstraintInfo constructor zeroes it). A bridge or modifier ends the
// list; padding (type 0) skips to the next 16 bytes.
void hkpConstraintData::getConstraintInfoUtil(const hkpConstraintAtom* atoms, int sizeOfAllAtoms, ConstraintInfo& infoOut)
{
    infoOut.m_sizeOfAllAtoms = sizeOfAllAtoms;
    const char* atom = reinterpret_cast<const char*>(atoms);
    const char* atomsEnd = atom + sizeOfAllAtoms;
    infoOut.m_atoms = const_cast<hkpConstraintAtom*>(atoms);
    infoOut.m_maxSizeOfSchema = 0;
    infoOut.m_numSolverResults = 0;
    infoOut.m_numSolverElemTemps = 0;
    infoOut.m_sizeOfSchemas = 0x10;
    // per type: schema bytes, solver results, solver element temps, extra schema bytes and the atom's size
    while (atom < atomsEnd)
    {
        const unsigned short type = *reinterpret_cast<const unsigned short*>(atom);
        int schemaSize = 0;
        int results = 0;
        int temps = 0;
        int extra = 0;
        int atomSize = 0x10;
        switch (type)
        {
        case hkpConstraintAtom::TYPE_INVALID:
            atom = reinterpret_cast<const char*>((reinterpret_cast<unsigned int>(atom) + 0xF) & ~0xFu);
            continue;
        case hkpConstraintAtom::TYPE_BRIDGE:
        case hkpConstraintAtom::TYPE_MODIFIER_SOFT_CONTACT:
        case hkpConstraintAtom::TYPE_MODIFIER_MASS_CHANGER:
        case hkpConstraintAtom::TYPE_MODIFIER_VISCOUS_SURFACE:
        case hkpConstraintAtom::TYPE_MODIFIER_MOVING_SURFACE:
        case hkpConstraintAtom::TYPE_MODIFIER_IGNORE_CONSTRAINT:
            return;
        case hkpConstraintAtom::TYPE_SET_LOCAL_TRANSFORMS: atomSize = 0x90; break;
        case hkpConstraintAtom::TYPE_SET_LOCAL_TRANSLATIONS: atomSize = 0x30; break;
        case hkpConstraintAtom::TYPE_SET_LOCAL_ROTATIONS: atomSize = 0x70; break;
        // the largest of 3 bilateral schemas (0x90) and the stable ball socket (0x40): the extra size is the difference
        case hkpConstraintAtom::TYPE_BALL_SOCKET: schemaSize = 0x90; results = temps = 3; extra = -0x10; break;
        case hkpConstraintAtom::TYPE_STIFF_SPRING: schemaSize = 0x40; results = temps = 1; extra = 0x40; atomSize = 0x20; break;
        case hkpConstraintAtom::TYPE_LIN: schemaSize = 0x30; results = temps = 1; break;
        case hkpConstraintAtom::TYPE_LIN_SOFT:
        case hkpConstraintAtom::TYPE_LIN_LIMIT:
        case hkpConstraintAtom::TYPE_RACK_AND_PINION:
        case hkpConstraintAtom::TYPE_COG_WHEEL: schemaSize = 0x40; results = temps = 1; break;
        case hkpConstraintAtom::TYPE_LIN_FRICTION:
        case hkpConstraintAtom::TYPE_2D_ANG: schemaSize = 0x40; results = temps = 2; break;
        case hkpConstraintAtom::TYPE_LIN_MOTOR: schemaSize = 0x50; results = temps = 2; atomSize = 0x20; break;
        case hkpConstraintAtom::TYPE_ANG:
        {
            const int numAxes = reinterpret_cast<const unsigned char*>(atom)[5]; // m_numConstrainedAxes
            schemaSize = numAxes * 0x20;
            results = temps = numAxes;
            break;
        }
        case hkpConstraintAtom::TYPE_ANG_LIMIT:
        case hkpConstraintAtom::TYPE_TWIST_LIMIT:
        case hkpConstraintAtom::TYPE_CONE_LIMIT: schemaSize = 0x30; results = temps = 1; atomSize = 0x20; break;
        case hkpConstraintAtom::TYPE_ANG_FRICTION:
        {
            const int numAxes = reinterpret_cast<const hkpAngFrictionConstraintAtom*>(atom)->m_numFrictionAxes;
            schemaSize = numAxes * 0x30;
            results = temps = numAxes * 2;
            break;
        }
        case hkpConstraintAtom::TYPE_ANG_MOTOR: schemaSize = 0x40; results = temps = 2; atomSize = 0x20; break;
        case hkpConstraintAtom::TYPE_RAGDOLL_MOTOR: schemaSize = 0xC0; results = temps = 6; atomSize = 0x50; break;
        case hkpConstraintAtom::TYPE_PULLEY: schemaSize = 0x40; results = temps = 1; atomSize = 0x40; break;
        case hkpConstraintAtom::TYPE_SETUP_STABILIZATION: break;
        case hkpConstraintAtom::TYPE_3D_ANG: schemaSize = 0x30; results = temps = 3; extra = 0x30; break;
        case hkpConstraintAtom::TYPE_DEFORMABLE_3D_LIN:
        case hkpConstraintAtom::TYPE_DEFORMABLE_3D_ANG: schemaSize = 0x80; results = temps = 3; extra = 0x40; atomSize = 0x60; break;
        case hkpConstraintAtom::TYPE_WHEEL_FRICTION: schemaSize = 0x70; temps = 2; atomSize = 0x30; break;
        case hkpConstraintAtom::TYPE_ELLIPTICAL_LIMIT: schemaSize = 0x70; results = temps = 2; atomSize = 0x30; break;
        default:
            // TYPE_OVERWRITE_PIVOT, TYPE_CONTACT and TYPE_MODIFIER_CENTER_OF_MASS_CHANGER do not advance in the binary (it
            // loops forever); unreachable here
            hkErrorFwd::messageError(0x4F704551, "OpenRFG: getConstraintInfoUtil: atom type not handled", "hkpconstraintdata.cpp", __LINE__);
            return;
        }
        infoOut.m_sizeOfSchemas += schemaSize;
        infoOut.m_numSolverResults += results;
        infoOut.m_numSolverElemTemps += temps;
        infoOut.m_extraSchemaSize += extra;
        atom += atomSize;
    }
}

// 0x00DF5820: only the extra schema size starts at 0
hkpConstraintData::ConstraintInfo::ConstraintInfo()
{
    m_extraSchemaSize = 0;
}

// 0x00D708F0 (stub_impl.md S4 SD)
hkpConstraintData::hkpConstraintData(hkFinishLoadedObjectFlag f)
    : hkReferencedObject(f)
{
}

// 0x00D6D0A0
hkpConstraintData::hkpConstraintData()
{
    m_userData = 0;
}

// 0x00D6D0D0
hkpConstraintData::~hkpConstraintData()
{
}

// 0x00D70940
void hkpConstraintData::setMaximumLinearImpulse(float maxLinearImpulse)
{
}

// 0x00D70950
void hkpConstraintData::setMaximumAngularImpulse(float maxAngularImpulse)
{
}

// 0x00D70960
void hkpConstraintData::setBreachImpulse(float breachImpulse)
{
}

// 0x00D70970
float hkpConstraintData::getMaximumLinearImpulse() const
{
    return HK_REAL_MAX;
}

// 0x00D70980
float hkpConstraintData::getMaximumAngularImpulse() const
{
    return HK_REAL_MAX;
}

// 0x00D70990
float hkpConstraintData::getBreachImpulse() const
{
    return HK_REAL_MAX;
}

// 0x00D709B0
void hkpConstraintData::setBodyToNotify(int bodyIdx)
{
}

// 0x00D709A0
unsigned char hkpConstraintData::getNotifiedBodyIndex() const
{
    return 0xFF;
}

// 0x00D709C0
void hkpConstraintData::setSolvingMethod(hkpConstraintAtom::SolvingMethod method)
{
}

// 0x00D709D0
hkResult hkpConstraintData::setInertiaStabilizationFactor(const float inertiaStabilizationFactor)
{
    return HK_FAILURE;
}

// 0x00D709E0
hkResult hkpConstraintData::getInertiaStabilizationFactor(float& inertiaStabilizationFactorOut) const
{
    return HK_FAILURE;
}

// 0x00D70A10
void hkpConstraintData::getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtime, hkVector4f& impulseOut) const
{
    impulseOut.setZero();
}

// 0x00D70930
hkpSolverResults* hkpConstraintData::getSolverResults(hkpConstraintRuntime* runtime) const
{
    return reinterpret_cast<hkpSolverResults*>(runtime);
}

// 0x00D70900: a runtime starts zeroed
void hkpConstraintData::addInstance(hkpConstraintRuntime* runtime, int sizeOfRuntime) const
{
    if (runtime != nullptr)
    {
        memset(runtime, 0, sizeOfRuntime);
    }
}

// 0x00D70920
hkpConstraintData::UpdateAtomsResult::Enum hkpConstraintData::updateDirtyAtoms()
{
    return UpdateAtomsResult::RUNTIME_UNCHANGED;
}

// 0x00D709F0
void hkpConstraintData::buildJacobian(const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
}

// 0x00D6D0C0
hkBool hkpConstraintData::isBuildJacobianCallbackRequired() const
{
    return false;
}

// 0x00D70A00
void hkpConstraintData::buildJacobianCallback(const hkpConstraintQueryIn& in, const hkpConstraintQueryOut& out)
{
}
