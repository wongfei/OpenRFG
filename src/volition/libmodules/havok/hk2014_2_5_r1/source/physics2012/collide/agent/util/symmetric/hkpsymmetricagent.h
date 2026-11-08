// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); the template is ported from the 2014.1 SDK
// (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"

class hkpCdBody;
class hkpCdPointCollector;
struct hkpCollisionInput;
class hkpContactMgr;
struct hkpLinearCastCollisionInput;

// size (<hkpBvTreeAgent>) 0x40
// hkpSymmetricAgentLinearCast with the virtual linear cast flipped too (hkpSymmetricAgent.inl of the 2014.1 SDK).
template<typename AGENT>
class hkpSymmetricAgent : public hkpSymmetricAgentLinearCast<AGENT>
{
public:
    hkpSymmetricAgent(const hkpCdBody& A, const hkpCdBody& B, const hkpCollisionInput& input, hkpContactMgr* mgr)
        : hkpSymmetricAgentLinearCast<AGENT>(A, B, input, mgr)
    {
    }

    // <hkpBvTreeAgent> 0x00D79CF0
    hkpSymmetricAgent(hkpContactMgr* mgr)
        : hkpSymmetricAgentLinearCast<AGENT>(mgr)
    {
    }

    // vtbl+0x00 <hkpBvTreeAgent> 0x00D79D10: implicit

    // vtbl+0x18 <hkpBvTreeAgent> 0x00D7AD00
    // The start collector is a cast collector here (the static version wraps it as a point collector).
    virtual void linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
    {
        hkpLinearCastCollisionInput flippedInput = input;
        flippedInput.m_path.setNeg<4>(input.m_path);
        hkpSymmetricAgentFlipCastCollector flip(input.m_path, collector);
        if (startCollector != nullptr)
        {
            hkpSymmetricAgentFlipCastCollector startFlip(input.m_path, *startCollector);
            AGENT::linearCast(bodyB, bodyA, flippedInput, flip, &startFlip);
        }
        else
        {
            AGENT::linearCast(bodyB, bodyA, flippedInput, flip, nullptr);
        }
    }
};
