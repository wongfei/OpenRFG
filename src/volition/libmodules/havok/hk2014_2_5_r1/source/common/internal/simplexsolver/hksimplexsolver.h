// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/simplexsolver/hksimplexsolver.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (player_minimal.md P0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

// size 0x40
struct alignas(16) hkSurfaceConstraintInfo
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSurfaceConstraintInfo);

    hkVector4f m_plane; // 0x00
    hkVector4f m_velocity; // 0x10
    float m_staticFriction; // 0x20
    float m_extraUpStaticFriction; // 0x24
    float m_extraDownStaticFriction; // 0x28
    float m_dynamicFriction; // 0x2C
    int m_priority; // 0x30
};

// size 0x10
struct hkSurfaceConstraintInteraction
{
    enum hkStatus
    {
        HK_STATUS_OK = 0,
        HK_STATUS_3D_FAILURE = 1,
        HK_STATUS_2D_FAILURE = 2,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSurfaceConstraintInteraction);

    unsigned char m_touched; // 0x00
    unsigned char m_stopped; // 0x01
    float m_surfaceTime; // 0x04
    float m_penaltyDistance; // 0x08
    hkStatus m_status; // 0x0C
};

// size 0x50
struct hkSimplexSolverInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSimplexSolverInput);
    hkSimplexSolverInput(); // 0x00814760 (.h:143)

    hkVector4f m_position; // 0x00
    hkVector4f m_velocity; // 0x10
    hkVector4f m_maxSurfaceVelocity; // 0x20
    hkVector4f m_upVector; // 0x30
    float m_deltaTime; // 0x40
    float m_minDeltaTime; // 0x44
    hkSurfaceConstraintInfo* m_constraints; // 0x48
    int m_numConstraints; // 0x4C
};

// size 0x30
struct alignas(16) hkSimplexSolverOutput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSimplexSolverOutput);

    hkVector4f m_position; // 0x00
    hkVector4f m_velocity; // 0x10
    float m_deltaTime; // 0x20
    hkSurfaceConstraintInteraction* m_planeInteractions; // 0x24
};

void hkSimplexSolverSolve(const hkSimplexSolverInput& input, hkSimplexSolverOutput& output); // 0x00D6C230
