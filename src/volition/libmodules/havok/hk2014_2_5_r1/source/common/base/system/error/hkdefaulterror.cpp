// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkdefaulterror.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the default error handler. A message at or above the minimum level whose id
// is not disabled is formatted and passed to the report function (the game's is havok_report_function, which drops
// it); asserts and errors ask the caller to break. An id of -1 takes the id of the innermost open section.
//
// Not reimplemented: the stack trace of asserts and errors (hkStackTracer, S phase) is not printed.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkdefaulterror.h"

#include <stdio.h>

namespace
{
// RAII lock of the handler's critical section.
struct Lock
{
    hkCriticalSection& m_section;
    explicit Lock(hkCriticalSection& section) : m_section(section) { m_section.enter(); }
    ~Lock() { m_section.leave(); }
};
}

// 0x00CF7890 (:17)
hkDefaultError::hkDefaultError(void (*errorReportFunction)(const char*, void*), void* errorReportObject)
    : m_lock(0) // 0: the spin count from the number of hardware threads
{
    m_errorFunction = errorReportFunction;
    m_errorObject = errorReportObject;
    m_minimumMessageLevel = MESSAGE_ALL;
}

// 0x00CF7DC0
hkDefaultError::~hkDefaultError()
{
}

// 0x00CF77A0 (:29)
void hkDefaultError::setEnabled(int id, hkBool enabled)
{
    Lock lock(m_lock);
    if (enabled)
    {
        m_disabledAssertIds.remove(id);
        return;
    }
    m_disabledAssertIds.insert(id, 1);
}

// 0x00CF7800 (:43)
hkBool hkDefaultError::isEnabled(int id)
{
    Lock lock(m_lock);
    hkBool result;
    result.m_bool = m_disabledAssertIds.getWithDefault(id, 0) == 0;
    return result;
}

// 0x00CF7640 (:50)
void hkDefaultError::enableAll()
{
    Lock lock(m_lock);
    m_minimumMessageLevel = MESSAGE_ALL;
    m_disabledAssertIds.clear();
}

// 0x00CF74D0 (:59)
void hkDefaultError::setMinimumMessageLevel(hkError::Message msg)
{
    m_minimumMessageLevel = msg;
}

// 0x00CF74E0 (:65)
hkError::Message hkDefaultError::getMinimumMessageLevel()
{
    return m_minimumMessageLevel;
}

// 0x00CF74F0 (:78)
void hkDefaultError::showMessage(const char* what, int id, const char* desc, const char* file, int line, hkBool stackTrace)
{
    char buffer[0x2000];
    if (id == 0 && file == nullptr)
    {
        snprintf(buffer, sizeof(buffer), "%s", desc);
    }
    else if (id == 0 || id == -1)
    {
        snprintf(buffer, sizeof(buffer), "%s(%d): %s: %s", file, line, what, desc);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%s(%d): [0x%08X] %s: %s", file, line, id, what, desc);
    }
    m_errorFunction(buffer, m_errorObject);
    // the stack trace (stackTrace set) is not printed: hkStackTracer is S phase
}

// 0x00CF7670 (:122)
int hkDefaultError::message(hkError::Message msg, int id, const char* description, const char* file, int line)
{
    Lock lock(m_lock);
    if (id == -1 && m_sectionIds.getSize() != 0)
    {
        id = m_sectionIds.back();
    }
    if ((int)msg < (int)getMinimumMessageLevel() || !isEnabled(id))
    {
        return 0;
    }
    const char* what = "";
    hkBool stackTrace;
    stackTrace.m_bool = 0;
    switch (msg)
    {
    case MESSAGE_REPORT:
        what = "Report";
        break;
    case MESSAGE_WARNING:
        what = "Warning";
        break;
    case MESSAGE_ASSERT:
        what = "Assert";
        stackTrace.m_bool = 1;
        break;
    case MESSAGE_ERROR:
        what = "Error";
        stackTrace.m_bool = 1;
        break;
    default:
        break;
    }
    showMessage(what, id, description, file, line, stackTrace);
    return (msg == MESSAGE_ASSERT || msg == MESSAGE_ERROR) ? 1 : 0;
}

// 0x00CF7840 (:169)
void hkDefaultError::sectionBegin(int id, const char* sectionName)
{
    Lock lock(m_lock);
    m_sectionIds.pushBack(id);
}

// 0x00CF7780 (:176)
void hkDefaultError::sectionEnd()
{
    Lock lock(m_lock);
    m_sectionIds.popBack();
}
