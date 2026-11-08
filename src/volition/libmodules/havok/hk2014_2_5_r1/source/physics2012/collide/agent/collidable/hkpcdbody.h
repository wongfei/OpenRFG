// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declaration with the inline code of Havok 2014.1 (hkpCdBody.inl,
// release build: m_motion is a plain transform or motion state pointer). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

class hkClass;
struct hkInternalClassMember;
class hkMotionState;
class hkTransformf;
class hkpCollidable;
class hkpShape;

// size 0x10
class hkpCdBody
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCdBody); // new(unsigned int n, void* p) 0x00DB5840
    static const hkClass& staticClass(); // 0x00D7DD30

    // 0x00D751D0
    hkpCdBody(const hkpCdBody* parent, const hkTransformf* t)
    {
        m_parent = parent;
        setTransform(t);
    }

    // 0x00DB9B00
    hkpCdBody(const hkpCdBody* parent, const hkMotionState* ms)
    {
        m_parent = parent;
        setMotionState(ms);
    }

    // 0x00D76040: copies the parent and its motion; the shape and key are set by the caller
    explicit hkpCdBody(const hkpCdBody* parent)
    {
        m_parent = parent;
        m_motion = parent->m_motion;
    }

    hkpCdBody(const hkpCdBody&) {} // private copy constructor in Havok (copies nothing)

    // 0x00D9EC30
    hkpCdBody(const hkpShape* shape, const hkTransformf* t)
    {
        m_shape = shape;
        setTransform(t);
        m_parent = nullptr;
        m_shapeKey = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    }

    // 0x00DB9AE0
    hkpCdBody(const hkpShape* shape, const hkMotionState* motionState)
    {
        m_shape = shape;
        setMotionState(motionState);
        m_parent = nullptr;
        m_shapeKey = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    }

    hkpCdBody() {} // 0x00D7D8A0 (empty)

    // 0x00710F30 (hkpcdbody.inl:10)
    const hkTransformf& getTransform() const
    {
        return *static_cast<const hkTransformf*>(m_motion);
    }

    // 0x00B62ED0 (hkpcdbody.inl:45)
    const hkpShape* getShape() const
    {
        return m_shape;
    }

    // 0x00710F40 (hkpcdbody.inl:50)
    const hkpCollidable* getRootCollidable() const
    {
        const hkpCdBody* body = this;
        while (body->m_parent)
        {
            body = body->m_parent;
        }
        return reinterpret_cast<const hkpCollidable*>(body);
    }

    // 0x00710F60 (hkpcdbody.inl:101)
    unsigned int getShapeKey() const
    {
        return m_shapeKey;
    }

    unsigned int getTopLevelShapeKey() const
    {
        const hkpCdBody* root = this;
        const hkpCdBody* topChild = root;
        while (root->m_parent)
        {
            topChild = root;
            root = root->m_parent;
        }
        return topChild->m_shapeKey;
    }

    // 0x00D83E60
    const hkpCdBody* getParent() const
    {
        return m_parent;
    }

    // 0x00D751F0
    void setShape(const hkpShape* shape, unsigned int key)
    {
        m_shape = shape;
        m_shapeKey = key;
    }

    // 0x00D78330
    const hkMotionState* getMotionState() const
    {
        return static_cast<const hkMotionState*>(m_motion);
    }

    // 0x00DB9AD0
    void setMotionState(const hkMotionState* state)
    {
        m_motion = state;
    }

    // 0x00D751C0
    void setTransform(const hkTransformf* t)
    {
        m_motion = t;
    }

    static const hkInternalClassMember Members[]; // 0x01355C40
    const hkpShape* m_shape; // 0x00
    unsigned int m_shapeKey; // 0x04
public:
    const void* m_motion; // 0x08 (a transform or a motion state)
public:
    const hkpCdBody* m_parent; // 0x0C
};
