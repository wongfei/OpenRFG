// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldmemoryutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the world's reactions to low memory (the memory watchdog), from rfg.exe.
// The shipped build's monitor timer ("TtWatchDog:FreeMem") is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldmemoryutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/hkmemorysystem.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"

// 0x00E1BB20: the watchdog frees heap memory when the heap cannot give what it asks for
void hkpWorldMemoryUtil::watchHeapMemory(hkpWorld* world)
{
    if (world->getMemoryWatchDog() == nullptr)
    {
        return;
    }
    const int numBytes = world->getMemoryWatchDog()->getAmountOfFreeHeapMemoryRequested();
    if (!hkMemorySystem::getInstance().heapCanAllocTotal(numBytes))
    {
        world->getMemoryWatchDog()->freeHeapMemoryTillRequestedAmountIsAvailable(world);
    }
    if (hkOutOfMemoryState != hkMemoryAllocator::MEMORY_STATE_OK)
    {
        hkSetOutOfMemoryState(hkMemoryAllocator::MEMORY_STATE_OK);
    }
}

// 0x00E1B9A0: until the solver buffer of the largest island fits, sparse islands are split (with a halving minimum
// island size while a split changes nothing), islands that are not fully connected are marked sparse, and the
// watchdog reduces the constraints of the rest; the minimum island size is restored at the end
void hkpWorldMemoryUtil::checkMemoryForIntegration(hkpWorld* world)
{
    world->lock();
    const unsigned int minDesiredIslandSize = world->m_minDesiredIslandSize;
    int iteration = 0;
    hkWorldMemoryAvailableWatchDog::MemUsageInfo info;
    world->calcRequiredSolverBufferSize(info);
    while (!hkMemorySystem::getInstance().solverCanAllocSingleBlock(info.m_maxRuntimeBlockSize))
    {
        iteration++;
        hkpSimulationIsland* island = info.m_largestSimulationIsland;
        while (island->m_isSparse)
        {
            hkpSimulationIsland* splitIsland = island;
            const int maxRuntimeBlockSize = info.m_maxRuntimeBlockSize;
            bool noMinimum = false;
            do
            {
                if (world->m_minDesiredIslandSize == 0)
                {
                    noMinimum = true;
                    break;
                }
                hkpWorldOperationUtil::splitSimulationIsland(world, splitIsland);
                world->calcRequiredSolverBufferSize(info);
                island = info.m_largestSimulationIsland;
                if (island != splitIsland || info.m_maxRuntimeBlockSize != maxRuntimeBlockSize)
                {
                    break;
                }
                world->m_minDesiredIslandSize >>= 1;
            } while (island->m_isSparse);
            if (noMinimum || world->m_minDesiredIslandSize == 0)
            {
                break;
            }
        }
        if (!hkMemorySystem::getInstance().solverCanAllocSingleBlock(info.m_maxRuntimeBlockSize))
        {
            island = info.m_largestSimulationIsland;
            if (!island->isFullyConnected() && world->m_minDesiredIslandSize != 0)
            {
                island->m_isSparse = true;
            }
            else
            {
                world->getMemoryWatchDog()->reduceConstraintsInIsland(info, iteration);
                world->calcRequiredSolverBufferSize(info);
            }
        }
    }
    world->m_minDesiredIslandSize = minDesiredIslandSize;
    world->unlock();
}

// 0x00E1BAE0: the step parts that failed run again (collide after a collide failure, the TOIs after a TOI failure)
void hkpWorldMemoryUtil::repeatCollideAndToiHandling(hkpWorld* world)
{
    const unsigned int result = world->m_simulation->m_previousStepResult;
    if (result == HK_STEP_RESULT_SUCCESS || result == HK_STEP_RESULT_MEMORY_FAILURE_DURING_COLLIDE)
    {
        world->m_simulation->collide();
    }
    const unsigned int resultAfterCollide = world->m_simulation->m_previousStepResult;
    if (resultAfterCollide == HK_STEP_RESULT_SUCCESS || resultAfterCollide == HK_STEP_RESULT_MEMORY_FAILURE_DURING_TOI_SOLVE)
    {
        world->m_simulation->advanceTime();
    }
}

// 0x00E1BBF0: up to 10 attempts to free memory and finish the step
void hkpWorldMemoryUtil::tryToRecoverFromMemoryErrors(hkpWorld* world)
{
    if (world->getMemoryWatchDog() == nullptr)
    {
        return;
    }
    for (int i = 10; world->m_simulation->m_previousStepResult != HK_STEP_RESULT_SUCCESS && i > 0; i--)
    {
        world->getMemoryWatchDog()->freeHeapMemoryTillRequestedAmountIsAvailable(world);
        hkSetOutOfMemoryState(hkMemoryAllocator::MEMORY_STATE_OK);
        repeatCollideAndToiHandling(world);
    }
}
