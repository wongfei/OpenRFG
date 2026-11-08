// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/agent/hkpgskagentutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkVector4f;
struct hkpAgent3Input;
struct hkpAgentEntry;
class hkpGskCache;
struct hkpGskManifold;
struct hkpProcessCollisionOutput;

void hkGskAgentUtil_processCollisionNoTim(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData, hkpGskCache& gskCache, hkpGskManifold& gskManifold, hkVector4f& separatingNormal, int explicitlyAllowNewPoint, hkpProcessCollisionOutput& output); // 0x00E84330
