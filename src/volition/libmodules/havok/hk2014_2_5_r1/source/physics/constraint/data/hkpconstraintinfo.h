// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintinfo.h
//
// Seam only (Havok 2014.2.5).
#pragma once

// size 0x10
struct hkpConstraintInfo
{
    void clear(); // 0x00D702E0
    void addHeader(); // 0x00D702F0
    void add(const hkpConstraintInfo& delta); // 0x00E0B080
    void add(int schemaSize, int numSolverResults, int numSolverTempElems); // 0x00D70300
    void sub(const hkpConstraintInfo& delta); // 0x00E0AB80
    void merge(const hkpConstraintInfo& delta); // 0x00E1DBB0

    int m_maxSizeOfSchema; // 0x00
    int m_sizeOfSchemas; // 0x04
    int m_numSolverResults; // 0x08
    int m_numSolverElemTemps; // 0x0C
};
