// Original: library/modules/runtime/base/include/keen/base/spscqueue.hpp
//
// SPSCQueue, a lock-free single producer, single consumer queue.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/win32/mutex_win32.hpp"

namespace keen
{
    // size 0x40
    class SPSCQueue
    {
    public:
        // size 0x4
        struct Node
        {
            Node* pNext; // 0x00
        };

        void create();
        void destroy();
        bool isEmpty() const;
        void push(Node*);
        Node* pop();

    private:
        Mutex m_mutex; // 0x00
        Node* m_pHead; // 0x38
        Node* m_pTail; // 0x3C
    };
}
