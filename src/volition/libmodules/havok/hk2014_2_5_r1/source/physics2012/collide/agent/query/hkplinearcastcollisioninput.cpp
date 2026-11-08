// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.inl
//
// Havok 2014.2.5 (havok_impl.md HK7): the linear cast input from a process collision input, from rfg.exe (inline in
// the SDK; emitted out of line).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"

// 0x0081BD00 (hkplinearcastcollisioninput.inl:23): the collision input's fields and the agent config; the path and the
// extra penetration stay
void hkpLinearCastCollisionInput::set(const hkpProcessCollisionInput& input)
{
    m_dispatcher = input.m_dispatcher;
    m_weldClosestPoints = input.m_weldClosestPoints;
    m_forceAcceptContactPoints = input.m_forceAcceptContactPoints;
    m_tolerance = input.m_tolerance;
    m_filter = input.m_filter;
    m_convexListFilter = input.m_convexListFilter;
    m_createPredictiveAgents = input.m_createPredictiveAgents;
    m_aabb32Info = input.m_aabb32Info;
    m_config = input.m_config;
}
