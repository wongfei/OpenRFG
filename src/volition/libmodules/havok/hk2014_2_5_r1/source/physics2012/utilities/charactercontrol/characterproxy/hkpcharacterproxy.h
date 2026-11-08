// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxy.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (player_minimal.md P0). Hand-maintained since P1 (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkprootcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentitylistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomlistener.h"

class hkBool;
class hkContactPoint;
struct hkSimplexSolverInput;
struct hkSimplexSolverOutput;
class hkStepInfo;
struct hkSurfaceConstraintInfo;
class hkpAllCdPointCollector;
struct hkpCharacterObjectInteractionEvent;
struct hkpCharacterObjectInteractionResult;
class hkpCharacterProxyCinfo;
struct hkpCharacterProxyIntegrateCommand;
class hkpCharacterProxyListener;
class hkpCollidable;
class hkpEntity;
struct hkpLinearCastInput;
class hkpPhantom;
class hkpRigidBody;
class hkpShapePhantom;
struct hkpSurfaceInfo;
struct hkpSurfaceInfoDeprecated;
class hkpTriggerVolume;

// size 0xC0
class hkpCharacterProxy : public hkReferencedObject, public hkpEntityListener, public hkpPhantomListener
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00814740, delete(void* p, unsigned int nbytes) 0x00833B10
    hkpCharacterProxy(const hkpCharacterProxyCinfo& info); // 0x00F1A0C0
    virtual ~hkpCharacterProxy(); // vtbl+0x00 0x00F19D20
    void getCinfo(hkpCharacterProxyCinfo& info) const; // 0x00F188A0
    void updateFromCinfo(const hkpCharacterProxyCinfo& info); // 0x00F19A30
    void integrate(const hkStepInfo& stepInfo, const hkVector4f& worldGravity); // 0x00F1C630
    void integrateWithCollectors(const hkStepInfo& stepInfo, const hkVector4f& gravity, hkpAllCdPointCollector& castCollector, hkpAllCdPointCollector& startPointCollector); // 0x00F1C740
    void checkSupport(const hkVector4f& direction, hkpSurfaceInfo& ground); // 0x00F1C760
    void checkSupportWithCollector(const hkVector4f& direction, hkpSurfaceInfo& ground, hkpAllCdPointCollector& startPointCollector); // 0x00F1A230
    void refreshManifold(hkpAllCdPointCollector& startPointCollector); // 0x00F1A140
    const hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& getManifold() const; // 0x00F18810
    const hkVector4f& getPosition() const; // 0x00F18820
    void setPosition(const hkVector4f& position); // 0x00F18830
    const hkVector4f& getLinearVelocity() const; // 0x00F18860
    void setLinearVelocity(const hkVector4f& vel); // 0x00F18870
    const hkpShapePhantom* getShapePhantom() const; // 0x00F18890
    hkpShapePhantom* getShapePhantom(); // 0x00F18880
    void checkSupportDeprecated(const hkVector4f& direction, hkpSurfaceInfoDeprecated& ground) const; // 0x00F1AA30
    void addCharacterProxyListener(hkpCharacterProxyListener* cpl); // 0x00F19580
    void removeCharacterProxyListener(hkpCharacterProxyListener* cpl); // 0x00F18F30
    void integrateImplementation(const hkStepInfo& stepInfo, const hkVector4f& gravity, hkpCharacterProxyIntegrateCommand* command, hkpAllCdPointCollector& castCollector, hkpAllCdPointCollector& startCollector); // 0x00F1AFB0
public:
    virtual void updateManifold(const hkpAllCdPointCollector& startCollector, const hkpAllCdPointCollector& castCollector, hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& manifold, hkArray<hkpRigidBody*,hkContainerHeapAllocator>& bodies, hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantoms, hkBool isMultithreaded); // vtbl+0x10 0x00F1C060
    void applySurfaceInteractions(const hkStepInfo& stepInfo, const hkVector4f& worldGravity, hkpCharacterProxyIntegrateCommand* command, hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& manifold); // 0x00F190C0
    virtual void extractSurfaceConstraintInfo(const hkpRootCdPoint& hit, hkSurfaceConstraintInfo& surface, float timeTravelled) const; // vtbl+0x14 0x00F18B60
    int findSurface(const hkpRootCdPoint& info, hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& manifold) const; // 0x00F18F80
    float surfaceDistance(const hkpRootCdPoint& p1, const hkpRootCdPoint& p2) const; // 0x00F1D090
    void extractSurfaceVelocity(const hkpRootCdPoint& hit, hkVector4f& velocityOut) const; // 0x00F1C850
    void convertFractionToDistance(const hkpAllCdPointCollector& collector, const hkVector4f& displacement) const; // 0x00F1CCD0
    void convertFractionToDistance(const hkpRootCdPoint* cHit, int numHits, const hkVector4f& displacement) const; // 0x00F1CC30
    void fireContactAdded(const hkpRootCdPoint& point) const; // 0x00F18B00
    void fireContactRemoved(const hkpRootCdPoint& point) const; // 0x00F18B30
    void fireConstraintsProcessed(const hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& manifold, hkSimplexSolverInput& input) const; // 0x00F18AC0
    void fireObjectInteraction(const hkpCharacterObjectInteractionEvent& input, hkpCharacterObjectInteractionResult& output); // 0x00F18A80
    void fireCharacterInteraction(hkpCharacterProxy* otherProxy, const hkContactPoint& contact); // 0x00F18A40
    void addEntityOrPhantom(const hkpCollidable* collidable, hkArray<hkpRigidBody*,hkContainerHeapAllocator>& bodies, hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantoms); // 0x00F1D420
    // returns float in rfg.pdb; the x87 result in ST0 is the exact (double) product, which the callers use unrounded
    double moveToLinearCastHitPosition(const hkSimplexSolverOutput& output, const hkpRootCdPoint& hit, const hkpLinearCastInput& castInput, hkVector4f& position); // 0x00F197D0
public:
    virtual void entityRemovedCallback(hkpEntity* entity); // vtbl+0x08 0x00F18D10
    virtual void phantomRemovedCallback(hkpPhantom* phantom); // vtbl+0x08 0x00F18E60
    void processTriggerVolumes(hkpTriggerVolume** triggerVolumeAndFlags, const int maxNumTriggerVolumes); // 0x00F19F00
public:
    void removeTriggerVolumes(hkpAllCdPointCollector& collector, hkLocalArray<hkpTriggerVolume*>& touchedTriggers, hkLocalArray<hkpRootCdPoint>& triggerHits); // 0x00F195C0
    void updateTriggerVolumes(hkLocalArray<hkpTriggerVolume*>& touchedTriggers, hkArrayBase<hkpTriggerVolume*>& triggerVolumeAndFlags); // 0x00F19740
    void triggerVolumeTouched(hkpTriggerVolume* triggerVolume, hkArrayBase<hkpTriggerVolume*>& triggerVolumeAndFlags); // 0x00F19000
    void updateNearTriggerVolumes(hkLocalArray<hkpTriggerVolume*>& touchedTriggers, hkLocalArray<hkpRootCdPoint>& triggerHits, hkArrayBase<hkpTriggerVolume*>& triggerVolumeAndFlags, float distance); // 0x00F19060

public:
    hkArray<hkpRootCdPoint,hkContainerHeapAllocator> m_manifold; // 0x10
    hkArray<hkpRigidBody*,hkContainerHeapAllocator> m_bodies; // 0x1C
    hkArray<hkpPhantom*,hkContainerHeapAllocator> m_phantoms; // 0x28
    hkArray<hkpTriggerVolume*,hkContainerHeapAllocator> m_overlappingTriggerVolumes; // 0x34
    hkVector4f m_velocity; // 0x40
    hkVector4f m_oldDisplacement; // 0x50
    hkpShapePhantom* m_shapePhantom; // 0x60
    float m_dynamicFriction; // 0x64
    float m_staticFriction; // 0x68
    hkVector4f m_up; // 0x70
    float m_extraUpStaticFriction; // 0x80
    float m_extraDownStaticFriction; // 0x84
    float m_keepDistance; // 0x88
    float m_keepContactTolerance; // 0x8C
    float m_contactAngleSensitivity; // 0x90
    int m_userPlanes; // 0x94
    float m_maxCharacterSpeedForSolver; // 0x98
    float m_characterStrength; // 0x9C
    float m_characterMass; // 0xA0
    hkArray<hkpCharacterProxyListener*,hkContainerHeapAllocator> m_listeners; // 0xA4
    float m_maxSlopeCosine; // 0xB0
    float m_penetrationRecoverySpeed; // 0xB4
    int m_maxCastIterations; // 0xB8
    bool m_refreshManifoldInCheckSupport; // 0xBC
};
