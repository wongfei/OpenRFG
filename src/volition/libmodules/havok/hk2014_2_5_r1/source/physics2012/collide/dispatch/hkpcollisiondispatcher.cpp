// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the collision dispatcher, from rfg.exe. It maps every pair of shape types to an
// agent (old style agent2 creation and static query functions, and agent3 stream functions, each with a predictive
// variant), keeps the contact manager factories per response type pair and the collision quality table.
//
// The debug tables (m_debugAgent2Table, ...) are never allocated in the shipped build: the dispatcher only frees and
// fills them when they exist.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/null/hkpnullagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/agent3bridge/hkpagent3bridge.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpcontactmgrfactory.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

// Size of one debug table: 35 x 35 entries of 3 bytes.
static const int HK_DEBUG_TABLE_SIZE = sizeof(hkpCollisionDispatcher::DebugTable); // 0xE5B

// 0x00D9D0E0: no agent data.
void* hkNullAgent3::create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* freeMemory)
{
    entry->m_streamCommand = hkAgent3::STREAM_NULL;
    return freeMemory;
}

// 0x00D9D0F0
void hkNullAgent3::destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher)
{
}

// 0x00D9D100
void* hkNullAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormalOut, hkpProcessCollisionOutput& result)
{
    return agentData;
}

// 0x00D9E5E0
// Every pair starts at the null agents (index 0 of both function tables; the agent3 bridge is index 1) and every
// response type pair at the default contact manager factory (one reference per slot).
hkpCollisionDispatcher::hkpCollisionDispatcher(CreateFunc defaultCollisionAgent, hkpContactMgrFactory* defaultContactMgrFactory)
{
    m_defaultCollisionAgent = defaultCollisionAgent;
    m_debugAgent2Table = nullptr;
    m_debugAgent2TablePred = nullptr;
    m_debugAgent3Table = nullptr;
    m_debugAgent3TablePred = nullptr;
    m_collisionAgentRegistered = false;
    m_checkEnabled = true;
    m_numAgent3Types = 0;
    m_midphaseAgent3Registered = false;
    for (int i = 0; i < HK_MAX_RESPONSE_TYPE; i++)
    {
        for (int j = 0; j < HK_MAX_RESPONSE_TYPE; j++)
        {
            m_contactMgrFactory[i][j] = defaultContactMgrFactory;
            if (defaultContactMgrFactory != nullptr)
            {
                defaultContactMgrFactory->addReference();
            }
        }
    }
    // Each type is its own alternate type: bit (type mod 32), rotated in.
    unsigned int bit = 1;
    for (int i = 0; i < HK_MAX_SHAPE_TYPE; i++)
    {
        m_hasAlternateType[i] = bit;
        bit = (bit << 1) | (bit >> 31);
    }
    resetCreationFunctions();
    m_agent3Registered = false;
}

// 0x00D9E720
hkpCollisionDispatcher::~hkpCollisionDispatcher()
{
    disableDebugging();
    for (int i = 0; i < HK_MAX_RESPONSE_TYPE; i++)
    {
        for (int j = 0; j < HK_MAX_RESPONSE_TYPE; j++)
        {
            if (m_contactMgrFactory[i][j] != nullptr)
            {
                m_contactMgrFactory[i][j]->removeReference();
            }
        }
    }
}

// 0x00D9D110
// Clears the type tables (both agent2 and agent3, predictive or not) and installs the null agents at index 0 and the
// agent3 bridge (which runs the agent2 agents in agent3 streams) at index 1.
void hkpCollisionDispatcher::resetCreationFunctions()
{
    m_numAgent2Types = 1;
    m_numAgent3Types = 1;
    for (int a = 0; a < HK_MAX_SHAPE_TYPE; a++)
    {
        for (int b = 0; b < HK_MAX_SHAPE_TYPE; b++)
        {
            if (m_debugAgent2Table != nullptr)
            {
                (*m_debugAgent2Table)[a][b].m_priority = 100;
                (*m_debugAgent2TablePred)[a][b].m_priority = 100;
                (*m_debugAgent3Table)[a][b].m_priority = 100;
                (*m_debugAgent3TablePred)[a][b].m_priority = 100;
            }
            m_agent2Types[a][b] = 0;
            m_agent3Types[a][b] = 0;
            m_agent2TypesPred[a][b] = 0;
            m_agent3TypesPred[a][b] = 0;
        }
    }
    AgentFuncs& f2 = m_agent2Func[0];
    f2.m_createFunc = hkpNullAgent::createNullAgent;
    f2.m_getPenetrationsFunc = hkpNullAgent::staticGetPenetrations;
    f2.m_getClosestPointFunc = hkpNullAgent::staticGetClosestPoints;
    f2.m_linearCastFunc = hkpNullAgent::staticLinearCast;
    f2.m_isFlipped = false;
    f2.m_isPredictive = true;

    Agent3FuncsIntern& f3 = m_agent3Func[0];
    f3.m_createFunc = hkNullAgent3::create;
    f3.m_destroyFunc = hkNullAgent3::destroy;
    f3.m_cleanupFunc = nullptr;
    f3.m_removePointFunc = nullptr;
    f3.m_commitPotentialFunc = nullptr;
    f3.m_createZombieFunc = nullptr;
    f3.m_updateFilterFunc = nullptr;
    f3.m_sepNormalFunc = nullptr;
    f3.m_invalidateTimFunc = nullptr;
    f3.m_warpTimeFunc = nullptr;
    f3.m_processFunc = hkNullAgent3::process;
    f3.m_symmetric = hkAgent3::IS_SYMMETRIC;

    hkAgent3Bridge::registerAgent3(this);
    m_collisionAgentRegistered = false;
    if (m_debugAgent3Table != nullptr)
    {
        for (int a = 0; a < HK_MAX_SHAPE_TYPE; a++)
        {
            for (int b = 0; b < HK_MAX_SHAPE_TYPE; b++)
            {
                (*m_debugAgent3Table)[a][b].m_priority = 100;
                (*m_debugAgent3TablePred)[a][b].m_priority = 100;
            }
        }
    }
}

// 0x00D9D260
void hkpCollisionDispatcher::setEnableChecks(hkBool checkEnabled)
{
    m_checkEnabled = checkEnabled;
}

// 0x00D9D2A0
// Writes agentType into every (typeA, typeB) cell of the table, including the cells of the shape types whose
// alternate type is typeA or typeB (recursively, one depth level per step). ALL_SHAPE_TYPES covers every type.
void hkpCollisionDispatcher::internalRegisterCollisionAgent(unsigned char (*agentTypesTable)[35], int agentType, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB, hkcdShapeType::ShapeTypeEnum origA, hkcdShapeType::ShapeTypeEnum origB, DebugEntry (*debugTable)[35][35], int depth)
{
    m_collisionAgentRegistered = true;
    for (int i = 0; i < m_shapeInheritance.getSize(); i++)
    {
        ShapeInheritance& si = m_shapeInheritance[i];
        if (si.m_alternateType == typeA)
        {
            internalRegisterCollisionAgent(agentTypesTable, agentType, si.m_primaryType, typeB, origA, origB, debugTable, depth + 1);
        }
        if (si.m_alternateType == typeB)
        {
            internalRegisterCollisionAgent(agentTypesTable, agentType, typeA, si.m_primaryType, origA, origB, debugTable, depth + 1);
        }
    }

    int beginA = typeA;
    int endA = typeA + 1;
    int beginB = typeB;
    int endB = typeB + 1;
    int depthA = depth;
    if (typeA == hkcdShapeType::ALL_SHAPE_TYPES)
    {
        beginA = hkcdShapeType::FIRST_SHAPE_TYPE;
        endA = HK_MAX_SHAPE_TYPE;
        depthA = depth + 1;
    }
    int priority = depthA;
    if (typeB == hkcdShapeType::ALL_SHAPE_TYPES)
    {
        beginB = hkcdShapeType::FIRST_SHAPE_TYPE;
        endB = HK_MAX_SHAPE_TYPE;
        priority = depthA + 1;
    }
    for (int a = beginA; a < endA; a++)
    {
        for (int b = beginB; b < endB; b++)
        {
            agentTypesTable[a][b] = (unsigned char)agentType;
            if (debugTable != nullptr)
            {
                DebugEntry& e = (*debugTable)[a][b];
                // m_checkEnabled && e.m_priority < priority: "Agent handling types <%s-%s> would override more
                // specialized agent <%s-%s>" (the warning text is formatted, then dropped: HK_WARN is compiled out)
                e.m_typeA = (char)origA;
                e.m_priority = (char)priority;
                e.m_typeB = (char)origB;
            }
        }
    }
}

// 0x00D9D510
// The alternate types of alternateType become alternate types of primaryType and of every type whose alternate type is
// primaryType (recursively).
void hkpCollisionDispatcher::updateHasAlternateType(hkcdShapeType::ShapeTypeEnum primaryType, hkcdShapeType::ShapeTypeEnum alternateType, int depth)
{
    m_hasAlternateType[primaryType] |= m_hasAlternateType[alternateType];
    for (int i = 0; i < m_shapeInheritance.getSize(); i++)
    {
        ShapeInheritance& si = m_shapeInheritance[i];
        if (si.m_alternateType == primaryType)
        {
            updateHasAlternateType(si.m_primaryType, si.m_alternateType, depth + 1);
        }
    }
}

// 0x00D9D570
void hkpCollisionDispatcher::registerContactMgrFactory(hkpContactMgrFactory* fac, int responseA, int responseB)
{
    fac->addReference();
    m_contactMgrFactory[responseB][responseA]->removeReference();
    m_contactMgrFactory[responseB][responseA] = fac;
    fac->addReference();
    m_contactMgrFactory[responseA][responseB]->removeReference();
    m_contactMgrFactory[responseA][responseB] = fac;
}

// 0x00D9D610
void hkpCollisionDispatcher::registerContactMgrFactoryWithAll(hkpContactMgrFactory* fac, int responseA)
{
    for (int i = 0; i < HK_MAX_RESPONSE_TYPE; i++)
    {
        fac->addReference();
        m_contactMgrFactory[i][responseA]->removeReference();
        m_contactMgrFactory[i][responseA] = fac;
        fac->addReference();
        m_contactMgrFactory[responseA][i]->removeReference();
        m_contactMgrFactory[responseA][i] = fac;
    }
}

// 0x00D9D740
// The collision qualities: PSI (1, also the base of the others), simplified TOI (2), TOI (3), TOI higher (4), TOI forced
// (5), character (6, from TOI forced) and the temporary expand-manifold quality (7, from PSI).
void hkpCollisionDispatcher::initCollisionQualityInfo(InitCollisionQualityInfo& input)
{
    m_expectedMinPsiDeltaTime = input.m_minDeltaTime;
    m_expectedMaxLinearVelocity = input.m_maxLinearVelocity;
    // the distance gravity moves a body in one step
    float gravityDistance = input.m_minDeltaTime * input.m_gravityLength * input.m_minDeltaTime * 0.5f;

    hkpCollisionQualityInfo& psi = m_collisionQualityInfo[COLLISION_QUALITY_PSI];
    psi.m_enableToiWeldRejection = input.m_enableToiWeldRejection;
    psi.m_keepContact = input.m_collisionTolerance;
    psi.m_create4dContact = input.m_collisionTolerance;
    psi.m_manifoldTimDistance = input.m_collisionTolerance;
    if (input.m_enableNegativeManifoldTims)
    {
        psi.m_manifoldTimDistance = gravityDistance * -2.0f;
    }
    psi.m_createContact = input.m_collisionTolerance;
    if (input.m_enableNegativeToleranceToCreateNon4dContacts)
    {
        psi.m_createContact = gravityDistance * -1.0f;
    }
    psi.m_maxContraintViolation = HK_REAL_MAX;
    psi.m_useContinuousPhysics = 0;
    psi.m_useSimpleToiHandling = false;
    psi.m_constraintPriority = input.m_defaultConstraintPriority;
    psi.m_minSeparation = -5e19f; // 0xE02D78EC: -HK_REAL_HIGH * 0.5
    psi.m_minExtraSeparation = -5e19f;
    psi.m_toiSeparation = -1e19f; // 0xDF0AC723: -HK_REAL_HIGH * 0.1
    psi.m_toiExtraSeparation = -1e19f;
    {
        // |-HK_REAL_HIGH * 0.05| (0xE0AD78EC * 0x3D4CCCCD, sign cleared)
        float accuracy = -1e20f;
        accuracy *= 0.05f;
        psi.m_toiAccuracy = hkMath::fabs(accuracy);
    }
    psi.m_minSafeDeltaTime = 1.0f;
    psi.m_minAbsoluteSafeDeltaTime = 1.0f;
    psi.m_minToiDeltaTime = 1.0f;

    m_collisionQualityInfo[COLLISION_QUALITY_TMP_EXPAND_MANIFOLD] = psi;
    m_collisionQualityInfo[COLLISION_QUALITY_SIMPLIFIED_TOI] = psi;
    m_collisionQualityInfo[COLLISION_QUALITY_TOI] = psi;
    m_collisionQualityInfo[COLLISION_QUALITY_TOI_HIGHER] = psi;
    m_collisionQualityInfo[COLLISION_QUALITY_TOI_FORCED] = psi;

    hkpCollisionQualityInfo& expand = m_collisionQualityInfo[COLLISION_QUALITY_TMP_EXPAND_MANIFOLD];
    expand.m_manifoldTimDistance = input.m_collisionTolerance;
    expand.m_create4dContact = input.m_collisionTolerance;
    expand.m_createContact = input.m_collisionTolerance;

    if (input.m_wantContinuousCollisionDetection)
    {
        hkpCollisionQualityInfo& q = m_collisionQualityInfo[COLLISION_QUALITY_SIMPLIFIED_TOI];
        q.m_minSeparation = -0.5f;
        q.m_minExtraSeparation = -0.5f / (input.m_numToisTillAllowedPenetrationSimplifiedToi - 1.0f);
        q.m_toiSeparation = -0.25f;
        q.m_toiExtraSeparation = -0.35f / (input.m_numToisTillAllowedPenetrationSimplifiedToi - 1.0f);
        q.m_toiAccuracy = q.m_toiExtraSeparation * -0.3f;
        q.m_maxContraintViolation = q.m_minExtraSeparation * -1.0f;
        q.m_minSafeDeltaTime = 5.0f / input.m_maxLinearVelocity;
        q.m_minAbsoluteSafeDeltaTime = 0.005f / input.m_maxLinearVelocity;
        q.m_minToiDeltaTime = (q.m_minExtraSeparation * -2.0f) / input.m_maxLinearVelocity;
        q.m_constraintPriority = input.m_toiConstraintPriority;
        q.m_useContinuousPhysics = 1;
        q.m_useSimpleToiHandling = true;
    }
    if (input.m_wantContinuousCollisionDetection)
    {
        hkpCollisionQualityInfo& q = m_collisionQualityInfo[COLLISION_QUALITY_TOI];
        q.m_minSeparation = -0.5f;
        q.m_minExtraSeparation = -0.5f / (input.m_numToisTillAllowedPenetrationToi - 1.0f);
        q.m_toiSeparation = -0.25f;
        q.m_toiExtraSeparation = -0.35f / (input.m_numToisTillAllowedPenetrationToi - 1.0f);
        q.m_toiAccuracy = q.m_toiExtraSeparation * -0.3f;
        q.m_maxContraintViolation = q.m_minExtraSeparation * -1.0f;
        q.m_minSafeDeltaTime = 5.0f / input.m_maxLinearVelocity;
        q.m_minAbsoluteSafeDeltaTime = 0.005f / input.m_maxLinearVelocity;
        q.m_minToiDeltaTime = (q.m_minExtraSeparation * -2.0f) / input.m_maxLinearVelocity;
        q.m_constraintPriority = input.m_toiConstraintPriority;
        q.m_useContinuousPhysics = 1;
        // m_useSimpleToiHandling stays false (from PSI)

        if (input.m_wantContinuousCollisionDetection)
        {
            hkpCollisionQualityInfo& h = m_collisionQualityInfo[COLLISION_QUALITY_TOI_HIGHER];
            h.m_minSeparation = -0.5f;
            h.m_minExtraSeparation = -0.5f / (input.m_numToisTillAllowedPenetrationToiHigher - 1.0f);
            h.m_toiSeparation = -0.25f;
            h.m_toiExtraSeparation = -0.35f / (input.m_numToisTillAllowedPenetrationToiHigher - 1.0f);
            h.m_toiAccuracy = h.m_toiExtraSeparation * -0.3f;
            h.m_maxContraintViolation = h.m_minExtraSeparation * -1.0f;
            h.m_minSafeDeltaTime = 5.0f / input.m_maxLinearVelocity;
            h.m_minAbsoluteSafeDeltaTime = 0.005f / input.m_maxLinearVelocity;
            // unlike the other TOI qualities: the allowed violation, not twice the extra separation
            h.m_minToiDeltaTime = h.m_maxContraintViolation / input.m_maxLinearVelocity;
            h.m_constraintPriority = input.m_toiHigherConstraintPriority;
            h.m_useContinuousPhysics = 1;
        }
    }

    hkpCollisionQualityInfo& forced = m_collisionQualityInfo[COLLISION_QUALITY_TOI_FORCED];
    forced.m_keepContact = input.m_collisionTolerance;
    if (input.m_wantContinuousCollisionDetection)
    {
        forced.m_minSeparation = -0.4f;
        forced.m_minExtraSeparation = -0.6f / (input.m_numToisTillAllowedPenetrationToiForced - 1.0f);
        forced.m_toiSeparation = -0.2f;
        forced.m_toiExtraSeparation = -0.42f / (input.m_numToisTillAllowedPenetrationToiForced - 1.0f);
        forced.m_toiAccuracy = forced.m_toiExtraSeparation * -0.3f;
        forced.m_maxContraintViolation = forced.m_minExtraSeparation * -1.0f;
        forced.m_minSafeDeltaTime = 5.0f / input.m_maxLinearVelocity;
        forced.m_minAbsoluteSafeDeltaTime = 0.005f / input.m_maxLinearVelocity;
        forced.m_minToiDeltaTime = forced.m_maxContraintViolation / input.m_maxLinearVelocity;
        forced.m_constraintPriority = input.m_toiForcedConstraintPriority;
        forced.m_useContinuousPhysics = 1;
    }

    hkpCollisionQualityInfo& character = m_collisionQualityInfo[COLLISION_QUALITY_CHARACTER];
    character = forced;
    character.m_manifoldTimDistance = input.m_collisionTolerance;
    character.m_createContact = 0.001f;
    character.m_create4dContact = 0.01f;

    // Rows and columns are hkpCollidableQualityType + 1 (row 0: HK_COLLIDABLE_QUALITY_INVALID); the cells are
    // CollisionQualityLevel values (0 invalid, 1 PSI, 2 simplified TOI, 3 TOI, 4 TOI higher, 5 TOI forced, 6 character).
    static const signed char table[10][10] = {
        {0, 0, 1, 2, 4, 5, 4, 1, 6, 3},
        {0, 0, 1, 1, 3, 3, 3, 1, 3, 3},
        {1, 1, 1, 1, 1, 1, 3, 1, 1, 1},
        {2, 1, 1, 1, 1, 1, 3, 1, 1, 1},
        {4, 3, 1, 1, 1, 3, 3, 1, 3, 3},
        {5, 3, 1, 1, 3, 3, 3, 1, 3, 3},
        {4, 3, 3, 3, 3, 3, 3, 1, 3, 3},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {6, 3, 1, 1, 3, 3, 3, 1, 3, 3},
        {3, 3, 1, 1, 3, 3, 3, 1, 3, 3},
    };
    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            m_collisionQualityTable[i][j] = table[i][j];
        }
    }
}

// 0x00D9DD70
void hkpCollisionDispatcher::disableDebugging()
{
    if (m_debugAgent2Table != nullptr)
    {
        hkMemoryRouter& router = hkMemoryRouter::getInstance();
        router.heap().blockFree(m_debugAgent2Table, HK_DEBUG_TABLE_SIZE);
        router.heap().blockFree(m_debugAgent2TablePred, HK_DEBUG_TABLE_SIZE);
        router.heap().blockFree(m_debugAgent3Table, HK_DEBUG_TABLE_SIZE);
        router.heap().blockFree(m_debugAgent3TablePred, HK_DEBUG_TABLE_SIZE);
        m_debugAgent2Table = nullptr;
        m_debugAgent2TablePred = nullptr;
        m_debugAgent3Table = nullptr;
        m_debugAgent3TablePred = nullptr;
    }
}

// 0x00D9DE20
// An agent2 agent; it also becomes the agent3 of the pair through the bridge (agent3 type 1).
void hkpCollisionDispatcher::registerCollisionAgent(AgentFuncs& f, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB)
{
    m_agent2Func[m_numAgent2Types] = f;
    internalRegisterCollisionAgent(m_agent3Types, 1, typeA, typeB, typeA, typeB, m_debugAgent3Table, 0);
    internalRegisterCollisionAgent(m_agent2Types, m_numAgent2Types, typeA, typeB, typeA, typeB, m_debugAgent2Table, 0);
    if (f.m_isPredictive)
    {
        internalRegisterCollisionAgent(m_agent3TypesPred, 1, typeA, typeB, typeA, typeB, m_debugAgent3TablePred, 0);
        internalRegisterCollisionAgent(m_agent2TypesPred, m_numAgent2Types, typeA, typeB, typeA, typeB, m_debugAgent2TablePred, 0);
    }
    m_numAgent2Types++;
}

// 0x00D9DEF0
// An agent2 agent only (the agent3 tables keep their agent).
void hkpCollisionDispatcher::registerCollisionAgent2(AgentFuncs& f, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB)
{
    m_agent2Func[m_numAgent2Types] = f;
    internalRegisterCollisionAgent(m_agent2Types, m_numAgent2Types, typeA, typeB, typeA, typeB, m_debugAgent2Table, 0);
    if (f.m_isPredictive)
    {
        internalRegisterCollisionAgent(m_agent2TypesPred, m_numAgent2Types, typeA, typeB, typeA, typeB, m_debugAgent2TablePred, 0);
    }
    m_numAgent2Types++;
}

// 0x00D9DF90
// An agent3 agent. For two different types it also registers the symmetric entry (B, A) first, flagged
// IS_NOT_SYMMETRIC_AND_FLIPPED, unless the agent asks not to; the entry for (A, B) is flagged IS_NOT_SYMMETRIC then.
// m_reusePreviousEntry registers (A, B) under the last entry instead of a new one. Returns the agent type of (A, B).
int hkpCollisionDispatcher::registerAgent3(Agent3Funcs& f, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB)
{
    m_agent3Registered = true;
    Agent3FuncsIntern intern;
    static_cast<Agent3Funcs&>(intern) = f;
    intern.m_symmetric = hkAgent3::IS_SYMMETRIC;
    if (typeA != typeB && !f.m_ignoreSymmetricVersion)
    {
        intern.m_symmetric = hkAgent3::IS_NOT_SYMMETRIC_AND_FLIPPED;
        m_agent3Func[m_numAgent3Types] = intern;
        internalRegisterCollisionAgent(m_agent3Types, m_numAgent3Types, typeB, typeA, typeB, typeA, m_debugAgent3Table, 0);
        if (intern.m_isPredictive)
        {
            internalRegisterCollisionAgent(m_agent3TypesPred, m_numAgent3Types, typeB, typeA, typeB, typeA, m_debugAgent3TablePred, 0);
        }
        m_numAgent3Types++;
        intern.m_symmetric = hkAgent3::IS_NOT_SYMMETRIC;
    }
    int agentType = m_numAgent3Types;
    if (!f.m_reusePreviousEntry)
    {
        m_numAgent3Types = agentType + 1;
        m_agent3Func[agentType] = intern;
    }
    else
    {
        agentType--;
    }
    internalRegisterCollisionAgent(m_agent3Types, agentType, typeA, typeB, typeA, typeB, m_debugAgent3Table, 0);
    if (intern.m_isPredictive)
    {
        internalRegisterCollisionAgent(m_agent3TypesPred, agentType, typeA, typeB, typeA, typeB, m_debugAgent3TablePred, 0);
    }
    return agentType;
}

// 0x00D9E510
// Re-registering a pair moves it to the end of the inheritance list.
void hkpCollisionDispatcher::registerAlternateShapeType(hkcdShapeType::ShapeTypeEnum primaryType, hkcdShapeType::ShapeTypeEnum alternateType)
{
    for (int i = 0; i < m_shapeInheritance.getSize(); i++)
    {
        if (m_shapeInheritance[i].m_primaryType == primaryType && m_shapeInheritance[i].m_alternateType == alternateType)
        {
            m_shapeInheritance.removeAtAndCopy(i);
            i--;
        }
    }
    updateHasAlternateType(primaryType, alternateType, 0);
    ShapeInheritance& si = m_shapeInheritance.expandOne();
    si.m_primaryType = primaryType;
    si.m_alternateType = alternateType;
}

// 0x00D78820 (hkpcollisiondispatcher.inl)
hkpCollisionAgent* hkpCollisionDispatcher::getNewCollisionAgent(const hkpCdBody& collA, const hkpCdBody& collB, const hkpCollisionInput& environment, hkpContactMgr* mgr) const
{
    hkcdShapeType::ShapeTypeEnum typeA = (hkcdShapeType::ShapeTypeEnum)collA.getShape()->m_type.m_storage;
    hkcdShapeType::ShapeTypeEnum typeB = (hkcdShapeType::ShapeTypeEnum)collB.getShape()->m_type.m_storage;
    IsAgentPredictive predictive = (IsAgentPredictive)(int)environment.m_createPredictiveAgents.val();
    CreateFunc f = getCollisionAgentCreationFunction(typeA, typeB, predictive);
    return f(collA, collB, environment, mgr);
}
