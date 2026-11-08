// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolvergettoivelocities.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the velocities along the rows of one constraint, from rfg.exe (written from the
// disassembly in its SSE operation order). The TOI solver calls it on the schemas of a constraint (its header first)
// to see whether the constraint still closes in after a solver pass. Only the schemas of contacts, the 1d bilateral
// rows and the modifiers are ported; the stable ball-and-socket (three velocities through the bodies' quaternions)
// belongs to constraint types no in-scope code creates and reports an error.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolvergettoivelocities.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

namespace
{
__forceinline Q loadQ(const void* p) { return _mm_load_ps(static_cast<const float*>(p)); }

// the velocity along a 1 linear 2 angular row: hadd3(((ang1 * B.ang) + (ang0 * A.ang)) + (A.lin - B.lin) * lin)
__forceinline Q rowVelocity(const char* jac, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB)
{
    const Q ang = _mm_add_ps(_mm_mul_ps(loadQ(jac + 0x20), mB->m_angularVel.m_quad), _mm_mul_ps(loadQ(jac + 0x10), mA->m_angularVel.m_quad));
    const Q lin = _mm_mul_ps(_mm_sub_ps(mA->m_linearVel.m_quad, mB->m_linearVel.m_quad), loadQ(jac));
    return hadd3(_mm_add_ps(ang, lin));
}
}

// 0x00E9BC30: up to maxNumVelocities velocities of the constraint whose header starts the schemas, in row order;
// contact rows report only approaching velocities (min with 0). Stops at the end, the next header or a friction
// schema; a schema that would not fit in the output stops it too. The center of mass modifier moves the bodies'
// velocities in local copies. Returns the number of velocities written.
int hkSolveGetToiViolatingConstraintVelocity(hkpSolverInfo& i, const hkpJacobianSchema* schemas, const hkpVelocityAccumulator* accums, int maxNumVelocities, float* velocitiesOut)
{
    int numVelocities = 0;
    const char* schema = reinterpret_cast<const char*>(schemas);
    if (*schema == hkpJacobianSchema::SCHEMA_TYPE_END)
    {
        return 0;
    }
    const hkpJacobianHeaderSchema* header = reinterpret_cast<const hkpJacobianHeaderSchema*>(schema);
    const hkpVelocityAccumulator* mA = accums + header->m_bodyAIndex;
    const hkpVelocityAccumulator* mB = accums + header->m_bodyBIndex;
    schema += sizeof(hkpJacobianHeaderSchema);
    HK_ALIGN16(hkpVelocityAccumulator localA);
    HK_ALIGN16(hkpVelocityAccumulator localB);
    for (;;)
    {
        const unsigned char type = *reinterpret_cast<const unsigned char*>(schema);
        switch (type)
        {
        case hkpJacobianSchema::SCHEMA_TYPE_END:
        case hkpJacobianSchema::SCHEMA_TYPE_HEADER:
        case hkpJacobianSchema::SCHEMA_TYPE_2D_FRICTION:
        case hkpJacobianSchema::SCHEMA_TYPE_3D_FRICTION:
        case hkpJacobianSchema::SCHEMA_TYPE_2D_ROLLING_FRICTION:
            return numVelocities;
        case hkpJacobianSchema::SCHEMA_TYPE_GOTO:
            schema += reinterpret_cast<const hkpJacobianGotoSchema*>(schema)->m_offset;
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_SHIFT_SOLVER_RESULTS_POINTER:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_LINEAR_LIMITS:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_FRICTION:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_LINEAR_MOTOR:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_LIMITS:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_FRICTION:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_MOTOR:
        case hkpJacobianSchema::SCHEMA_TYPE_SET_MASS:
        case hkpJacobianSchema::SCHEMA_TYPE_ADD_VELOCITY:
            // skipped, with any directly following schemas of the same type
            do
            {
                schema += hkpJacobianSchema::getSize(reinterpret_cast<const hkpJacobianSchema*>(schema));
            } while (*reinterpret_cast<const unsigned char*>(schema) == type);
            break;
        case hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL_WITH_IMPULSE_LIMIT:
        case hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL_USER_TAU:
        {
            if (numVelocities + 1 > maxNumVelocities)
            {
                return numVelocities;
            }
            *velocitiesOut++ = lane0(rowVelocity(schema, mA, mB));
            numVelocities++;
            schema += type == hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL_USER_TAU ? hkpJacobianSchemaInfo::BilateralUserTau1D::Sizeof : hkpJacobianSchemaInfo::Bilateral1D::Sizeof;
            break;
        }
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_CONTACT:
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_CONTACT_WITH_ACCESSOR_CHECK:
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_LIMIT_CONTACT:
        case hkpJacobianSchema::SCHEMA_TYPE_SINGLE_LIMIT_CONTACT_WITH_ACCESSOR_CHECK:
        {
            if (numVelocities + 1 > maxNumVelocities)
            {
                return numVelocities;
            }
            *velocitiesOut++ = lane0(_mm_min_ps(rowVelocity(schema, mA, mB), _mm_setzero_ps()));
            numVelocities++;
            schema += hkpJacobianSchemaInfo::SingleContact::Sizeof;
            break;
        }
        case hkpJacobianSchema::SCHEMA_TYPE_PAIR_CONTACT:
        case hkpJacobianSchema::SCHEMA_TYPE_PAIR_CONTACT_WITH_ACCESSOR_CHECK:
        {
            if (numVelocities + 2 > maxNumVelocities)
            {
                return numVelocities;
            }
            velocitiesOut[0] = lane0(_mm_min_ps(rowVelocity(schema, mA, mB), _mm_setzero_ps()));
            velocitiesOut[1] = lane0(_mm_min_ps(rowVelocity(schema + 0x30, mA, mB), _mm_setzero_ps()));
            velocitiesOut += 2;
            numVelocities += 2;
            schema += hkpJacobianSchemaInfo::PairContact::Sizeof;
            break;
        }
        case hkpJacobianSchema::SCHEMA_TYPE_SET_CENTER_OF_MASS:
        {
            // the bodies' angular velocities (and sums) moved to the linear ones, in copies of the accumulators
            const float* a = reinterpret_cast<const float*>(schema);
            const float* b = a + 12;
            localA = *mA;
            localB = *mB;
            localA.m_scratch0.m_quad = _mm_add_ps(rotate(loadQ(a), loadQ(a + 4), loadQ(a + 8), localA.m_scratch1.m_quad), localA.m_scratch0.m_quad);
            localA.m_linearVel.m_quad = _mm_add_ps(rotate(loadQ(a), loadQ(a + 4), loadQ(a + 8), localA.m_angularVel.m_quad), localA.m_linearVel.m_quad);
            localB.m_linearVel.m_quad = _mm_add_ps(rotate(loadQ(b), loadQ(b + 4), loadQ(b + 8), localB.m_angularVel.m_quad), localB.m_linearVel.m_quad);
            localB.m_scratch0.m_quad = _mm_add_ps(rotate(loadQ(b), loadQ(b + 4), loadQ(b + 8), localB.m_scratch1.m_quad), localB.m_scratch0.m_quad);
            mA = &localA;
            mB = &localB;
            schema += hkpJacobianSchemaInfo::SetCenterOfMass::Sizeof;
            break;
        }
        default:
            // the binary loops on the same schema forever here (no such schema in RFG's constraints)
            hkErrorFwd::messageError(0x4F70454F, "OpenRFG: hkSolveGetToiViolatingConstraintVelocity: schema type not ported", "hkpsolvergettoivelocities.cpp", __LINE__);
            return numVelocities;
        }
    }
}
