// Original: library/modules/runtime/base/sources/global/calendartime.cpp

#include "keen/base/calendartime.hpp"

#include <time.h>

namespace keen
{
    // 0x00BEF0C0 (:13)
    int64_t getCurrentCalendarTime()
    {
        return _time64(nullptr);
    }

    // 0x00BEF0D0 (:48)
    bool fillCalendarTime(CalendarDateTime* pDateTime, int64_t time, RequestedTimeZone timeZone)
    {
        __time64_t t = time;
        tm calendar;
        if (timeZone == RequestedTimeZone_Local)
        {
            if (_localtime64_s(&calendar, &t) != 0)
            {
                return false;
            }
        }
        else
        {
            if (_gmtime64_s(&calendar, &t) != 0)
            {
                return false;
            }
        }
        pDateTime->seconds = calendar.tm_sec;
        pDateTime->hour = calendar.tm_hour;
        pDateTime->minutes = calendar.tm_min;
        pDateTime->dayOfMonth = calendar.tm_mday;
        pDateTime->year = calendar.tm_year; // years since 1900, as tm keeps it
        pDateTime->month = calendar.tm_mon; // 0-11
        pDateTime->dayOfWeek = calendar.tm_wday;
        pDateTime->daylightSavingInEffect = calendar.tm_isdst;
        pDateTime->dayOfYear = calendar.tm_yday;
        return true;
    }

    // 0x00BEF190 (:239)
    bool fillLocalCalendarTime(CalendarDateTime* pDateTime, int64_t time)
    {
        return fillCalendarTime(pDateTime, time, RequestedTimeZone_Local); // inlined
    }

    // 0x00BEF230 (:244)
    bool fillUTCCalendarTime(CalendarDateTime* pDateTime, int64_t time)
    {
        return fillCalendarTime(pDateTime, time, RequestedTimeZone_UTC); // inlined
    }
}
