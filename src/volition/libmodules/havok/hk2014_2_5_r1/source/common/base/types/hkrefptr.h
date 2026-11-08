// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h
//
// Seam only (Havok 2014.2.5): a reference-counting pointer and the non-owning view pointer.
#pragma once

// size 0x4
// Assigning a pointer references the new object and releases the old one (Havok 2014.1 hkRefPtr.h/.inl); TYPE must
// be complete where it is assigned. Copying (construction and assignment from another hkRefPtr, as
// hkpWorld::getCinfo does with the memory watchdog) references the object too (HK9: the implicit copy took no
// reference, so every havok_get_broadphase_extents released one of the watchdog's and freed it). The default
// constructor stays empty: the classes set or release their members themselves, and the finishing constructors of
// loaded objects must keep the pointers the loader wrote (Havok has hkRefPtr(hkFinishLoadedObjectFlag) for that). There
// is no destructor for the same reason.
template<typename TYPE>
class hkRefPtr
{
public:
    hkRefPtr() {}
    // references e (as the SDK's hkRefPtr(TYPE*)); null is allowed
    hkRefPtr(TYPE* e)
    {
        if (e != nullptr)
        {
            e->addReference();
        }
        m_pntr = e;
    }
    hkRefPtr(const hkRefPtr& rp)
    {
        if (rp.m_pntr != nullptr)
        {
            rp.m_pntr->addReference();
        }
        m_pntr = rp.m_pntr;
    }
    TYPE* val() const { return m_pntr; }
    operator TYPE*() const { return m_pntr; }
    TYPE* operator->() const { return m_pntr; }
    hkRefPtr& operator=(TYPE* p)
    {
        if (p != nullptr)
        {
            p->addReference(); // first, for self-assignment
        }
        if (m_pntr != nullptr)
        {
            m_pntr->removeReference();
        }
        m_pntr = p;
        return *this;
    }
    // takes over a reference the caller holds (a new object): the old one is released unless it is e
    void setAndDontIncrementRefCount(TYPE* e)
    {
        if (m_pntr != nullptr && m_pntr != e)
        {
            m_pntr->removeReference();
        }
        m_pntr = e;
    }
    hkRefPtr& operator=(const hkRefPtr& rp)
    {
        return operator=(rp.m_pntr);
    }

private:
    TYPE* m_pntr; // 0x00
};

// size 0x4
// <hkpConstraintData> 0x00D703C0 (default constructor)
template<typename TYPE>
class hkViewPtr
{
public:
    operator TYPE*() const { return m_ptr; }
    TYPE* get() const { return m_ptr; }
    TYPE* operator->() const { return m_ptr; }
    // no reference is taken (hkpBridgeConstraintAtom::init)
    hkViewPtr& operator=(TYPE* p)
    {
        m_ptr = p;
        return *this;
    }

private:
    TYPE* m_ptr; // 0x00
};
