// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the out-of-line accessors of hkpVelocityAccumulator in rfg.exe (inline in
// Havok's header; the PDB-generated header declares them). The others are inlined where the solver uses them.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"

// 0x00B73850: leaves the accumulator uninitialized
hkpVelocityAccumulator::hkpVelocityAccumulator()
{
}

// 0x00D728F0: build-Jacobians context only (scratch3)
const hkVector4f& hkpVelocityAccumulator::getCenterOfMassInWorld() const
{
    return m_scratch3;
}
