// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/convexlist/hkpconvexlistutils.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); the inline code (shared by hkpConvexListAgent and
// hkConvexListAgent3) ported from the 2014.1 SDK header and checked against rfg.exe (stub_impl.md S4). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/deprecated/convexlist/hkpconvexlistshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"

class hkContactPoint;
class hkpCollidable;
class hkpConstraintOwner;
class hkpContactPointProperties;
struct hkpProcessCollisionData;
struct hkpProcessCollisionInput;

// Not in rfg.pdb (inlined everywhere): the output state a convex list agent restores when it drops the points of a
// try (the first free point, the TOI and the welding information).
struct hkpProcessCollisionOutputBackup
{
    hkpProcessCollisionOutputBackup(const hkpProcessCollisionOutput& output)
        : m_firstPoint(output.m_firstFreeContactPoint), m_toi(output.m_toi)
    {
        if (output.m_potentialContacts.val() != nullptr)
        {
            m_weldingInformation = *output.m_potentialContacts.val();
        }
    }

    // A TOI added by the try is removed from the contact manager first.
    void rollbackOutput(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpProcessCollisionOutput& output, hkpContactMgr* mgr)
    {
        if (!(output.m_toi.m_time.val() == m_toi.m_time.val()))
        {
            mgr->removeToi(*output.m_constraintOwner.val(), output.m_toi.m_properties);
            output.m_toi = m_toi;
        }
        output.m_firstFreeContactPoint = m_firstPoint;
        if (output.m_potentialContacts.val() != nullptr)
        {
            *output.m_potentialContacts.val() = m_weldingInformation;
        }
    }

    hkpProcessCdPoint* m_firstPoint;
    hkpProcessCollisionOutput::PotentialInfo m_weldingInformation;
    hkpProcessCollisionOutput::ToiInfo m_toi;
};

// size 0x14
// Forwards to the agent's contact manager with the convex list bodies replaced by the child the GSK features are on;
// a point whose features span two children is refused (m_invalidPointHit).
class hkpMapPointsToSubShapeContactMgr : public hkpContactMgr
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00DB58C0

    // 0x00DB5800
    hkpMapPointsToSubShapeContactMgr(hkpContactMgr* mgr)
        : hkpContactMgr(hkpContactMgr::TYPE_CONVEX_LIST_CONTACT_MGR)
    {
        m_contactMgr = mgr;
        m_invalidPointHit = false;
    }

    // vtbl+0x00 0x00DB5950: implicit

    // vtbl+0x10 0x00DB5AB0
    virtual unsigned short addContactPointImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, const hkpGskCache* contactCache, hkContactPoint& cp)
    {
        const hkpCdBody* aMod = &a;
        const hkpCdBody* bMod = &b;
        // Original behaviour kept: one child shape buffer serves both bodies (the SDK's two block-scoped buffers share
        // the stack slot); a convex list returns its stored children, so the buffer is never written.
        hkpShapeBuffer buffer;
        hkpCdBody aTemp(&a);
        hkpCdBody bTemp(&b);
        if (a.getShape()->m_type.m_storage == hkcdShapeType::CONVEX_LIST)
        {
            // the child index is the high byte of a vertex id
            const int shapeKey = contactCache->m_vertices[0] >> 8;
            for (int i = 1; i < contactCache->m_dimA; i++)
            {
                if ((contactCache->m_vertices[i] >> 8) != shapeKey)
                {
                    m_invalidPointHit = true;
                    return 0xFFFF; // HK_INVALID_CONTACT_POINT
                }
            }
            const hkpShape* childShape = static_cast<const hkpConvexListShape*>(a.getShape())->getChildShape(shapeKey, buffer);
            aTemp.setShape(childShape, shapeKey);
            aMod = &aTemp;
        }
        if (b.getShape()->m_type.m_storage == hkcdShapeType::CONVEX_LIST)
        {
            const int shapeKey = contactCache->m_vertices[contactCache->m_dimA] >> 8;
            for (int i = contactCache->m_dimA + 1; i < contactCache->m_dimA + contactCache->m_dimB; i++)
            {
                if ((contactCache->m_vertices[i] >> 8) != shapeKey)
                {
                    m_invalidPointHit = true;
                    return 0xFFFF; // HK_INVALID_CONTACT_POINT
                }
            }
            const hkpShape* childShape = static_cast<const hkpConvexListShape*>(b.getShape())->getChildShape(shapeKey, buffer);
            bTemp.setShape(childShape, shapeKey);
            bMod = &bTemp;
        }
        return m_contactMgr->addContactPoint(*aMod, *bMod, input, output, contactCache, cp);
    }

    // vtbl+0x14 0x00DB5900
    virtual hkResult reserveContactPointsImpl(int numPoints)
    {
        return m_contactMgr->reserveContactPoints(numPoints);
    }

    // vtbl+0x18 0x00DB5920
    virtual void removeContactPointImpl(unsigned short cpId, hkpConstraintOwner& constraintOwner)
    {
        m_contactMgr->removeContactPoint(cpId, constraintOwner);
    }

    // vtbl+0x1C 0x00DB5930
    virtual void processContactImpl(const hkpCollidable& a, const hkpCollidable& b, const hkpProcessCollisionInput& input, hkpProcessCollisionData& collisionData)
    {
        m_contactMgr->processContact(a, b, input, collisionData);
    }

    // vtbl+0x20 0x00DB5BE0: as addContactPointImpl
    virtual hkpContactMgr::ToiAccept addToiImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, float toi, hkContactPoint& cp, const hkpGskCache* gskCache, float& projectedVelocity, hkpContactPointProperties& propertiesOut)
    {
        const hkpCdBody* aMod = &a;
        const hkpCdBody* bMod = &b;
        hkpShapeBuffer buffer; // shared by both bodies, as above
        hkpCdBody aTemp(&a);
        hkpCdBody bTemp(&b);
        if (a.getShape()->m_type.m_storage == hkcdShapeType::CONVEX_LIST)
        {
            const int shapeKey = gskCache->m_vertices[0] >> 8;
            for (int i = 1; i < gskCache->m_dimA; i++)
            {
                if ((gskCache->m_vertices[i] >> 8) != shapeKey)
                {
                    m_invalidPointHit = true;
                    return TOI_REJECT;
                }
            }
            const hkpShape* childShape = static_cast<const hkpConvexListShape*>(a.getShape())->getChildShape(shapeKey, buffer);
            aTemp.setShape(childShape, shapeKey);
            aMod = &aTemp;
        }
        if (b.getShape()->m_type.m_storage == hkcdShapeType::CONVEX_LIST)
        {
            const int shapeKey = gskCache->m_vertices[gskCache->m_dimA] >> 8;
            for (int i = gskCache->m_dimA + 1; i < gskCache->m_dimA + gskCache->m_dimB; i++)
            {
                if ((gskCache->m_vertices[i] >> 8) != shapeKey)
                {
                    m_invalidPointHit = true;
                    return TOI_REJECT;
                }
            }
            const hkpShape* childShape = static_cast<const hkpConvexListShape*>(b.getShape())->getChildShape(shapeKey, buffer);
            bTemp.setShape(childShape, shapeKey);
            bMod = &bTemp;
        }
        return m_contactMgr->addToi(*aMod, *bMod, input, output, toi, cp, gskCache, projectedVelocity, propertiesOut);
    }

    // vtbl+0x24 0x00DB5940
    virtual void removeToiImpl(hkpConstraintOwner& constraintOwner, hkpContactPointProperties& properties)
    {
        m_contactMgr->removeToi(constraintOwner, properties);
    }

    // vtbl+0x28 0x00DB5830
    virtual void cleanup()
    {
        m_contactMgr->cleanup();
    }

    hkpContactMgr* m_contactMgr; // 0x0C
    hkBool m_invalidPointHit; // 0x10
};
