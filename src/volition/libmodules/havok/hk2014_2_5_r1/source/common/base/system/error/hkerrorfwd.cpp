// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerrorfwd.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): out-of-line forwards of the error macros to the error handler singleton.
// Each returns nonzero when the caller should break into the debugger.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"

// 0x00CE7880 (:11)
int hkErrorFwd::messageReport(int id, const char* description, const char* file, int line)
{
    return hkError::getInstance().message(hkError::MESSAGE_REPORT, id, description, file, line);
}

// 0x00CE78B0 (:16)
int hkErrorFwd::messageWarning(int id, const char* description, const char* file, int line)
{
    return hkError::getInstance().message(hkError::MESSAGE_WARNING, id, description, file, line);
}

// 0x00CE78E0 (:21)
int hkErrorFwd::messageAssert(int id, const char* description, const char* file, int line)
{
    return hkError::getInstance().message(hkError::MESSAGE_ASSERT, id, description, file, line);
}

// 0x00CE7910 (:26)
int hkErrorFwd::messageError(int id, const char* description, const char* file, int line)
{
    return hkError::getInstance().message(hkError::MESSAGE_ERROR, id, description, file, line);
}
