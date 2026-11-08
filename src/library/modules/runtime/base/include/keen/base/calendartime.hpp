// Original: library/modules/runtime/base/include/keen/base/calendartime.hpp
// Implementation: library/modules/runtime/base/sources/global/calendartime.cpp
//
// Calendar time (CalendarDateTime) in local time or UTC.
#pragma once

#include "keen/base/types.hpp"
#include <stdint.h>

namespace keen
{
    enum RequestedTimeZone
    {
        RequestedTimeZone_UTC = 0,
        RequestedTimeZone_Local = 1,
    };

    // size 0x24
    struct CalendarDateTime
    {
        int seconds; // 0x00
        int minutes; // 0x04
        int hour; // 0x08
        int dayOfMonth; // 0x0C
        int month; // 0x10
        int year; // 0x14
        int dayOfWeek; // 0x18
        int dayOfYear; // 0x1C
        int daylightSavingInEffect; // 0x20
    };

    int64_t getCurrentCalendarTime(); // 0x00BEF0C0 (:13)
    bool fillCalendarTime(CalendarDateTime* pDateTime, int64_t time, RequestedTimeZone timeZone); // 0x00BEF0D0 (:48)
    bool fillLocalCalendarTime(CalendarDateTime* pDateTime, int64_t time); // 0x00BEF190 (:239)
    bool fillUTCCalendarTime(CalendarDateTime* pDateTime, int64_t time); // 0x00BEF230 (:244)
}
