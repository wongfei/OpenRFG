// Original: volition/rfg/code/havok/hk_custom_character_proxy.h
// Implementation: volition/rfg/code/havok/hk_custom_character_proxy.cpp
//
// The character proxy of the character controller (hkcharactercontroller.h): Havok's hkpCharacterProxy with its
// integration split into steps so that the proxies of all characters run their linear casts together
// (mt_integrate_*), and with the rigid bodies a character touches pushed after the step (mt_push_objects).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/simplexsolver/hksimplexsolver.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpallcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxy.h"

struct hkpCdPoint;
class hkpCharacterProxyCinfo;
class hkpCollidable;
class hkpRigidBody;
struct hkpRootCdPoint;

// size 0x30
struct character_proxy_contact_to_push
{
    character_proxy_contact_to_push(); // 0x00813BC0

    hkContactPoint m_contact; // 0x00
    const hkpCollidable* m_collidable; // 0x20
};

// size 0x1B0
class character_proxy_cd_point_collector : public hkpAllCdPointCollector
{
public:
    struct MustEndWithSemiColon; // no definition in PDB

    static void* operator new(unsigned int, void*);
    static void* operator new(unsigned int);
    static void operator delete(void*, void*);
    static void operator delete(void* p, unsigned int nbytes); // 0x00833820 (.h:57)
    static void* operator new[](unsigned int, void*);
    static void operator delete[](void*, void*);
    virtual void addCdPoint(const hkpCdPoint& event); // vtbl+0x04 0x00830250 (:240)
    character_proxy_cd_point_collector(); // 0x008337F0
    virtual ~character_proxy_cd_point_collector(); // vtbl+0x00 0x00833850

    hkArray<character_proxy_contact_to_push,hkContainerHeapAllocator>* m_contacts_to_push; // 0x1A0
};

// size 0x500
class hk_custom_character_proxy : public hkpCharacterProxy
{
public:
    // size 0xA0
    struct multithreaded_integration
    {
        multithreaded_integration(); // 0x008208A0

        float m_remaining_time; // 0x00
        hkTransformf m_transform; // 0x10
        hkpLinearCastInput m_linearcast_input; // 0x50
        hkSimplexSolverOutput m_simplex_solver_output; // 0x70
    };

    // size 0x30
    struct apply_surface_interactions_params
    {
        apply_surface_interactions_params(); // 0x008208C0

        hkStepInfo m_step_info; // 0x00
        hkVector4f m_world_gravity; // 0x10
        hkVector4f m_velocity; // 0x20
    };

    hk_custom_character_proxy(const hkpCharacterProxyCinfo& c_info); // 0x00837520 (:432)
    virtual ~hk_custom_character_proxy(); // vtbl+0x00 0x00833920 (:451)
    character_proxy_cd_point_collector& get_cast_collector(); // 0x0080D2A0 (.h:87)
    character_proxy_cd_point_collector& get_start_point_collector(); // 0x0080D2B0 (.h:93)
    void push_rigid_body(hkpRigidBody* p_rigid_body, const hkContactPoint& contact_point); // 0x0082B480 (:462)
    void refreshManifold_custom(character_proxy_cd_point_collector& startPointCollector, character_proxy_cd_point_collector& castCollector); // 0x00837640 (:492)
    void mt_integrate_start_initial_casts(float time_step); // 0x00830780 (:558)
    void mt_integrate_process_casts(const hkStepInfo& step_info, const hkVector4f& world_gravity); // 0x00837870 (:590)
    bool mt_integrate_further_casts(bool last_iteration); // 0x00833B50 (:621)
    void mt_push_objects(); // 0x0082B5C0 (:653)
    void mt_integrate_finish(); // 0x0082B610 (:676)
protected:
    float surfaceDistance(const hkpRootCdPoint& p1, const hkpRootCdPoint& p2) const; // 0x00826860 (:799)
    void extractSurfaceVelocity(const hkpRootCdPoint& hit, hkVector4f& velocityOut) const; // 0x008126C0 (:821)
    void convertFractionToDistance(const hkpRootCdPoint* cHit, int numHits, const hkVector4f& displacement) const; // 0x008208D0 (:835)
    void mt_integrate_linear_cast(bool initial_penetration_check); // 0x0082B7E0 (:854)
    void updateManifold_mt(const hkpAllCdPointCollector& startPointCollector, const hkpAllCdPointCollector& castCollector); // 0x00833BC0 (:985)
    bool mt_integrate_solve_casts(const hkStepInfo& stepInfo, const hkVector4f& worldGravity); // 0x00834290 (:1189)
    void mt_integrate_move(); // 0x00830830 (:1315)

    multithreaded_integration m_mti; // 0xC0
    apply_surface_interactions_params m_apply_surface_interactions_params; // 0x160
    hkArray<character_proxy_contact_to_push,hkContainerHeapAllocator> m_contacts_to_push; // 0x190
    character_proxy_cd_point_collector m_cast_collector; // 0x1A0
    character_proxy_cd_point_collector m_start_collector; // 0x350
};
