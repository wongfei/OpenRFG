// Original: library/modules/runtime/base/include/keen/base/sort.hpp
//
// Sorting helpers (quickSort, insertionSort; function templates listed as comments) and lessThanPredicate<T>.
#pragma once

#include "keen/base/types.hpp"

#include <type_traits>

namespace keen
{
    // size (<PakfileEntry>) 0x1
    template<typename T>
    class lessThanPredicate
    {
    public:
        bool operator()(const T& lhs, const T& rhs) // <PakfileEntry> 0x00BF3FF0 (.h:145)
        {
            return lhs < rhs;
        }

    };

    // template instance: void quickSort<voice_chat_speak_duration,float,voice_chat_speak_duration,16>(voice_chat_speak_duration* pData, uint32 count); // 0x008CE660 (.h:30)
    // template instance: void quickSort<RenderCommand,uint32,RenderCommandKey,16>(RenderCommand* pData, uint32 count); // 0x00CC8E10 (.h:30)
    // template instance: void quickSort<RenderCommand,uint32,RenderCommandKey>(RenderCommand* pData, uint32 count); // 0x00CCAB20 (.h:77)
    // template instance: void quickSort<voice_chat_speak_duration,float,voice_chat_speak_duration>(voice_chat_speak_duration* pData, uint32 count); // 0x008DD9C0 (.h:77)
    // template instance: void insertionSort<memory::LayoutCreationHelper::CreationBlockInfo,memory::LayoutCreationHelper::CreationBlockInfo>(memory::LayoutCreationHelper::CreationBlockInfo* pData, uint32 count); // 0x00BF3B80 (.h:102)
    // template instance: void insertionSort<PakfileEntry*>(PakfileEntry* start, PakfileEntry* end); // 0x00C00D00 (.h:155)
    // template instance: void insertionSort<PakfileEntry*,lessThanPredicate<PakfileEntry> >(PakfileEntry* start, PakfileEntry* end, lessThanPredicate<PakfileEntry> pred); // 0x00BFBE90 (.h:166)

    // <PakfileEntry*,lessThanPredicate<PakfileEntry> > 0x00BFBE90 (.h:166)
    template<typename TIterator, typename TPredicate>
    void insertionSort(TIterator start, TIterator end, TPredicate pred)
    {
        if (start == end)
        {
            return;
        }
        for (TIterator it = start + 1; it != end; ++it)
        {
            const auto value = *it;
            TIterator target = it;
            TIterator previous = it - 1;
            while (pred(value, *previous))
            {
                *target = *previous;
                --target;
                if (previous == start)
                {
                    break;
                }
                --previous;
            }
            *target = value;
        }
    }

    // <PakfileEntry*> 0x00C00D00 (.h:155)
    template<typename TIterator>
    void insertionSort(TIterator start, TIterator end)
    {
        typedef typename std::remove_reference<decltype(*start)>::type ValueType;
        insertionSort(start, end, lessThanPredicate<ValueType>());
    }
}
