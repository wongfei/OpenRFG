// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h
//
// Seam only (Havok 2014.2.5).
#pragma once

// size 0x8
class hkpSolverResults
{
public:
    // 0x0103FD00
    float getImpulseApplied() const
    {
        return m_impulseApplied;
    }

    // 0x00DDCC30
    hkpSolverResults()
    {
        init();
    }

    // 0x00D6D090
    void init()
    {
        m_impulseApplied = 0.0f;
        m_internalSolverData = 0.0f;
    }

    float m_impulseApplied; // 0x00
    float m_internalSolverData; // 0x04
};
