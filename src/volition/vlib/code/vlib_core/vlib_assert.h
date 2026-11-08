// Placement and names inferred: no header of this name is recorded in rfg.pdb.
//
// Release builds of vlib replace failed critical checks by a log line "Int3 critical function at <file>(<line>)" and
// carry on (the string and its __FILE__/__LINE__ arguments are in the binary; the macro itself is not recorded).
#pragma once

#include "keen/base/trace.hpp"

#define VLIB_CRITICAL_FAILURE() keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__)

#define VLIB_CRITICAL_ASSERT(expression) \
    do \
    { \
        if (!(expression)) \
        { \
            VLIB_CRITICAL_FAILURE(); \
        } \
    } while (false)
