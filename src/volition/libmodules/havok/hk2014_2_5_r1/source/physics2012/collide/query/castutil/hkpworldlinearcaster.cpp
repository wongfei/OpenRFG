// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldlinearcaster.cpp
//
// Havok 2014.2.5 (havok_impl.md HK5): world linear casts (a collidable swept along a path), from rfg.exe. The
// collidable's box (grown by the start point tolerance) is cast through the broadphase; each object it passes is
// filtered against the cast collidable and handed to the dispatcher's linear cast function for the shape type pair
// (the agents, HK6). The out-of-line copy of hkpLinearCastInput's inline constructor is here too.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldlinearcaster.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollidablecollidablefilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

// 0x0080CC40 (hkplinearcastinput.h:28): m_to is left uninitialized
hkpLinearCastInput::hkpLinearCastInput()
{
    m_maxExtraPenetration = 1.192092896e-07f; // HK_REAL_EPSILON
    m_startPointTolerance = 1.192092896e-07f;
}

// 0x00DEA2C0 (.h:33): only the linear cast collision input's constructor runs
hkpWorldLinearCaster::hkpWorldLinearCaster()
{
}

// 0x00DE86D0 (.h:35)
hkpWorldLinearCaster::~hkpWorldLinearCaster()
{
}

// 0x0104BD80
// The collision input is copied from collInput (with config), the path runs from collA's position to input.m_to and
// the tolerance is the start point tolerance. The box of collA's shape (grown by that tolerance) is cast from its
// centre along the path through the broadphase (castAabb, with the broadphase cache when one is given).
void hkpWorldLinearCaster::linearCast(const hkpBroadPhase& broadphase, const hkpCollidable* collA, const hkpLinearCastInput& input, const hkpCollidableCollidableFilter* filter, const hkpCollisionInput& collInput, hkpCollisionAgentConfig* config, char* m_broadPhaseCache, hkpCdPointCollector& castCollector, hkpCdPointCollector* startPointCollector)
{
    m_castCollector = &castCollector;
    m_startPointCollector = startPointCollector;
    m_collidableA = collA;
    m_filter = filter;
    m_input = &input;
    m_typeA = (hkcdShapeType::ShapeTypeEnum)collA->m_shape->m_type.m_storage;

    // the hkpCollisionInput part, member by member (the padding at 0x1C is not copied)
    m_shapeInput.m_dispatcher = collInput.m_dispatcher;
    m_shapeInput.m_weldClosestPoints = collInput.m_weldClosestPoints;
    m_shapeInput.m_forceAcceptContactPoints = collInput.m_forceAcceptContactPoints;
    m_shapeInput.m_tolerance = collInput.m_tolerance;
    m_shapeInput.m_filter = collInput.m_filter;
    m_shapeInput.m_convexListFilter = collInput.m_convexListFilter;
    m_shapeInput.m_createPredictiveAgents = collInput.m_createPredictiveAgents;
    m_shapeInput.m_aabb32Info = collInput.m_aabb32Info;
    m_shapeInput.m_config = config;

    const hkTransformf& transform = collA->getTransform();
    hkVector4f path;
    path.setSub(input.m_to, transform.getTranslation());
    m_shapeInput.setPathAndTolerance(path, input.m_startPointTolerance);

    hkAabb aabb;
    collA->m_shape->getAabb(transform, input.m_startPointTolerance, aabb);
    m_shapeInput.m_maxExtraPenetration = input.m_maxExtraPenetration;

    hkpBroadPhase::hkpCastAabbInput aabbInput;
    aabbInput.m_halfExtents.setSub(aabb.m_max, aabb.m_min);
    aabbInput.m_halfExtents.mul(hkSimdFloat32::getConstant<HK_QUADREAL_INV_2>());
    aabbInput.m_from.setAdd(aabbInput.m_halfExtents, aabb.m_min);
    aabbInput.m_to.setAdd(aabbInput.m_from, path);
    aabbInput.m_aabbCacheInfo = m_broadPhaseCache;
    broadphase.castAabb(aabbInput, *this);
}

// 0x0104BCF0
// One object along the cast: a collidable with a shape, other than collA, that the filter accepts goes to the
// dispatcher's linear cast function for (collA's type, its type). Returns the cast collector's early-out distance.
float hkpWorldLinearCaster::addBroadPhaseHandle(const hkpBroadPhaseHandle* broadPhaseHandle, int castIndex)
{
    const hkpCollidable* collidable = (const hkpCollidable*)static_cast<const hkpTypedBroadPhaseHandle*>(broadPhaseHandle)->getOwner();
    const hkpShape* shape = collidable->m_shape;
    if (shape != nullptr && m_collidableA != collidable)
    {
        if (m_filter->isCollisionEnabled(*m_collidableA, *collidable).m_bool)
        {
            const hkpCollisionDispatcher* dispatcher = m_shapeInput.m_dispatcher;
            const int index = dispatcher->m_agent2Types[m_typeA][shape->m_type.m_storage];
            dispatcher->m_agent2Func[index].m_linearCastFunc(*m_collidableA, *collidable, m_shapeInput, *m_castCollector, m_startPointCollector);
        }
    }
    return m_castCollector->m_earlyOutDistance;
}
