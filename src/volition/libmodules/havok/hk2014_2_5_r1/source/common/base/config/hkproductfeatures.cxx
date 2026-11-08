// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkproductfeatures.cxx
//
// Havok 2014.2.5 (havok_impl.md HK5, the simulations HK7, the version patches stub_impl.md S4 SG):
// hkProductFeatures::initialize, compiled into the game with its feature list (volition/rfg/code/havok/hkproductfeatures.inl,
// included first by havok_cm_manager.cpp). hkBaseSystem::init calls it; it requests the optional components, which install the
// creation and cast function pointers, and registers Havok's class version patches.
//
// rfg.exe (0x00819910) requests, in this order: hkSerializeDeprecated, the version patches (clearPatches if any,
// registerVersionPatches 0x008197B0, recomputePatchDependencies), hkpHeightFieldAgent, hkpSimulation,
// hkpContinuousSimulation, hkpMultiThreadedSimulation, hkpAccurateInertiaTensorComputer, hkp3AxisSweep,
// hkpTreeBroadPhase, hkpTreeBroadPhase32 and hkpSampledHeightField_AllCasts. Only the components whose code is
// reimplemented are requested here; the others are listed where they are skipped:
// - hkSerializeDeprecated installs hkSerializeDeprecated2 (the pre-7.0 packfile loader), not ported yet (SG part 2);
//   the default singleton (which loads nothing) stays in place. RFG's data holds no packfiles.
// - hkpHeightFieldAgent registers the height field agent (HK6; the terrain is a tri-sampled BV tree, so the agent is
//   not reached).
// - hkpMultiThreadedSimulation and the tree broadphases are out of scope (havok_impl.md section 2): the game's world
//   uses the continuous simulation and the 3-axis sweep, so their function pointers are never called.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkproductfeatures.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkoptionalcomponent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanager.h"

// 0x008197B0 (:41)
static void registerVersionPatches()
{
    hkVersionPatchManager& man = hkVersionPatchManager::getInstance();
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/patches/hkregisterpatches.cxx"
}

// 0x00819910 (:51)
void hkProductFeatures::initialize()
{
    // HK_OPTIONAL_COMPONENT_REQUEST(hkSerializeDeprecated): SG part 2

    if (hkVersionPatchManager::getInstance().getNumPatches() > 0)
    {
        hkVersionPatchManager::getInstance().clearPatches();
    }
    registerVersionPatches();
    hkVersionPatchManager::getInstance().recomputePatchDependencies();

#ifdef HK_FEATURE_PRODUCT_PHYSICS_2012
    // HK_OPTIONAL_COMPONENT_REQUEST(hkpHeightFieldAgent): HK6
    HK_OPTIONAL_COMPONENT_REQUEST(hkpSimulation);
    HK_OPTIONAL_COMPONENT_REQUEST(hkpContinuousSimulation);
    // HK_OPTIONAL_COMPONENT_REQUEST(hkpMultiThreadedSimulation): out of scope
    HK_OPTIONAL_COMPONENT_REQUEST(hkpAccurateInertiaTensorComputer);
    HK_OPTIONAL_COMPONENT_REQUEST(hkp3AxisSweep);
    // HK_OPTIONAL_COMPONENT_REQUEST(hkpTreeBroadPhase), (hkpTreeBroadPhase32): out of scope
    HK_OPTIONAL_COMPONENT_REQUEST(hkpSampledHeightField_AllCasts);
#endif
}
