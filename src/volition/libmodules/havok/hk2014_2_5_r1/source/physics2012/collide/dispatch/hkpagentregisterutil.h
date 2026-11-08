// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpagentregisterutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpCollisionDispatcher;

// size 0x1
class hkpAgentRegisterUtil
{
public:
    static void registerAllAgents(hkpCollisionDispatcher* dis); // 0x00D9AF20
    static void _registerBvTreeAgents(hkpCollisionDispatcher* dis); // 0x00D9AE10
    static void _registerTerminalAgents(hkpCollisionDispatcher* dis); // 0x00D9AEC0
    static void _registerListAgents(hkpCollisionDispatcher* dis); // 0x00D9AE60

};
