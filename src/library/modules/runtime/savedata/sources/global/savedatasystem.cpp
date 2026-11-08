// Original: library/modules/runtime/savedata/sources/global/savedatasystem.cpp
//
// Which save data systems the platform has (stub_impl.md S2).

#include "keen/savedata/savedatasystem.hpp"

namespace keen
{
    namespace SaveData
    {
        // 0x00CCCDA0 (:6): the Win32 build has the development, native and Steam systems (only Steam is creatable,
        // savedatasystem_win32.cpp)
        bool isSaveDataSystemTypeAvailable(SaveDataSystemType type)
        {
            return type == SaveDataSystemType_Win32Dev || type == SaveDataSystemType_Win32Native || type == SaveDataSystemType_Steam;
        }
    }
}
