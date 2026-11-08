// Original: library/modules/runtime/base/include/keen/base/statetree.hpp
// Implementation: library/modules/runtime/base/sources/global/statetree.cpp
//
// StateTree, hierarchical state machines: a definition (states and transitions) and instances stepping through
// transitions.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum StateTreeStateId
    {
        StateTreeStateId_Invalid = -1,
        StateTreeStateId_Root = 0,
    };

    // size 0x8
    struct StateTreeTransitionDefinition
    {
        const uint32* pTransitionStepIds; // 0x00
        uint32 stepCount; // 0x04
    };

    // size 0x20
    struct StateTreeTransition
    {
        bool isInitialUpdate() const;
        bool isStarting() const;

        int sourceStateId; // 0x00
        int targetStateId; // 0x04
        int currentStateId; // 0x08
        int nextStateId; // 0x0C
        uint32 currentStepId; // 0x10
        int currentStepIndex; // 0x14
        uint32 currentStepUpdateCount; // 0x18
        uint32 totalUpdateCount; // 0x1C
    };

    // size 0xC
    struct StateTreeDefinition
    {
        bool checkInvariants() const; // 0x00C09490 (:82)
        bool isValidStateId(int stateId) const; // 0x00BF18E0 (.h:43)

        const int* pStateParentIds; // 0x00
        uint32 stateCount; // 0x04
        const StateTreeTransitionDefinition* pTransitions; // 0x08
    };

    // size 0x28
    struct StateTreeInstance
    {
        bool checkInvariants() const; // 0x00C095A0 (:222)

        const StateTreeDefinition* pDefinition; // 0x00
        int currentStateId; // 0x04
        StateTreeTransition transition; // 0x08
    };

    namespace StateTree
    {
        bool isInTransition(const StateTreeInstance& stateTree); // 0x00BF1900 (.h:100)
        int findNextTransitionStateId(const StateTreeDefinition& definition, int currentStateId, int targetStateId); // 0x00C00440 (:122)
        bool createInstance(StateTreeInstance* pStateTree, const StateTreeDefinition& definition); // 0x00C09510 (:169)
        void destroyInstance(StateTreeInstance* pStateTree); // 0x00C09560 (:199)
        bool isInState(const StateTreeInstance& stateTree, int stateId); // 0x00C004A0 (:303)
        bool startTransition(StateTreeInstance* pStateTree, int targetStateId); // 0x00C09670 (:331)
        const StateTreeTransition* updateTransition(StateTreeInstance* pStateTree); // 0x00BF1940 (:404)
        void finishCurrentStep(StateTreeInstance* pStateTree); // 0x00C041B0 (:421)
    }
}
