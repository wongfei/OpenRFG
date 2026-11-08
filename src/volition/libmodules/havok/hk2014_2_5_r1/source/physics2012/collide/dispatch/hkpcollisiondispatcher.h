// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); inline members of the 2014.1 SDK .inl added by
// hand (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidablequalitytype.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"

class hkTransformf;
class hkVector4f;
struct hkpAgent3Input;
struct hkpAgent3ProcessInput;
struct hkpAgentEntry;
class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollisionAgent;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
class hkpContactMgrFactory;
struct hkpLinearCastCollisionInput;
struct hkpProcessCollisionOutput;
class hkpShape;

// size 0x2100
class hkpCollisionDispatcher : public hkReferencedObject
{
public:
    enum
    {
        HK_MAX_RESPONSE_TYPE = 8,
        HK_MAX_SHAPE_TYPE = 35,
        HK_MAX_AGENT2_TYPES = 64,
        HK_MAX_AGENT3_TYPES = 18,
        HK_MAX_COLLISION_QUALITIES = 8,
    };

    enum IsAgentPredictive
    {
        IS_NOT_PREDICTIVE = 0,
        IS_PREDICTIVE = 1,
    };

    enum CollisionQualityLevel
    {
        COLLISION_QUALITY_INVALID = 0,
        COLLISION_QUALITY_PSI = 1,
        COLLISION_QUALITY_SIMPLIFIED_TOI = 2,
        COLLISION_QUALITY_TOI = 3,
        COLLISION_QUALITY_TOI_HIGHER = 4,
        COLLISION_QUALITY_TOI_FORCED = 5,
        COLLISION_QUALITY_CHARACTER = 6,
        COLLISION_QUALITY_TMP_EXPAND_MANIFOLD = 7,
    };

    typedef hkpCollisionAgent* (*CreateFunc)(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpContactMgr*);
    typedef void (*GetPenetrationsFunc)(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpCdBodyPairCollector&);
    typedef void (*GetClosestPointsFunc)(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpCdPointCollector&);
    typedef void (*LinearCastFunc)(const hkpCdBody&, const hkpCdBody&, const hkpLinearCastCollisionInput&, hkpCdPointCollector&, hkpCdPointCollector*);
    typedef hkpCollisionAgent* (*CollisionAgentCreationFunction)(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpContactMgr*);
    typedef signed char CollisionQualityIndex;
    struct DebugEntry;
    typedef DebugEntry DebugTable[35][35];

    // size 0x14
    struct AgentFuncs
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(AgentFuncs);
        // 0x00D78340
        AgentFuncs()
        {
            m_isFlipped = false;
            m_isPredictive = false;
        }

        hkpCollisionAgent* (*m_createFunc)(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpContactMgr*); // 0x00
        void (*m_getPenetrationsFunc)(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpCdBodyPairCollector&); // 0x04
        void (*m_getClosestPointFunc)(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpCdPointCollector&); // 0x08
        void (*m_linearCastFunc)(const hkpCdBody&, const hkpCdBody&, const hkpLinearCastCollisionInput&, hkpCdPointCollector&, hkpCdPointCollector*); // 0x0C
        hkBool m_isFlipped; // 0x10
        hkBool m_isPredictive; // 0x11
    };

    // size 0x30
    struct alignas(16) Agent3Funcs
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Agent3Funcs);
        // 0x00D9EC50
        Agent3Funcs()
        {
            m_isPredictive = false;
            m_ignoreSymmetricVersion = false;
            m_reusePreviousEntry = false;
            m_updateFilterFunc = nullptr;
            m_invalidateTimFunc = nullptr;
            m_warpTimeFunc = nullptr;
            m_sepNormalFunc = nullptr;
        }

        void* (*m_createFunc)(const hkpAgent3Input&, hkpAgentEntry*, void*); // 0x00
        void (*m_destroyFunc)(hkpAgentEntry*, void*, hkpContactMgr*, hkpConstraintOwner&, hkpCollisionDispatcher*); // 0x04
        void* (*m_cleanupFunc)(hkpAgentEntry*, void*, hkpContactMgr*, hkpConstraintOwner&); // 0x08
        void (*m_removePointFunc)(hkpAgentEntry*, void*, unsigned short); // 0x0C
        void (*m_commitPotentialFunc)(hkpAgentEntry*, void*, unsigned short); // 0x10
        void (*m_createZombieFunc)(hkpAgentEntry*, void*, unsigned short); // 0x14
        void (*m_updateFilterFunc)(hkpAgentEntry*, void*, const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpContactMgr*, hkpConstraintOwner&); // 0x18
        void (*m_invalidateTimFunc)(hkpAgentEntry*, void*, const hkpCollisionInput&); // 0x1C
        void (*m_warpTimeFunc)(hkpAgentEntry*, void*, float, float, const hkpCollisionInput&); // 0x20
        void (*m_sepNormalFunc)(const hkpAgent3Input&, hkpAgentEntry*, void*, hkVector4f&); // 0x24
        void* (*m_processFunc)(const hkpAgent3ProcessInput&, hkpAgentEntry*, void*, hkVector4f*, hkpProcessCollisionOutput&); // 0x28
        hkBool m_isPredictive; // 0x2C
        hkBool m_ignoreSymmetricVersion; // 0x2D
        hkBool m_reusePreviousEntry; // 0x2E
    };

    // size 0x2C
    struct InitCollisionQualityInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(InitCollisionQualityInfo);
        // 0x00DE8910: leaves everything uninitialized
        InitCollisionQualityInfo()
        {
        }

        float m_gravityLength; // 0x00
        float m_collisionTolerance; // 0x04
        float m_minDeltaTime; // 0x08
        float m_maxLinearVelocity; // 0x0C
        float m_numToisTillAllowedPenetrationSimplifiedToi; // 0x10
        float m_numToisTillAllowedPenetrationToi; // 0x14
        float m_numToisTillAllowedPenetrationToiHigher; // 0x18
        float m_numToisTillAllowedPenetrationToiForced; // 0x1C
        unsigned short m_defaultConstraintPriority; // 0x20
        unsigned short m_toiConstraintPriority; // 0x22
        unsigned short m_toiHigherConstraintPriority; // 0x24
        unsigned short m_toiForcedConstraintPriority; // 0x26
        hkBool m_wantContinuousCollisionDetection; // 0x28
        hkBool m_enableNegativeManifoldTims; // 0x29
        hkBool m_enableNegativeToleranceToCreateNon4dContacts; // 0x2A
        hkBool m_enableToiWeldRejection; // 0x2B
    };

    // size 0x8
    struct ShapeInheritance
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ShapeInheritance);

        hkcdShapeType::ShapeTypeEnum m_primaryType; // 0x00
        hkcdShapeType::ShapeTypeEnum m_alternateType; // 0x04
    };

    // size 0x20
    struct AgentEntry
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(AgentEntry);

        hkcdShapeType::ShapeTypeEnum m_typeA; // 0x00
        hkcdShapeType::ShapeTypeEnum m_typeB; // 0x04
        hkBool m_checkEnabled; // 0x08
        AgentFuncs m_createFunc; // 0x0C
    };

    // size 0x40
    struct Agent3Entry
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Agent3Entry);

        hkcdShapeType::ShapeTypeEnum m_typeA; // 0x00
        hkcdShapeType::ShapeTypeEnum m_typeB; // 0x04
        hkBool m_checkEnabled; // 0x08
        Agent3Funcs m_funcs; // 0x10
    };

    // size 0x40
    struct alignas(16) Agent3FuncsIntern : public Agent3Funcs
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Agent3FuncsIntern);
        // 0x00D9ED40: the base constructor only
        Agent3FuncsIntern()
        {
        }

        hkAgent3::Symmetric m_symmetric; // 0x30
    };

    // size 0x3
    struct DebugEntry
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(DebugEntry);

        char m_typeA; // 0x00
        char m_typeB; // 0x01
        char m_priority; // 0x02
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DE9DB0, delete(void* p, unsigned int nbytes) 0x00D9EF30
    hkpCollisionDispatcher(hkpCollisionAgent* (*defaultCollisionAgent)(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpContactMgr*), hkpContactMgrFactory* defaultContactMgrFactory); // 0x00D9E5E0
    virtual ~hkpCollisionDispatcher(); // vtbl+0x00 0x00D9E720
    void registerAlternateShapeType(hkcdShapeType::ShapeTypeEnum primaryType, hkcdShapeType::ShapeTypeEnum alternateType); // 0x00D9E510
    // Inline (hkpcollisiondispatcher.inl of the 2014.1 SDK): havok_terrain_height_internal reads the table directly.
    unsigned int hasAlternateType(hkcdShapeType::ShapeTypeEnum type, hkcdShapeType::ShapeTypeEnum alternateType)
    {
        return m_hasAlternateType[type] & (1u << alternateType);
    }
    void registerCollisionAgent(AgentFuncs& f, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB); // 0x00D9DE20
    void registerCollisionAgent2(AgentFuncs& f, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB); // 0x00D9DEF0
    void setEnableChecks(hkBool checkEnabled); // 0x00D9D260
    void debugPrintTable(); // 0x00D9E810
    void disableDebugging(); // 0x00D9DD70
    // ---- inline (hkpcollisiondispatcher.inl of the 2014.1 SDK; emitted out of line at the addresses given)
    // 0x00D78820
    hkpCollisionAgent* getNewCollisionAgent(const hkpCdBody& collA, const hkpCdBody& collB, const hkpCollisionInput& environment, hkpContactMgr* mgr) const;
    // 0x00D78350
    CreateFunc getCollisionAgentCreationFunction(hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB, IsAgentPredictive predictive) const
    {
        int idx = (predictive ? m_agent2TypesPred : m_agent2Types)[typeA][typeB];
        return m_agent2Func[idx].m_createFunc;
    }
    GetPenetrationsFunc getGetPenetrationsFunc(hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB) const
    {
        int idx = m_agent2Types[typeA][typeB];
        return m_agent2Func[idx].m_getPenetrationsFunc;
    }
    // 0x00D78390
    GetClosestPointsFunc getGetClosestPointsFunc(hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB) const
    {
        int idx = m_agent2Types[typeA][typeB];
        return m_agent2Func[idx].m_getClosestPointFunc;
    }
    LinearCastFunc getLinearCastFunc(hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB) const
    {
        int idx = m_agent2Types[typeA][typeB];
        return m_agent2Func[idx].m_linearCastFunc;
    }
    // 0x00DDCBB0
    hkBool getIsFlipped(hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB) const
    {
        int idx = m_agent2Types[typeA][typeB];
        return m_agent2Func[idx].m_isFlipped;
    }
    int registerAgent3(Agent3Funcs& f, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB); // 0x00D9DF90
    // 0x00DCF5E0
    int getAgent3Type(hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB, unsigned int predictive) const
    {
        if (predictive)
        {
            return m_agent3TypesPred[typeA][typeB];
        }
        return m_agent3Types[typeA][typeB];
    }
    hkAgent3::CreateFunc getAgent3CreateFunc(int type) { return m_agent3Func[type].m_createFunc; } // 0x00DCF540
    hkAgent3::DestroyFunc getAgent3DestroyFunc(int type) { return m_agent3Func[type].m_destroyFunc; } // 0x00DCB6B0
    hkAgent3::CleanupFunc getAgent3CleanupFunc(int type) { return m_agent3Func[type].m_cleanupFunc; } // 0x00DCF560
    hkAgent3::RemovePointFunc getAgent3RemovePointFunc(int type) { return m_agent3Func[type].m_removePointFunc; } // 0x00DCB6D0
    hkAgent3::CommitPotentialFunc getAgent3CommitPotentialFunc(int type) { return m_agent3Func[type].m_commitPotentialFunc; } // 0x00DCB6F0
    hkAgent3::CreateZombieFunc getAgent3CreateZombieFunc(int type) { return m_agent3Func[type].m_createZombieFunc; } // 0x00DCB710
    hkAgent3::UpdateFilterFunc getAgent3UpdateFilterFunc(int type) const { return m_agent3Func[type].m_updateFilterFunc; } // 0x00DCF620
    hkAgent3::InvalidateTimFunc getAgent3InvalidateTimFunc(int type) { return m_agent3Func[type].m_invalidateTimFunc; } // 0x00DCB730
    hkAgent3::WarpTimeFunc getAgent3WarpTimeFunc(int type) { return m_agent3Func[type].m_warpTimeFunc; } // 0x00DCB750
    hkAgent3::ProcessFunc getAgent3ProcessFunc(int type) { return m_agent3Func[type].m_processFunc; } // 0x00DCF580
    hkAgent3::SepNormalFunc getAgent3SepNormalFunc(int type) { return m_agent3Func[type].m_sepNormalFunc; } // 0x00DCF5A0
    hkAgent3::Symmetric getAgent3Symmetric(int type) { return m_agent3Func[type].m_symmetric; } // 0x00DCF5C0
    void registerContactMgrFactory(hkpContactMgrFactory* fac, int responseA, int responseB); // 0x00D9D570
    // 0x00E1DD60
    hkpContactMgrFactory* getContactMgrFactory(int responseA, int responseB) const
    {
        return m_contactMgrFactory[responseA][responseB];
    }
    void registerContactMgrFactoryWithAll(hkpContactMgrFactory* fac, int responseA); // 0x00D9D610
    // 0x00DDCBE0
    signed char getCollisionQualityIndex(hkpCollidableQualityType a, hkpCollidableQualityType b)
    {
        return m_collisionQualityTable[a][b];
    }
    // 0x00DDCC00
    hkpCollisionQualityInfo* getCollisionQualityInfo(signed char index)
    {
        return &m_collisionQualityInfo[index];
    }
    void initCollisionQualityInfo(InitCollisionQualityInfo& input); // 0x00D9D740
    void getClosestPoints(const hkpShape* shapeA, const hkTransformf& transformA, const hkpShape* shapeB, const hkTransformf& transformB, float collisionTolerance, hkpCdPointCollector& collector); // 0x00D9E130
    void getPenetrations(const hkpShape* shapeA, const hkTransformf& transformA, const hkpShape* shapeB, const hkTransformf& transformB, float collisionTolerance, hkpCdBodyPairCollector& collector); // 0x00D9E320
public:
    void internalRegisterCollisionAgent(unsigned char (*agentTypesTable)[35], int agentType, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB, hkcdShapeType::ShapeTypeEnum origA, hkcdShapeType::ShapeTypeEnum origB, DebugEntry (*debugTable)[35][35], int depth); // 0x00D9D2A0
    void updateHasAlternateType(hkcdShapeType::ShapeTypeEnum primaryType, hkcdShapeType::ShapeTypeEnum alternateType, int depth); // 0x00D9D510
    void resetCreationFunctions(); // 0x00D9D110

public:
    hkpCollisionAgent* (*m_defaultCollisionAgent)(const hkpCdBody&, const hkpCdBody&, const hkpCollisionInput&, hkpContactMgr*); // 0x08
    hkpContactMgrFactory* m_contactMgrFactory[8][8]; // 0x0C
    alignas(16) unsigned int m_hasAlternateType[35]; // 0x110
    int m_numAgent2Types; // 0x19C
    unsigned char m_agent2Types[35][35]; // 0x1A0
    unsigned char m_agent2TypesPred[35][35]; // 0x669
    AgentFuncs m_agent2Func[64]; // 0xB34
    int m_numAgent3Types; // 0x1034
    alignas(16) unsigned char m_agent3Types[35][35]; // 0x1040
    alignas(16) unsigned char m_agent3TypesPred[35][35]; // 0x1510
    Agent3FuncsIntern m_agent3Func[18]; // 0x19E0
    signed char m_collisionQualityTable[10][10]; // 0x1E60
    hkpCollisionQualityInfo m_collisionQualityInfo[8]; // 0x1ED0
    hkBool m_collisionAgentRegistered; // 0x20D0
    hkBool m_agent3Registered; // 0x20D1
    hkBool m_midphaseAgent3Registered; // 0x20D2
    hkBool m_checkEnabled; // 0x20D3
    hkArray<ShapeInheritance,hkContainerHeapAllocator> m_shapeInheritance; // 0x20D4
    DebugEntry (*m_debugAgent2Table)[35][35]; // 0x20E0
    DebugEntry (*m_debugAgent2TablePred)[35][35]; // 0x20E4
    DebugEntry (*m_debugAgent3Table)[35][35]; // 0x20E8
    DebugEntry (*m_debugAgent3TablePred)[35][35]; // 0x20EC
    float m_expectedMaxLinearVelocity; // 0x20F0
    float m_expectedMinPsiDeltaTime; // 0x20F4
};
