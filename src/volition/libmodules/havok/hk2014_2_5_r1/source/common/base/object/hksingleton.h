// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.cpp
//
// Hand-written (Havok 2014.2.5, havok_impl.md HK1): the singleton template (one static instance pointer per type)
// and the registration nodes hkBaseSystem::initSingletons creates the registered singletons from (Havok 2014.1
// hkSingleton.h). Registration happens at static initialization (HK_SINGLETON_IMPLEMENTATION); no singleton of the
// subset is registered that way, so the node list stays empty.
#pragma once

class hkReferencedObject;

// size 0x1
// <hkBuiltinTypeRegistry> getInstance 0x00813890, s_instance 0x03A380D8
template<typename T>
class hkSingleton
{
public:
    static T& getInstance() { return *s_instance; }
    // Inline in Havok: the old instance loses its reference.
    static void replaceInstance(T* t)
    {
        if (s_instance != nullptr)
        {
            s_instance->removeReference();
        }
        s_instance = t;
    }

    static void replaceInstanceAndAddReference(T* t)
    {
        t->addReference();
        if (s_instance != nullptr)
        {
            s_instance->removeReference();
        }
        s_instance = t;
    }

protected:
    hkSingleton() {}

public:
    static T* s_instance;
};

// size 0x10
struct hkSingletonInitNode
{
    typedef hkReferencedObject* (*SingletonCreationFunction)();

    hkSingletonInitNode(const char* name, hkReferencedObject* (*func)(), void** ptr); // 0x00CE6230
    static void populate(hkSingletonInitNode* dstBase, const hkSingletonInitNode* srcBase); // 0x00CFCEA0
    static void depopulate(hkSingletonInitNode* dstBase, const hkSingletonInitNode* srcBase); // 0x00CFCF50
    hkSingletonInitNode* findByName(const char* name); // 0x00CFCE60

    const char* m_name; // 0x00
    hkReferencedObject* (*m_createFunc)(); // 0x04
    hkSingletonInitNode* m_next; // 0x08
    void** m_value; // 0x0C
    static hkSingletonInitNode* m_head; // 0x03A27004
};
