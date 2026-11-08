// Original: library/modules/runtime/base/sources/global/stringbuilder.cpp
//
// StringBuilder appends into a fixed, always terminated buffer. Running out of space sets the overflow flag and the
// append fails; appendString(const char*) then also drops the remaining capacity to zero.

#include "keen/base/stringbuilder.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <stdarg.h>

namespace keen
{
    // 0x00BF2410 (:11)
    StringBuilder::StringBuilder()
    {
        m_pOriginalDestination = nullptr;
        m_originalCapacity = 0;
        m_pCurrentDestination = nullptr;
        m_remainingCapacity = 0;
        m_overflow = false;
    }

    // 0x00C005C0 (:21)
    StringBuilder::StringBuilder(char* pDestination, uint32 capacity)
    {
        reset(pDestination, capacity);
    }

    // 0x00BFAC10 (:26)
    void StringBuilder::reset(char* pDestination, uint32 capacity)
    {
        // Appends after the text already in the buffer.
        m_originalCapacity = capacity;
        m_remainingCapacity = capacity;
        m_pOriginalDestination = pDestination;
        m_pCurrentDestination = pDestination;
        while (*m_pCurrentDestination != '\0' && m_remainingCapacity != 0)
        {
            ++m_pCurrentDestination;
            --m_remainingCapacity;
        }
        m_overflow = false;
    }

    // 0x00BF2430 (:40)
    void StringBuilder::clear()
    {
        m_pCurrentDestination = m_pOriginalDestination;
        m_remainingCapacity = m_originalCapacity;
    }

    // 0x00BF2440 (:46)
    void StringBuilder::setBuffer(char* pDestination, uint32 capacity)
    {
        m_pOriginalDestination = pDestination;
        m_originalCapacity = capacity;
        m_pCurrentDestination = pDestination;
        m_remainingCapacity = capacity;
    }

    // 0x00BF2460 (:54)
    uint32 StringBuilder::getSize()
    {
        return (uint32)(m_pCurrentDestination - m_pOriginalDestination);
    }

    // 0x00BFAC50 (:60)
    bool StringBuilder::appendString(const char* pSource)
    {
        if (pSource == nullptr || *pSource == '\0')
        {
            return true;
        }
        if (m_remainingCapacity == 0)
        {
            m_overflow = true;
            return false;
        }
        const uint32 length = keen::copyString(m_pCurrentDestination, m_remainingCapacity, pSource);
        if (length >= m_remainingCapacity)
        {
            m_overflow = true;
            m_remainingCapacity = 0;
            return false;
        }
        m_pCurrentDestination += length;
        m_remainingCapacity -= length;
        return true;
    }

    // 0x00BFACB0 (:86)
    bool StringBuilder::appendString(const char* pBegin, const char* pEnd)
    {
        for (; pBegin != pEnd; ++pBegin)
        {
            if (*pBegin == '\0')
            {
                continue;
            }
            if (m_remainingCapacity < 2)
            {
                m_overflow = true;
                return false;
            }
            *m_pCurrentDestination++ = *pBegin;
            *m_pCurrentDestination = '\0';
            --m_remainingCapacity;
        }
        return true;
    }

    // 0x00C09720 (:98)
    bool StringBuilder::appendFormattedString(const char* pFormat, ...)
    {
        if (pFormat == nullptr || *pFormat == '\0')
        {
            return true;
        }
        if (m_remainingCapacity == 0)
        {
            m_overflow = true;
            return false;
        }
        va_list arguments;
        va_start(arguments, pFormat);
        const uint32 size = formatStringVarArgs(m_pCurrentDestination, m_remainingCapacity, pFormat, arguments);
        va_end(arguments);
        if (size >= m_remainingCapacity)
        {
            debug::nativePrintArgs("[base] buffer overflow in StringBuilder !\n");
            return false;
        }
        // size includes the terminator.
        m_pCurrentDestination += size - 1;
        m_remainingCapacity = m_remainingCapacity - size + 1;
        return true;
    }

    // 0x00BFAD00 (:129)
    bool StringBuilder::copyString(const char* pSource)
    {
        clear();
        return appendString(pSource);
    }

    // 0x00BFAD60 (:135)
    bool StringBuilder::copyString(const char* pBegin, const char* pEnd)
    {
        clear();
        return appendString(pBegin, pEnd);
    }

    // 0x00BF2470 (:141)
    bool StringBuilder::appendCharacter(char source)
    {
        if (source == '\0')
        {
            return true;
        }
        if (m_remainingCapacity < 2)
        {
            m_overflow = true;
            return false;
        }
        *m_pCurrentDestination++ = source;
        *m_pCurrentDestination = '\0';
        --m_remainingCapacity;
        return true;
    }

    // 0x00BEFBB0 (.h:39)
    bool StringBuilder::isOverflown() const
    {
        return m_overflow;
    }

    // 0x00BEFBC0 (.h:40)
    bool StringBuilder::isEmpty() const
    {
        return m_pOriginalDestination == m_pCurrentDestination;
    }

    // 0x00BF24B0 (:161)
    void StringBuilder::removeLastCharacter()
    {
        if (m_pOriginalDestination < m_pCurrentDestination)
        {
            ++m_remainingCapacity;
            --m_pCurrentDestination;
            *m_pCurrentDestination = '\0';
        }
    }

    // 0x00BFAD70 (:172)
    void StringBuilder::removeStringUntilCharacter(char letter)
    {
        while (getLastCharacter() != letter)
        {
            if (m_pCurrentDestination <= m_pOriginalDestination)
            {
                return;
            }
            ++m_remainingCapacity;
            --m_pCurrentDestination;
            *m_pCurrentDestination = '\0';
        }
    }

    // 0x00BF24D0 (:180)
    char StringBuilder::getLastCharacter() const
    {
        if (m_pCurrentDestination != nullptr && m_pCurrentDestination != m_pOriginalDestination)
        {
            return m_pCurrentDestination[-1];
        }
        return '\0';
    }
}
