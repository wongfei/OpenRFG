// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the schema tables, read from rfg.exe. 255 marks a type that is not a record of
// its own (the end/goto markers handled by the solver loop, the reserved ids); 0 a type without data.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h"

// 0x01A368BC: bytes per schema, by type
const unsigned char hkpJacobianSchema::s_schemaSize[48] = {
    16, 16, 16, 16, 255, 48, 48, 64, 64, 64, 80, 64, 32, 48, 48, 64,
    64, 128, 64, 128, 48, 48, 48, 48, 112, 112, 112, 144, 80, 32, 32, 96,
    48, 96, 128, 128, 192, 192, 112, 255, 255, 255, 255, 255, 112, 0, 0, 0,
};

// 0x01A368EC: hkpSolverElemTemp entries per schema, by type
const unsigned char hkpJacobianSchema::s_schemaNumSolverElemTemps[48] = {
    255, 255, 255, 255, 255, 1, 1, 1, 1, 2, 2, 1, 1, 1, 2, 2,
    3, 3, 1, 1, 1, 1, 1, 1, 2, 2, 3, 4, 3, 0, 0, 0,
    3, 3, 3, 3, 3, 3, 2, 255, 255, 255, 255, 255, 2, 0, 0, 0,
};
