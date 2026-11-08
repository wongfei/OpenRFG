// Original: library/modules/runtime/base/include/keen/base/fileaccessmode.hpp
#pragma once

namespace keen
{
    enum FileAccessMode
    {
        FileAccessMode_Read = 0,
        FileAccessMode_Write = 1,
        FileAccessMode_Append = 2,
        FileAccessMode_ReadWrite = 3,
        FileAccessMode_Count = 4,
    };
}
