// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/simplexsolver/hksimplexsolver.cpp
//
// Havok 2014.2.5 (player_minimal.md P1): the simplex solver of the character proxy, from rfg.exe. It moves a particle
// through a set of moving planes over a time step: it walks until the first plane is hit, adds it to the active set
// (at most four planes), solves the velocity against the active planes (1d: slide on one plane with static and dynamic
// friction, 2d: along the crease of two planes, 3d: the corner of three) and walks on. The PDB puts this file in
// nightly job 20150629-01, the rest of the SDK in 20150603-01.
//
// LTCG custom conventions in rfg.exe (mapped in Ghidra): Test1d sci EAX, velocityIn ECX; Solve1d sci EAX,
// velocityOut EDX; Solve2d info ESI, sci1 EDI, velocityOut EDX; Solve3d and ExamineActivePlanes info EAX.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/simplexsolver/hksimplexsolver.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"

// ---- TU-private types (rfg.pdb; no header names them)
// size 0xC
struct hkSimplexSolverActivePlanes
{
    int m_index; // 0x00
    const hkSurfaceConstraintInfo* m_constraint; // 0x04
    hkSurfaceConstraintInteraction* m_interaction; // 0x08
};

// size 0x60
struct hkSimplexSolverInfo
{
    hkSurfaceConstraintInteraction& getOutput(const hkSurfaceConstraintInfo* info); // 0x00D6B3A0 (:32)

    hkSimplexSolverActivePlanes m_supportPlanes[4]; // 0x00
    int m_numSupportPlanes; // 0x30
    hkSimdFloat32 m_currentTime; // 0x40
    const hkSimplexSolverInput* m_input; // 0x50
    hkSimplexSolverOutput* m_output; // 0x54
};

// 0x00814760 (hksimplexsolver.h:143)
hkSimplexSolverInput::hkSimplexSolverInput()
{
    m_upVector.setConstant<HK_QUADREAL_0100>();
    m_maxSurfaceVelocity.setAll(hkSimdFloat32::getConstant<HK_QUADREAL_EPS>());
    m_position.setZero();
}

// 0x00D6B3A0: the interaction that belongs to a constraint (same index)
hkSurfaceConstraintInteraction& hkSimplexSolverInfo::getOutput(const hkSurfaceConstraintInfo* info)
{
    return m_output->m_planeInteractions[info - m_input->m_constraints];
}

// Inline in Solve2d/Solve3d: is a component of the solved velocity (x, y or z) faster than m_maxSurfaceVelocity?
static inline bool hkSimplexSolverTooFast(const hkSimplexSolverInfo& info, const hkVector4f& velocity)
{
    hkVector4f absVelocity;
    absVelocity.setAbs(velocity);
    return (_mm_movemask_ps(_mm_cmplt_ps(info.m_input->m_maxSurfaceVelocity.m_quad, absVelocity.m_quad)) & 7) != 0;
}

// 0x00D6B430: the active planes by priority (lowest first), equal priorities by plane speed (slowest first)
static void hkSimplexSolverSortInfo(hkSimplexSolverInfo& info)
{
    for (int i = 0; i < info.m_numSupportPlanes - 1; i++)
    {
        for (int j = i + 1; j < info.m_numSupportPlanes; j++)
        {
            hkSimplexSolverActivePlanes& planeI = info.m_supportPlanes[i];
            hkSimplexSolverActivePlanes& planeJ = info.m_supportPlanes[j];
            if (planeI.m_constraint->m_priority < planeJ.m_constraint->m_priority)
            {
                continue;
            }
            if (planeI.m_constraint->m_priority == planeJ.m_constraint->m_priority)
            {
                const hkSimdFloat32 velI = planeI.m_constraint->m_velocity.lengthSquared<3>();
                const hkSimdFloat32 velJ = planeJ.m_constraint->m_velocity.lengthSquared<3>();
                // ucomiss/jc: NaN keeps the order
                if (!(velI.getReal() >= velJ.getReal()))
                {
                    continue;
                }
            }
            const hkSimplexSolverActivePlanes tmp = planeI;
            planeI = planeJ;
            planeJ = tmp;
        }
    }
}

// 0x00D6B520: does the velocity move into the plane (faster than the plane, with a 1 mm/s tolerance)?
static unsigned int hkSimplexSolverSolveTest1d(const hkSurfaceConstraintInfo& sci, const hkVector4f& velocityIn)
{
    hkVector4f relVel;
    relVel.setSub(velocityIn, sci.m_velocity);
    const hkSimdFloat32 eps = hkSimdFloat32::fromFloat(-0.001f); // 0xBA83126F
    // ucomiss/cmovc: NaN counts as a violation
    return !(sci.m_plane.dot<3>(relVel).getReal() >= eps.getReal());
}

// 0x00D6B570: slide on one plane. Static friction stops the motion along the plane when the speed into the plane
// times the friction exceeds the speed along it; extra up/down friction is a box friction around the up axis.
// Dynamic friction keeps a share of the input speed along the plane.
static void hkSimplexSolverSolve1d(const hkSimplexSolverInfo& info, const hkSurfaceConstraintInfo& sci, const hkVector4f& velocityIn, hkVector4f& velocityOut)
{
    const hkSimdFloat32 dynamicFriction = hkSimdFloat32::fromFloat(sci.m_dynamicFriction);
    const hkVector4f groundVelocity = sci.m_velocity;
    hkVector4f relVel;
    relVel.setSub(velocityIn, groundVelocity);
    const hkSimdFloat32 staticFriction = hkSimdFloat32::fromFloat(sci.m_staticFriction);

    const hkSimdFloat32 planeVel = sci.m_plane.dot<3>(relVel);
    const hkSimdFloat32 origVelocity2 = relVel.lengthSquared<3>();
    relVel.subMul(sci.m_plane, planeVel);

    const hkSimplexSolverInput& input = *info.m_input;
    const hkSimdFloat32 vp2 = planeVel * planeVel;
    const hkSimdFloat32 zero = hkSimdFloat32::getConstant<HK_QUADREAL_0>();
    const hkSimdFloat32 one = hkSimdFloat32::getConstant<HK_QUADREAL_1>();
    const float extraFrictionF = (input.m_upVector.dot<3>(relVel).getReal() > 0.0f) ? sci.m_extraUpStaticFriction : sci.m_extraDownStaticFriction;
    const hkSimdFloat32 extraFriction = hkSimdFloat32::fromFloat(extraFrictionF);

    if (extraFriction.getReal() > 0.0f)
    {
        // box friction: the horizontal part (perpendicular to up in the plane) and the rest are tested separately
        hkVector4f horizontal;
        horizontal.setCross(input.m_upVector, sci.m_plane);
        const hkSimdFloat32 horizontalLen2 = horizontal.lengthSquared<3>();
        hkSimdFloat32 horizontalVel = zero;
        if (horizontalLen2.getReal() > hkSimdFloat32::getConstant<HK_QUADREAL_EPS>().getReal())
        {
            horizontal.mul(horizontalLen2.sqrtInverse<HK_ACC_23_BIT, HK_SQRT_IGNORE>());
            horizontalVel = horizontal.dot<3>(relVel);
            // ucomiss/jc: NaN skips
            if ((staticFriction * staticFriction * vp2).getReal() >= (horizontalVel * horizontalVel).getReal())
            {
                relVel.subMul(horizontal, horizontalVel);
                horizontalVel = zero;
            }
        }
        const hkSimdFloat32 totalFriction = extraFriction + staticFriction;
        // ucomiss/jc: NaN goes on to the dynamic friction
        if ((totalFriction * totalFriction * vp2).getReal() >= (origVelocity2 - horizontalVel * horizontalVel - vp2).getReal())
        {
            // ucomiss/jnz: NaN counts as zero
            if (!(horizontalVel.getReal() < 0.0f) && !(horizontalVel.getReal() > 0.0f))
            {
                velocityOut = groundVelocity;
                return;
            }
            relVel.setMul(horizontal, horizontalVel);
        }
    }
    else
    {
        // static friction: vp2 * f2 > vpar2 = origVelocity2 - vp2, that is vp2 * (f2 + 1) > origVelocity2
        if (((staticFriction * staticFriction + one) * vp2).getReal() >= origVelocity2.getReal())
        {
            velocityOut = groundVelocity;
            return;
        }
    }

    // ucomiss/jnc: NaN friction goes on
    if (!(dynamicFriction.getReal() >= one.getReal()))
    {
        const hkSimdFloat32 velocity2 = relVel.lengthSquared<3>();
        if (velocity2.getReal() >= hkSimdFloat32::getConstant<HK_QUADREAL_EPS>().getReal() &&
            velocity2.getReal() > (hkSimdFloat32::fromFloat(0.001f) * origVelocity2).getReal()) // 0x3A83126F
        {
            hkSimdFloat32 ratio;
            ratio.setDiv<HK_ACC_23_BIT, HK_DIV_IGNORE>(origVelocity2, velocity2);
            const hkSimdFloat32 f = (one - dynamicFriction) * ratio.sqrt<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>() + dynamicFriction;
            relVel.mul(f);
            relVel.subMul(sci.m_plane, sci.m_plane.dot<3>(relVel));
        }
    }
    velocityOut.setAdd(groundVelocity, relVel);
}

// 0x00D6B840: move along the crease of two planes. The crease velocity solves both plane velocities and takes the
// mean speed along the axis; if that is faster than m_maxSurfaceVelocity (nearly parallel planes), the planes are
// solved one after the other instead, the higher priority one last.
static void hkSimplexSolverSolve2d(hkSimplexSolverInfo& info, const hkSurfaceConstraintInfo& sci0, const hkSurfaceConstraintInfo& sci1, const hkVector4f& velocityIn, hkVector4f& velocityOut)
{
    hkVector4f axis;
    axis.setCross(sci0.m_plane, sci1.m_plane);
    const hkSimdFloat32 axisLen2 = axis.lengthSquared<3>();
    if (axisLen2.getReal() > hkSimdFloat32::getConstant<HK_QUADREAL_EPS>().getReal())
    {
        const hkSimdFloat32 invAxisLen = axisLen2.sqrtInverse<HK_ACC_23_BIT, HK_SQRT_IGNORE>();
        hkVector4f axisN;
        axisN.setMul(axis, invAxisLen);

        // the inverse of the rows (plane0, plane1, axisN) has the columns cross(plane1, axisN), cross(axisN, plane0)
        // and cross(plane0, plane1), over the determinant |cross(plane0, plane1)|
        hkVector4f col1;
        col1.setCross(sci1.m_plane, axisN);
        hkVector4f col2;
        col2.setCross(axisN, sci0.m_plane);
        hkVector4f sumVel;
        sumVel.setAdd(sci0.m_velocity, sci1.m_velocity);
        const hkSimdFloat32 axisVelocity = axisN.dot<3>(sumVel) * hkSimdFloat32::getConstant<HK_QUADREAL_INV_2>();
        const hkSimdFloat32 plane0Velocity = sci0.m_plane.dot<3>(sci0.m_velocity);
        const hkSimdFloat32 plane1Velocity = sci1.m_plane.dot<3>(sci1.m_velocity);

        hkVector4f planeVelocity;
        planeVelocity.setMul(axis, axisVelocity);
        planeVelocity.addMul(col1, plane0Velocity);
        planeVelocity.addMul(col2, plane1Velocity);
        planeVelocity.mul(invAxisLen);

        if (!hkSimplexSolverTooFast(info, planeVelocity))
        {
            // the crease
            hkVector4f relVel;
            relVel.setSub(velocityIn, planeVelocity);
            const hkSimdFloat32 origVelocity2 = relVel.lengthSquared<3>();
            const hkSimdFloat32 upAxis = info.m_input->m_upVector.dot<3>(axisN);
            hkSimdFloat32 axisVel = relVel.dot<3>(axisN);
            const hkSimdFloat32 staticFriction = hkSimdFloat32::fromFloat(sci0.m_staticFriction + sci1.m_staticFriction);
            const float extraFrictionF = ((axisVel * upAxis).getReal() > 0.0f) ? sci0.m_extraUpStaticFriction + sci1.m_extraUpStaticFriction : sci0.m_extraDownStaticFriction + sci1.m_extraDownStaticFriction;
            const hkSimdFloat32 friction = (hkSimdFloat32::fromFloat(extraFrictionF) * upAxis + staticFriction) * hkSimdFloat32::getConstant<HK_QUADREAL_INV_2>();
            const hkSimdFloat32 dynamicFriction = hkSimdFloat32::fromFloat(sci0.m_dynamicFriction + sci1.m_dynamicFriction) * hkSimdFloat32::getConstant<HK_QUADREAL_INV_2>();
            const hkSimdFloat32 axisVel2 = axisVel * axisVel;
            // static friction: the speed perpendicular to the crease times the friction against the speed along it
            // ucomiss/jc: NaN goes on to the dynamic friction
            if (((origVelocity2 - axisVel2) * (friction * friction)).getReal() >= axisVel2.getReal())
            {
                velocityOut = planeVelocity;
                return;
            }
            // ucomiss/jnc: NaN friction goes on
            if (!(dynamicFriction.getReal() >= hkSimdFloat32::getConstant<HK_QUADREAL_1>().getReal()) &&
                axisVel2.getReal() > (hkSimdFloat32::fromFloat(0.001f) * origVelocity2).getReal()) // 0x3A83126F
            {
                hkSimdFloat32 invAxisVel;
                invAxisVel.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(axisVel);
                invAxisVel.setAbs(invAxisVel);
                const hkSimdFloat32 f = origVelocity2.sqrt<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>() * invAxisVel * (hkSimdFloat32::getConstant<HK_QUADREAL_1>() - dynamicFriction) + dynamicFriction;
                axisVel = axisVel * f;
            }
            velocityOut.setAddMul(planeVelocity, axisN, axisVel);
            return;
        }
    }

    // parallel planes: penetrate the lower priority plane
    info.getOutput(&sci0).m_status = hkSurfaceConstraintInteraction::HK_STATUS_2D_FAILURE;
    info.getOutput(&sci1).m_status = hkSurfaceConstraintInteraction::HK_STATUS_2D_FAILURE;
    if (sci0.m_priority > sci1.m_priority)
    {
        hkSimplexSolverSolve1d(info, sci1, velocityIn, velocityOut);
        hkSimplexSolverSolve1d(info, sci0, velocityOut, velocityOut);
    }
    else
    {
        hkSimplexSolverSolve1d(info, sci0, velocityIn, velocityOut);
        hkSimplexSolverSolve1d(info, sci1, velocityOut, velocityOut);
    }
}

// 0x00D6BC00: the corner of three planes (the point velocity that solves all three plane velocities). A degenerate
// corner or one faster than m_maxSurfaceVelocity falls back to the creases of the three pairs.
static void hkSimplexSolverSolve3d(hkSimplexSolverInfo& info, const hkSurfaceConstraintInfo* sci0, const hkSurfaceConstraintInfo* sci1, const hkSurfaceConstraintInfo* sci2, int allowResort, const hkVector4f& velocityIn, hkVector4f& velocityOut)
{
    {
        hkVector4f r0;
        r0.setCross(sci1->m_plane, sci2->m_plane);
        hkVector4f r1;
        r1.setCross(sci2->m_plane, sci0->m_plane);
        hkVector4f r2;
        r2.setCross(sci0->m_plane, sci1->m_plane);
        const hkSimdFloat32 det = r0.dot<3>(sci0->m_plane);
        hkSimdFloat32 absDet;
        absDet.setAbs(det);
        // ucomiss/jc: NaN fails
        if (absDet.getReal() >= hkSimdFloat32::getConstant<HK_QUADREAL_EPS>().getReal())
        {
            const hkSimdFloat32 plane0Velocity = sci0->m_plane.dot<3>(sci0->m_velocity);
            const hkSimdFloat32 plane1Velocity = sci1->m_plane.dot<3>(sci1->m_velocity);
            const hkSimdFloat32 plane2Velocity = sci2->m_plane.dot<3>(sci2->m_velocity);
            hkVector4f pointVel;
            pointVel.setMul(r0, plane0Velocity);
            pointVel.addMul(r1, plane1Velocity);
            pointVel.addMul(r2, plane2Velocity);
            hkSimdFloat32 invDet;
            invDet.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(det);
            pointVel.mul(invDet);
            if (!hkSimplexSolverTooFast(info, pointVel))
            {
                velocityOut = pointVel;
                return;
            }
        }
    }

    if (allowResort)
    {
        hkSimplexSolverSortInfo(info);
        sci0 = info.m_supportPlanes[0].m_constraint;
        sci1 = info.m_supportPlanes[1].m_constraint;
        sci2 = info.m_supportPlanes[2].m_constraint;
    }
    info.getOutput(sci0).m_status = hkSurfaceConstraintInteraction::HK_STATUS_3D_FAILURE;
    info.getOutput(sci1).m_status = hkSurfaceConstraintInteraction::HK_STATUS_3D_FAILURE;
    info.getOutput(sci2).m_status = hkSurfaceConstraintInteraction::HK_STATUS_3D_FAILURE;

    // the creases until one of them changes the active set (only ExamineActivePlanes does; kept as the original)
    const int oldNum = info.m_numSupportPlanes;
    hkSimplexSolverSolve2d(info, *sci0, *sci1, velocityIn, velocityOut);
    if (oldNum == info.m_numSupportPlanes)
    {
        hkSimplexSolverSolve2d(info, *sci0, *sci2, velocityOut, velocityOut);
    }
    if (oldNum == info.m_numSupportPlanes)
    {
        hkSimplexSolverSolve2d(info, *sci1, *sci2, velocityOut, velocityOut);
    }
}

// 0x00D6BE60: the output velocity from the active planes, dropping planes the solution leaves. Every solve starts
// from the input velocity (m_input->m_velocity), not from the output velocity of the previous plane hits.
static void hkSimplexSolverExamineActivePlanes(hkSimplexSolverInfo& info)
{
    while (1)
    {
        switch (info.m_numSupportPlanes)
        {
        case 1:
        {
            hkSimplexSolverSolve1d(info, *info.m_supportPlanes[0].m_constraint, info.m_input->m_velocity, info.m_output->m_velocity);
            return;
        }
        case 2:
        {
            // the newest plane alone is enough when the result does not move into the other one
            hkVector4f velocity;
            hkSimplexSolverSolve1d(info, *info.m_supportPlanes[1].m_constraint, info.m_input->m_velocity, velocity);
            if (!hkSimplexSolverSolveTest1d(*info.m_supportPlanes[0].m_constraint, velocity))
            {
                info.m_output->m_velocity = velocity;
                info.m_supportPlanes[0] = info.m_supportPlanes[1];
                info.m_numSupportPlanes = 1;
                return;
            }
            hkSimplexSolverSolve2d(info, *info.m_supportPlanes[0].m_constraint, *info.m_supportPlanes[1].m_constraint, info.m_input->m_velocity, info.m_output->m_velocity);
            return;
        }
        case 3:
        {
            {
                hkVector4f velocity;
                hkSimplexSolverSolve1d(info, *info.m_supportPlanes[2].m_constraint, info.m_input->m_velocity, velocity);
                if (!hkSimplexSolverSolveTest1d(*info.m_supportPlanes[0].m_constraint, velocity) &&
                    !hkSimplexSolverSolveTest1d(*info.m_supportPlanes[1].m_constraint, velocity))
                {
                    info.m_output->m_velocity = velocity;
                    info.m_supportPlanes[0] = info.m_supportPlanes[2];
                    info.m_numSupportPlanes = 1;
                    continue;
                }
            }
            {
                bool reduced = false;
                for (int i = 0; i < 2; i++)
                {
                    // the newest plane with one of the others, the remaining one must not be violated
                    hkVector4f velocity;
                    hkSimplexSolverSolve2d(info, *info.m_supportPlanes[i].m_constraint, *info.m_supportPlanes[2].m_constraint, info.m_input->m_velocity, velocity);
                    if (!hkSimplexSolverSolveTest1d(*info.m_supportPlanes[1 - i].m_constraint, velocity))
                    {
                        info.m_supportPlanes[0] = info.m_supportPlanes[i];
                        info.m_supportPlanes[1] = info.m_supportPlanes[2];
                        info.m_numSupportPlanes--;
                        reduced = true;
                        break;
                    }
                }
                if (reduced)
                {
                    continue;
                }
            }
            hkSimplexSolverSolve3d(info, info.m_supportPlanes[0].m_constraint, info.m_supportPlanes[1].m_constraint, info.m_supportPlanes[2].m_constraint, 1, info.m_input->m_velocity, info.m_output->m_velocity);
            return;
        }
        case 4:
        {
            hkSimplexSolverSortInfo(info);
            bool reduced = false;
            for (int i = 0; i < 3; i++)
            {
                // the newest plane with two of the others, the remaining one must not be violated
                hkVector4f velocity;
                hkSimplexSolverSolve3d(info, info.m_supportPlanes[(i + 1) % 3].m_constraint, info.m_supportPlanes[(i + 2) % 3].m_constraint, info.m_supportPlanes[3].m_constraint, 0, info.m_input->m_velocity, velocity);
                if (!hkSimplexSolverSolveTest1d(*info.m_supportPlanes[i].m_constraint, velocity))
                {
                    info.m_supportPlanes[i] = info.m_supportPlanes[2];
                    info.m_supportPlanes[2] = info.m_supportPlanes[3];
                    info.m_numSupportPlanes = 3;
                    reduced = true;
                    break;
                }
            }
            if (reduced)
            {
                continue;
            }

            // no three planes work: solve the four corners in turn, then drop the plane with the worst status
            const hkSurfaceConstraintInfo* sci0 = info.m_supportPlanes[0].m_constraint;
            const hkSurfaceConstraintInfo* sci1 = info.m_supportPlanes[1].m_constraint;
            const hkSurfaceConstraintInfo* sci2 = info.m_supportPlanes[2].m_constraint;
            const hkSurfaceConstraintInfo* sci3 = info.m_supportPlanes[3].m_constraint;
            const int oldNum = info.m_numSupportPlanes;
            hkVector4f velocity = info.m_input->m_velocity;
            hkSimplexSolverSolve3d(info, sci0, sci1, sci2, 0, velocity, velocity);
            if (oldNum == info.m_numSupportPlanes)
            {
                hkSimplexSolverSolve3d(info, sci0, sci1, sci3, 0, velocity, velocity);
            }
            if (oldNum == info.m_numSupportPlanes)
            {
                hkSimplexSolverSolve3d(info, sci0, sci2, sci3, 0, velocity, velocity);
            }
            if (oldNum == info.m_numSupportPlanes)
            {
                hkSimplexSolverSolve3d(info, sci1, sci2, sci3, 0, velocity, velocity);
            }
            info.m_output->m_velocity = velocity;

            int maxStatus = 0;
            for (int i = 0; i < 4; i++)
            {
                const int status = info.m_supportPlanes[i].m_interaction->m_status;
                maxStatus = (maxStatus <= status) ? status : maxStatus;
            }
            for (int i = 0; i < 4; i++)
            {
                if (info.m_supportPlanes[i].m_interaction->m_status == maxStatus)
                {
                    info.m_supportPlanes[i] = info.m_supportPlanes[3];
                    break;
                }
            }
            info.m_numSupportPlanes--;
            info.m_supportPlanes[0].m_interaction->m_status = hkSurfaceConstraintInteraction::HK_STATUS_OK;
            info.m_supportPlanes[1].m_interaction->m_status = hkSurfaceConstraintInteraction::HK_STATUS_OK;
            info.m_supportPlanes[2].m_interaction->m_status = hkSurfaceConstraintInteraction::HK_STATUS_OK;
            continue;
        }
        default:
            return;
        }
    }
}

// 0x00D6C230
void hkSimplexSolverSolve(const hkSimplexSolverInput& input, hkSimplexSolverOutput& output)
{
    hkSimplexSolverInfo info;
    for (int i = 0; i < 4; i++)
    {
        info.m_supportPlanes[i].m_index = (int)0xFAFAFAFA; // the original's "unused" fill
        info.m_supportPlanes[i].m_constraint = nullptr;
        info.m_supportPlanes[i].m_interaction = nullptr;
    }
    info.m_numSupportPlanes = 0;
    info.m_currentTime = hkSimdFloat32::getConstant<HK_QUADREAL_0>();
    info.m_input = &input;
    info.m_output = &output;

    hkSimdFloat32 remainingTime = hkSimdFloat32::fromFloat(input.m_deltaTime);
    output.m_velocity = input.m_velocity;
    output.m_position = input.m_position;
    for (int i = 0; i < input.m_numConstraints; i++)
    {
        hkSurfaceConstraintInteraction& interaction = output.m_planeInteractions[i];
        interaction.m_touched = 0;
        interaction.m_stopped = 0;
        interaction.m_surfaceTime = 0.0f;
        interaction.m_penaltyDistance = 0.0f;
        interaction.m_status = hkSurfaceConstraintInteraction::HK_STATUS_OK;
    }

    const hkSimdFloat32 zero = hkSimdFloat32::getConstant<HK_QUADREAL_0>();
    const hkSimdFloat32 minTimeStep = hkSimdFloat32::fromFloat(1.0e-4f); // 0x38D1B717
    // ucomiss/jnc: NaN stops
    while (remainingTime.getReal() >= zero.getReal())
    {
        // the first plane the particle reaches in the remaining time
        hkSimdFloat32 minTime = remainingTime;
        int hitIndex = -1;
        for (int i = 0; i < input.m_numConstraints; i++)
        {
            // the active planes are not tested (only the first three are looked at, as the original does)
            if (info.m_numSupportPlanes >= 1 && info.m_supportPlanes[0].m_index == i)
            {
                continue;
            }
            if (info.m_numSupportPlanes >= 2 && info.m_supportPlanes[1].m_index == i)
            {
                continue;
            }
            if (info.m_numSupportPlanes >= 3 && info.m_supportPlanes[2].m_index == i)
            {
                continue;
            }
            if (output.m_planeInteractions[i].m_status != hkSurfaceConstraintInteraction::HK_STATUS_OK)
            {
                continue;
            }
            const hkSurfaceConstraintInfo& sci = input.m_constraints[i];
            hkVector4f relVel;
            relVel.setSub(output.m_velocity, sci.m_velocity);
            const hkSimdFloat32 projectedVel = zero - sci.m_plane.dot<3>(relVel);
            if (!(projectedVel.getReal() > zero.getReal()))
            {
                continue;
            }
            // the plane at the current time, against the position
            hkVector4f relPos;
            relPos.setSubMul(output.m_position, sci.m_velocity, info.m_currentTime);
            hkSimdFloat32 distance = sci.m_plane.dot4xyz1(relPos);
            distance.zeroIfTrue(distance.lessEqual(hkSimdFloat32::getConstant<HK_QUADREAL_EPS>()));
            distance = distance + hkSimdFloat32::fromFloat(output.m_planeInteractions[i].m_penaltyDistance);
            // ucomiss/jnc: NaN counts as a hit
            if (!(distance.getReal() >= (minTime * projectedVel).getReal()))
            {
                minTime.setDiv<HK_ACC_23_BIT, HK_DIV_IGNORE>(distance, projectedVel);
                hitIndex = i;
            }
        }

        if (minTime.getReal() > minTimeStep.getReal())
        {
            output.m_position.addMul(minTime, output.m_velocity);
            info.m_currentTime = minTime + info.m_currentTime;
            remainingTime = remainingTime - minTime;
            for (int i = 0; i < info.m_numSupportPlanes; i++)
            {
                hkSurfaceConstraintInteraction* interaction = info.m_supportPlanes[i].m_interaction;
                interaction->m_surfaceTime = (hkSimdFloat32::fromFloat(interaction->m_surfaceTime) + minTime).getReal();
                interaction->m_touched = 1;
            }
            output.m_deltaTime = info.m_currentTime.getReal();
            if (info.m_currentTime.getReal() > input.m_minDeltaTime)
            {
                return;
            }
        }

        if (hitIndex < 0)
        {
            output.m_deltaTime = input.m_deltaTime;
            return;
        }

        hkSimplexSolverActivePlanes& plane = info.m_supportPlanes[info.m_numSupportPlanes++];
        plane.m_index = hitIndex;
        plane.m_constraint = &input.m_constraints[hitIndex];
        plane.m_interaction = &output.m_planeInteractions[hitIndex];
        // a plane hit again later counts as further away (keeps the solver from cycling)
        plane.m_interaction->m_penaltyDistance = (plane.m_interaction->m_penaltyDistance + 1.192092896e-07f) * 2.0f; // HK_REAL_EPSILON
        hkSimplexSolverExamineActivePlanes(info);
    }
}
