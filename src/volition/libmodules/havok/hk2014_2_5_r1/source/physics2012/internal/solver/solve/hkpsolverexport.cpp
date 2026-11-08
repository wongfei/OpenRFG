// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolverexport.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the solver's results back into the constraints, from rfg.exe (written from the
// disassembly in its SSE operation order). Every contact row writes its accumulated impulse and the right hand side
// the next step starts from (the row's right hand side damped, minus what the summed velocities already did) into its
// solver results; the friction rows write theirs (scaled by the friction cone factor) into the contact's friction
// state. Contacts beyond 99% of their impulse limit are listed at the start of the schema buffer (the solve setup
// fires them). As in hkpsolver.cpp only the schemas of the ported constraint types are handled.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolverexport.h"

#include <math.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolve.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/contact/hkpsimplecontactconstraintinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

namespace
{
__forceinline Q loadQ(const void* p) { return _mm_load_ps(static_cast<const float*>(p)); }

// the summed velocity along a 1 linear 2 angular row: hadd3(((A.sumAng * ang0) + (B.sumAng * ang1)) + lin * dSumLin)
__forceinline Q rowSumVelocity(const float* jac, const Q& dSumLin, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB)
{
    return hadd3(_mm_add_ps(_mm_add_ps(_mm_mul_ps(loadQ(jac + 4), mA->m_scratch1.m_quad), _mm_mul_ps(loadQ(jac + 8), mB->m_scratch1.m_quad)), _mm_mul_ps(loadQ(jac), dSumLin)));
}

// the next right hand side of a row: rhs * factor - summed velocity * numSteps * deltaTime
__forceinline float nextRhs(float rhs, const Q& factor, const Q& sumVelocity, const Q& numStepsDt)
{
    return lane0(_mm_sub_ps(_mm_mul_ps(splat(rhs), factor), _mm_mul_ps(sumVelocity, numStepsDt)));
}

__forceinline void writeResult(char*& sr, int striding, float impulse, float rhs)
{
    hkpSolverResults* results = reinterpret_cast<hkpSolverResults*>(sr);
    results->m_impulseApplied = impulse;
    results->m_internalSolverData = rhs;
    sr += striding;
}
}

// 0x00EB6EA0
void hkExportImpulsesAndRhs(const hkpSolverInfo& i, const hkpSolverElemTemp* temp, const hkpJacobianSchema* schemas, const hkpVelocityAccumulator* accums)
{
    hkSolverExport::exportImpulsesAndRhs(i, temp, schemas, accums);
}

// 0x00EB5B10
void hkSolverExport::exportImpulsesAndRhs(const hkpSolverInfo& info, const hkpSolverElemTemp* solverInputs, const hkpJacobianSchema* schemas, const hkpVelocityAccumulator* accums)
{
    const Q deltaTime = splat(info.m_deltaTime);
    const Q dampTauDt = _mm_mul_ps(splat(info.m_dampDivTau), deltaTime);
    const Q dampFrictionDt = _mm_mul_ps(splat(info.m_dampDivFrictionTau), deltaTime);
    const Q numSteps = splat((float)info.m_numSteps);
    const Q numStepsDt = _mm_mul_ps(numSteps, deltaTime);
    // the accumulator copies an add-velocity schema works on (for the rest of its constraint)
    HK_ALIGN16(hkpVelocityAccumulator copyA);
    HK_ALIGN16(hkpVelocityAccumulator copyB);

    const char* schema = reinterpret_cast<const char*>(schemas);
    const hkpSolverElemTemp* temps = solverInputs;
    const hkpVelocityAccumulator* mA = nullptr;
    const hkpVelocityAccumulator* mB = nullptr;
    char* sr = nullptr;
    int striding = 0;
    hkpConstraintInstance* instance = nullptr;
    int numBreached = 0;
    bool bilateralBreached = false; // a limited bilateral row of the current run of them beyond 99%
    char* breached = const_cast<char*>(reinterpret_cast<const char*>(schemas));
    for (;;)
    {
        const unsigned char type = *reinterpret_cast<const unsigned char*>(schema);
        switch (type)
        {
        case hkpJacobianSchema::SCHEMA_TYPE_END:
            reinterpret_cast<hkpImpulseLimitBreachedHeader*>(breached)->m_numBreached = numBreached;
            return;

        case hkpJacobianSchema::SCHEMA_TYPE_HEADER:
        {
            const hkpJacobianHeaderSchema* header = reinterpret_cast<const hkpJacobianHeaderSchema*>(schema);
            mA = accums + header->m_bodyAIndex;
            striding = header->m_solverResultStriding;
            sr = reinterpret_cast<char*>(header->m_solverResultInMainMemory);
            mB = accums + header->m_bodyBIndex;
            instance = header->m_constraintInstance;
            schema += sizeof(hkpJacobianHeaderSchema);
            if (sr == nullptr)
            {
                // no results: the constraint's simple schemas are skipped
                for (unsigned char next = *reinterpret_cast<const unsigned char*>(schema); next > hkpJacobianSchema::SCHEMA_TYPE_SIMPLE_BEGIN && next < hkpJacobianSchema::SCHEMA_TYPE_SIMPLE_END; next = *reinterpret_cast<const unsigned char*>(schema))
                {
                    temps += hkpJacobianSchema::s_schemaNumSolverElemTemps[next];
                    schema += hkpJacobianSchema::s_schemaSize[next];
                }
            }
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_GOTO:
            schema += reinterpret_cast<const hkpJacobianGotoSchema*>(schema)->m_offset;
            break;

        case hkpJacobianSchema::SCHEMA_TYPE_SHIFT_SOLVER_RESULTS_POINTER:
            sr += reinterpret_cast<const hkpJacobianShiftSolverResultsPointerSchema*>(schema)->getNumSolverResults() * (int)(short)striding;
            schema += sizeof(hkpJacobianShiftSolverResultsPointerSchema);
            break;

        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_LIMITS:
        {
            // MP7: the angular right hand side by the step's delta time (not damped), minus the summed velocity
            const float* jac = reinterpret_cast<const float*>(schema);
            const Q v = hadd3(_mm_add_ps(_mm_mul_ps(mB->m_scratch1.m_quad, loadQ(jac + 4)), _mm_mul_ps(mA->m_scratch1.m_quad, loadQ(jac))));
            writeResult(sr, striding, temps[0].m_impulseApplied, nextRhs(jac[7], deltaTime, v, numStepsDt));
            temps += 1;
            schema += type == hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR ? hkpJacobianSchemaInfo::Angular1D::Sizeof : hkpJacobianSchemaInfo::AngularLimits1D::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_FRICTION:
        {
            // MP7: the damped right hand side scaled by the friction limit's factor (the second temp); the second
            // result is skipped
            const float* jac = reinterpret_cast<const float*>(schema);
            const Q v = hadd3(_mm_add_ps(_mm_mul_ps(mA->m_scratch1.m_quad, loadQ(jac)), _mm_mul_ps(mB->m_scratch1.m_quad, loadQ(jac + 4))));
            const Q rhs = _mm_mul_ps(splat(nextRhs(jac[7], dampTauDt, v, numStepsDt)), splat(temps[1].m_impulseApplied));
            hkpSolverResults* results = reinterpret_cast<hkpSolverResults*>(sr);
            results->m_impulseApplied = temps[0].m_impulseApplied;
            results->m_internalSolverData = lane0(rhs);
            sr += 2 * striding;
            temps += 2;
            schema += hkpJacobianSchemaInfo::AngularFriction1D::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_1D_FRICTION:
        {
            // a prismatic constraint's friction: the damped right hand side scaled by the friction limit's factor (the
            // second temp); the second result is skipped
            const float* jac = reinterpret_cast<const float*>(schema);
            const Q dSumLin = _mm_sub_ps(mA->m_scratch0.m_quad, mB->m_scratch0.m_quad);
            const Q v = hadd3(_mm_add_ps(_mm_add_ps(_mm_mul_ps(mB->m_scratch1.m_quad, loadQ(jac + 8)), _mm_mul_ps(mA->m_scratch1.m_quad, loadQ(jac + 4))), _mm_mul_ps(dSumLin, loadQ(jac))));
            const Q rhs = _mm_mul_ps(splat(nextRhs(jac[3], dampTauDt, v, numStepsDt)), splat(temps[1].m_impulseApplied));
            hkpSolverResults* results = reinterpret_cast<hkpSolverResults*>(sr);
            results->m_impulseApplied = temps[0].m_impulseApplied;
            results->m_internalSolverData = lane0(rhs);
            sr += 2 * striding;
            temps += 2;
            schema += hkpJacobianSchemaInfo::Friction1D::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_MOTOR:
        {
            // a hinge's motor: (target velocity * numSteps + the right hand side) by the step's delta time, minus the
            // summed velocity, scaled by the clamp's factor (the second temp); the second result is skipped
            const float* jac = reinterpret_cast<const float*>(schema);
            const hkpJacobian1dAngularMotorSchema* motor = reinterpret_cast<const hkpJacobian1dAngularMotorSchema*>(schema);
            const Q v = hadd3(_mm_add_ps(_mm_mul_ps(mA->m_scratch1.m_quad, loadQ(jac)), _mm_mul_ps(mB->m_scratch1.m_quad, loadQ(jac + 4))));
            const Q target = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(splat(motor->m_velocity), numSteps), splat(jac[7])), deltaTime);
            const Q rhs = _mm_mul_ps(_mm_sub_ps(target, _mm_mul_ps(v, numStepsDt)), splat(temps[1].m_impulseApplied));
            hkpSolverResults* results = reinterpret_cast<hkpSolverResults*>(sr);
            results->m_impulseApplied = temps[0].m_impulseApplied;
            results->m_internalSolverData = lane0(rhs);
            sr += 2 * striding;
            temps += 2;
            schema += hkpJacobianSchemaInfo::AngularMotor1D::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL_WITH_IMPULSE_LIMIT:
        {
            // MP7: a row beyond 99% of its limit marks the constraint; after its last such row the constraint is
            // listed as breached (no solver result). The binary compares on the x87 (the product is exact in double).
            const float* jac = reinterpret_cast<const float*>(schema);
            if ((double)fabsf(temps[0].m_impulseApplied) > (double)jac[11] * (double)0.99f) // 0x3F7D70A4
            {
                bilateralBreached = true;
            }
            if (*reinterpret_cast<const unsigned char*>(schema + hkpJacobianSchemaInfo::Bilateral1D::Sizeof) != hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL_WITH_IMPULSE_LIMIT && bilateralBreached)
            {
                numBreached++;
                hkpImpulseLimitBreachedElem* elem = reinterpret_cast<hkpImpulseLimitBreachedElem*>(breached + numBreached * sizeof(hkpImpulseLimitBreachedElem));
                elem->m_type = 0;
                elem->m_isContact = 0;
                elem->m_constraintInstance = instance;
                elem->m_solverResult = nullptr;
                bilateralBreached = false;
            }
            const Q dSumLin = _mm_sub_ps(mA->m_scratch0.m_quad, mB->m_scratch0.m_quad);
            const Q v = hadd3(_mm_add_ps(_mm_add_ps(_mm_mul_ps(mB->m_scratch1.m_quad, loadQ(jac + 8)), _mm_mul_ps(mA->m_scratch1.m_quad, loadQ(jac + 4))), _mm_mul_ps(dSumLin, loadQ(jac))));
            writeResult(sr, striding, temps[0].m_impulseApplied, nextRhs(jac[3], dampTauDt, v, numStepsDt));
            temps += 1;
            schema += hkpJacobianSchemaInfo::Bilateral1D::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL: // MP7: as a contact row
        case hkpJacobianSchema::SCHEMA_TYPE_1D_LINEAR_LIMITS: // prismatic: as a contact row (the record is 0x40)
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_LIMIT_CONTACT:
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_LIMIT_CONTACT_WITH_ACCESSOR_CHECK:
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_CONTACT:
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_CONTACT_WITH_ACCESSOR_CHECK:
        {
            const float* jac = reinterpret_cast<const float*>(schema);
            if (type >= hkpJacobianSchema::SCHEMA_TYPE_SINGLE_LIMIT_CONTACT && temps[0].m_impulseApplied > jac[11] * 0.99f) // 0x3F7D70A4
            {
                numBreached++;
                hkpImpulseLimitBreachedElem* elem = reinterpret_cast<hkpImpulseLimitBreachedElem*>(breached + numBreached * sizeof(hkpImpulseLimitBreachedElem));
                elem->m_type = 0;
                elem->m_isContact = 1;
                elem->m_constraintInstance = instance;
                elem->m_solverResult = reinterpret_cast<hkpSolverResults*>(sr);
            }
            const Q dSumLin = _mm_sub_ps(mA->m_scratch0.m_quad, mB->m_scratch0.m_quad);
            const Q v = hadd3(_mm_add_ps(_mm_add_ps(_mm_mul_ps(mB->m_scratch1.m_quad, loadQ(jac + 8)), _mm_mul_ps(mA->m_scratch1.m_quad, loadQ(jac + 4))), _mm_mul_ps(dSumLin, loadQ(jac))));
            writeResult(sr, striding, temps[0].m_impulseApplied, nextRhs(jac[3], dampTauDt, v, numStepsDt));
            temps += 1;
            schema += type == hkpJacobianSchema::SCHEMA_TYPE_1D_LINEAR_LIMITS ? hkpJacobianSchemaInfo::LinearLimits1D::Sizeof : hkpJacobianSchemaInfo::SingleContact::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_PAIR_CONTACT:
        case hkpJacobianSchema::SCHEMA_TYPE_PAIR_CONTACT_WITH_ACCESSOR_CHECK:
        {
            const float* jac0 = reinterpret_cast<const float*>(schema);
            const float* jac1 = jac0 + 12;
            const Q dSumLin = _mm_sub_ps(mA->m_scratch0.m_quad, mB->m_scratch0.m_quad);
            const float rhs0 = nextRhs(jac0[3], dampTauDt, rowSumVelocity(jac0, dSumLin, mA, mB), numStepsDt);
            const float rhs1 = nextRhs(jac1[3], dampTauDt, rowSumVelocity(jac1, dSumLin, mA, mB), numStepsDt);
            writeResult(sr, striding, temps[0].m_impulseApplied, rhs0);
            writeResult(sr, striding, temps[1].m_impulseApplied, rhs1);
            temps += 2;
            schema += hkpJacobianSchemaInfo::PairContact::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_2D_FRICTION:
        {
            const hkpJacobian2dFrictionSchema* friction = reinterpret_cast<const hkpJacobian2dFrictionSchema*>(schema);
            const float* jac0 = reinterpret_cast<const float*>(&friction->m_jac.m_jac0);
            const float* jac1 = reinterpret_cast<const float*>(&friction->m_jac.m_jac1);
            const Q scale = splat(temps[2].m_impulseApplied);
            const Q dSumLin = _mm_sub_ps(mA->m_scratch0.m_quad, mB->m_scratch0.m_quad);
            hkpSimpleContactConstraintDataInfoInternal* frictionInfo = reinterpret_cast<hkpSimpleContactConstraintDataInfoInternal*>(friction->m_solverResults);
            frictionInfo->m_rhsLin[0] = lane0(_mm_mul_ps(splat(nextRhs(jac0[3], dampFrictionDt, rowSumVelocity(jac0, dSumLin, mA, mB), numStepsDt)), scale));
            frictionInfo->m_rhsLin[1] = lane0(_mm_mul_ps(splat(nextRhs(jac1[3], dampFrictionDt, rowSumVelocity(jac1, dSumLin, mA, mB), numStepsDt)), scale));
            const float t0 = temps[0].m_impulseApplied;
            const float t1 = temps[1].m_impulseApplied;
            frictionInfo->m_impulseSqr = t1 * t1 + t0 * t0;
            temps += 3;
            schema += hkpJacobianSchemaInfo::Friction2D::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_3D_FRICTION:
        {
            const hkpJacobian3dFrictionSchema* friction = reinterpret_cast<const hkpJacobian3dFrictionSchema*>(schema);
            const float* jac0 = reinterpret_cast<const float*>(&friction->m_jac.m_jac0);
            const float* jac1 = reinterpret_cast<const float*>(&friction->m_jac.m_jac1);
            const float* jac2 = reinterpret_cast<const float*>(&friction->m_jac.m_jac2);
            const Q scale = splat(temps[3].m_impulseApplied);
            const Q dSumLin = _mm_sub_ps(mA->m_scratch0.m_quad, mB->m_scratch0.m_quad);
            hkpSimpleContactConstraintDataInfoInternal* frictionInfo = reinterpret_cast<hkpSimpleContactConstraintDataInfoInternal*>(friction->m_solverResults);
            const Q v2 = hadd3(_mm_add_ps(_mm_mul_ps(loadQ(jac2 + 4), mB->m_scratch1.m_quad), _mm_mul_ps(mA->m_scratch1.m_quad, loadQ(jac2))));
            const float rhs2 = lane0(_mm_mul_ps(splat(nextRhs(jac2[7], dampFrictionDt, v2, numStepsDt)), scale));
            frictionInfo->m_rhsLin[0] = lane0(_mm_mul_ps(splat(nextRhs(jac0[3], dampFrictionDt, rowSumVelocity(jac0, dSumLin, mA, mB), numStepsDt)), scale));
            frictionInfo->m_rhsLin[1] = lane0(_mm_mul_ps(splat(nextRhs(jac1[3], dampFrictionDt, rowSumVelocity(jac1, dSumLin, mA, mB), numStepsDt)), scale));
            frictionInfo->m_rhsAng = rhs2;
            const float t0 = temps[0].m_impulseApplied;
            const float t1 = temps[1].m_impulseApplied;
            const float t2 = temps[2].m_impulseApplied;
            frictionInfo->m_impulseSqr = (t1 * t1 + t0 * t0) + t2 * t2;
            temps += 4;
            schema += hkpJacobianSchemaInfo::Friction3D::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_2D_ROLLING_FRICTION:
        {
            const hkpJacobian2dRollingFrictionSchema* rolling = reinterpret_cast<const hkpJacobian2dRollingFrictionSchema*>(schema);
            const float* jac0 = reinterpret_cast<const float*>(&rolling->m_jac0);
            const float* jac1 = reinterpret_cast<const float*>(&rolling->m_jac1);
            const Q scale = splat(temps[2].m_impulseApplied);
            const Q v0 = hadd3(_mm_add_ps(_mm_mul_ps(loadQ(jac0 + 4), mB->m_scratch1.m_quad), _mm_mul_ps(loadQ(jac0), mA->m_scratch1.m_quad)));
            const Q v1 = hadd3(_mm_add_ps(_mm_mul_ps(loadQ(jac1 + 4), mB->m_scratch1.m_quad), _mm_mul_ps(loadQ(jac1), mA->m_scratch1.m_quad)));
            const Q rhs0 = _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(splat(jac0[7]), deltaTime), _mm_mul_ps(v0, numStepsDt)), scale);
            const Q rhs1 = _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(splat(jac1[7]), deltaTime), _mm_mul_ps(v1, numStepsDt)), scale);
            const float t0 = temps[0].m_impulseApplied;
            const float t1 = temps[1].m_impulseApplied;
            const Q impulseSqr = splat(t1 * t1 + t0 * t0);
            // three halves (rounded by HK_QUADREAL_PACK_HALF, 1 + 1/256): the rolling impulse squared and right hand sides
            const Q packed = _mm_mul_ps(_mm_movelh_ps(_mm_unpacklo_ps(impulseSqr, rhs0), _mm_unpacklo_ps(rhs1, _mm_setzero_ps())), g_vectorfConstants[HK_QUADREAL_PACK_HALF]);
            const __m128i halves = _mm_packs_epi32(_mm_srai_epi32(_mm_castps_si128(packed), 16), _mm_setzero_si128());
            HK_ALIGN16(short h[8]);
            _mm_store_si128(reinterpret_cast<__m128i*>(h), halves);
            hkpSimpleContactConstraintDataInfoInternal* frictionInfo = reinterpret_cast<hkpSimpleContactConstraintDataInfoInternal*>(rolling->m_solverResults);
            short* infoHalves = reinterpret_cast<short*>(frictionInfo);
            infoHalves[3] = h[0]; // m_rollingImpulseSqr
            infoHalves[4] = h[1]; // m_rhsRolling[0]
            infoHalves[5] = h[2]; // m_rhsRolling[1]
            temps += 3;
            schema += hkpJacobianSchemaInfo::RollingFriction2D::Sizeof;
            break;
        }

        case hkpJacobianSchema::SCHEMA_TYPE_SET_MASS:
            schema += hkpJacobianSchemaInfo::SetMass::Sizeof;
            break;

        case hkpJacobianSchema::SCHEMA_TYPE_ADD_VELOCITY:
        {
            const hkpJacobianAddVelocitySchema* addVelocity = reinterpret_cast<const hkpJacobianAddVelocitySchema*>(schema);
            schema += hkpJacobianSchemaInfo::AddVelocity::Sizeof;
            hkpVelocityAccumulator* copy;
            if (addVelocity->m_bodyIndex == 0)
            {
                copyA = *mA;
                copy = &copyA;
                mA = copy;
            }
            else
            {
                copyB = *mB;
                copy = &copyB;
                mB = copy;
            }
            copy->m_linearVel.m_quad = _mm_add_ps(copy->m_linearVel.m_quad, addVelocity->m_deltaVel.m_quad);
            copy->m_scratch0.m_quad = _mm_add_ps(addVelocity->m_deltaVel.m_quad, copy->m_scratch0.m_quad);
            break;
        }

        default:
            hkErrorFwd::messageError(0x4F704550, "OpenRFG: hkSolverExport: schema type not ported", "hkpsolverexport.cpp", __LINE__);
            reinterpret_cast<hkpImpulseLimitBreachedHeader*>(breached)->m_numBreached = numBreached;
            return;
        }
    }
}
