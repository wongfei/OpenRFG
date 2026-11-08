// Original: library/modules/runtime/base/include/keen/base/terminate.hpp
// Implementation: library/modules/runtime/base/sources/global/terminate.cpp
//
// Termination handlers called in steps when the process shuts down.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum TerminationStep
    {
        TerminationStep_AbortLoading = 0,
        TerminationStep_TerminateMovieAndSound = 1,
        TerminationStep_TerminateLibraries = 2,
        TerminationStep_CompleteWrite = 3,
        TerminationStep_PS3_EndRSX = 4,
    };

    // size 0x8
    struct TerminationHandler
    {
        virtual ~TerminationHandler(); // vtbl+0x00
        virtual void handleTermination(TerminationStep) = 0; // vtbl+0x04

        // 0x00 vfptr
        TerminationHandler* pNext; // 0x04
    };

    void registerTerminationHandler(TerminationHandler* pHandler); // 0x00C00980 (:11)
    void unregisterTerminationHandler(TerminationHandler* pHandler); // 0x00C009A0 (:16)
    void callTerminationHandlers(TerminationStep terminationStep); // 0x00C009C0 (:21)
}
