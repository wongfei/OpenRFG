// Original: volition/rfg/code/havok/havok_raycast_collectors.h
//
// Ray hit collectors of the game queries (closest hit, filtered by the game filter callback).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpallcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpclosestcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/raycollector/hkpallrayhitcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/raycollector/hkpclosestrayhitcollector.h"

class hkpCdBody;
struct hkpCdPoint;
struct hkpShapeRayCastCollectorOutput;

// size 0x70
class havok_closest_ray_hit_collector : public hkpClosestRayHitCollector
{
protected:
    virtual void addRayHit(const hkpCdBody& cd_body, const hkpShapeRayCastCollectorOutput& hitInfo); // vtbl+0x00 0x00815C40 (.h:26)
public:
    havok_closest_ray_hit_collector(); // 0x00812650
    virtual ~havok_closest_ray_hit_collector(); // vtbl+0x04 0x00812680

};

// size 0x320
class havok_all_ray_hit_collector : public hkpAllRayHitCollector
{
protected:
    virtual void addRayHit(const hkpCdBody& cd_body, const hkpShapeRayCastCollectorOutput& hitInfo); // vtbl+0x00 0x008332B0 (.h:69)
public:
    havok_all_ray_hit_collector(); // 0x00833190
    virtual ~havok_all_ray_hit_collector(); // vtbl+0x04 0x008333B0

};

// size 0x50
class havok_closest_linear_cast_collector : public hkpClosestCdPointCollector
{
public:
    havok_closest_linear_cast_collector(); // 0x00815D00 (.h:109)
    virtual void reset(); // vtbl+0x08 0x00815D60 (.h:118)
protected:
    virtual void addCdPoint(const hkpCdPoint& point); // vtbl+0x04 0x00815D50 (.h:137)
public:
    virtual ~havok_closest_linear_cast_collector(); // vtbl+0x00 0x0080D940

};

// size 0x1A0
class havok_all_linear_cast_collector : public hkpAllCdPointCollector
{
public:
    virtual void reset(); // vtbl+0x08 0x0074D590 (.h:171)
protected:
    virtual void addCdPoint(const hkpCdPoint& point); // vtbl+0x04 0x0074D580 (.h:190)
public:
    havok_all_linear_cast_collector(); // 0x0074D550
    virtual ~havok_all_linear_cast_collector(); // vtbl+0x00 0x0074D5B0

};

// size 0x1B0
class havok_closest_point_per_object_collector : public havok_all_linear_cast_collector
{
public:
    virtual void reset(); // vtbl+0x08 0x00833510 (.h:214)
protected:
    virtual void addCdPoint(const hkpCdPoint& point); // vtbl+0x04 0x00833530 (.h:224)
public:
    havok_closest_point_per_object_collector(); // 0x008334E0
    virtual ~havok_closest_point_per_object_collector(); // vtbl+0x00 0x00833720

    hkVector4f center; // 0x1A0
};
