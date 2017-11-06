#include "stdafx.h"
#include "Emu/Cell/PPUModule.h"

#include "cellRtc.h"

#include "Utilities/date/tz.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

/*
	A lot of this code was based on / taken from ppsspp
*/

logs::channel cellRtc("cellRtc");

extern u64 get_system_time();

bool isLeapYear(u32 year)
{
	return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

// This is the # of microseconds between January 1, 0001 and January 1, 1970.
const u64 rtcMagicOffset = 62135596800000000ULL;
const u64 tickResolution = 1000000ULL;

// 400 years is a convenient number, since leap days and everything cycle every 400 years.
// 400 years is in other words 20871 full weeks.
const u64 rtc400YearTicks = (u64)20871 * 7 * 24 * 3600 * 1000000ULL;

u64 utcTickToUNIXTime(u64 utcTick)
{
	return (utcTick - rtcMagicOffset) / tickResolution;
}

#ifdef _WIN32

#include <windows.h>

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	time_t rawtime;

	time(&rawtime);
	tv->tv_sec = (long)rawtime;

	// here starts the microsecond resolution:

	LARGE_INTEGER tickPerSecond;
	LARGE_INTEGER tick; // a point in time

						// get the high resolution counter's accuracy
	QueryPerformanceFrequency(&tickPerSecond);

	// what time is it ?
	QueryPerformanceCounter(&tick);

	// and here we get the current microsecond! \o/
	tv->tv_usec = (tick.QuadPart % tickPerSecond.QuadPart);

	return 0;
}
#endif // _WIN32_

u64 unixTimeToUtcTick(u64 unixTime)
{
	return (unixTime * tickResolution) + rtcMagicOffset;
}

u64 currentTimeToUtcTick()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return tickResolution * now.tv_sec + now.tv_usec + rtcMagicOffset;
}

void rtcTimeToTm(tm &val, vm::ptr<CellRtcDateTime> pt)
{
	val.tm_year = pt->year - 1900;
	val.tm_mon = pt->month - 1;
	val.tm_mday = pt->day;
	val.tm_wday = -1;
	val.tm_yday = -1;
	val.tm_hour = pt->hour;
	val.tm_min = pt->minute;
	val.tm_sec = pt->second;
	val.tm_isdst = 0;
}

u64 getPTickFromPTime(vm::ptr<CellRtcDateTime> pTime, vm::ptr<CellRtcTick> pTick, s64 tickOffset = 0)
{
	tm local;
	rtcTimeToTm(local, pTime);

	if (!tickOffset)
	{
		while (local.tm_year < 70)
		{
			tickOffset -= rtc400YearTicks;
			local.tm_year += 400;
		}
		while (local.tm_year >= 470)
		{
			tickOffset += rtc400YearTicks;
			local.tm_year -= 400;
		}
	}

	time_t seconds = _mkgmtime(&local);
	u64 result = rtcMagicOffset + (u64)seconds * 1000000ULL;
	result += pTime->microsecond + tickOffset;
	return result;
}

void populatePTickFromPTime(vm::ptr<CellRtcDateTime> pTime, vm::ptr<CellRtcTick> pTick, s64 tickOffset=0)
{
	pTick->tick = getPTickFromPTime(pTime, pTick, tickOffset);
}

void populatePTimeFromUnix(vm::ptr<CellRtcDateTime> pTime, time_t unixTime, int numYearAdd=0)
{
	cellRtc.warning("populatePTimeFromUnix %u\n", unixTime);
	struct tm *time = gmtime(&unixTime);
	if (!time)
	{
		cellRtc.error("populatePTimeFromUnix Date is too high/low to handle, pretending to work.");//Yoinked from ppsspp
		return;
	}

	pTime->year = time->tm_year + 1900 - numYearAdd * 400;
	pTime->month = time->tm_mon + 1;
	pTime->day = time->tm_mday;
	pTime->hour = time->tm_hour;
	pTime->minute = time->tm_min;
	pTime->second = time->tm_sec;
	pTime->microsecond = 0;//This is set properly later
}

void populatePTimeFromCurrentTime(vm::ptr<CellRtcDateTime> pTime)
{
	populatePTimeFromUnix(pTime, time(NULL));
}

void populatePTimeFromPTick(vm::ptr<CellRtcDateTime> pTime, vm::ptr<CellRtcTick> pTick)
{
	int numYearAdd = 0;
	if (pTick->tick < tickResolution)
	{
		pTime->year = 1;
		pTime->month = 1;
		pTime->day = 1;
		pTime->hour = 0;
		pTime->minute = 0;
		pTime->second = 0;
		pTime->microsecond = pTick->tick % 10000000ULL;//Give or take a 0 ?
		return;
	}
	else if (pTick->tick < rtcMagicOffset)
	{
		//Need to get a year past 1970 for gmtime
		//Add enough 400 year to pass over 1970.
		numYearAdd = (int)((rtcMagicOffset - pTick->tick) / rtc400YearTicks + 1);
		pTick->tick += rtc400YearTicks * numYearAdd;
	}

	while (pTick->tick >= rtcMagicOffset + rtc400YearTicks)
	{
		pTick->tick -= rtc400YearTicks;
		--numYearAdd;
	}

	time_t unixtime = utcTickToUNIXTime(pTick->tick);
	populatePTimeFromUnix(pTime, unixtime, numYearAdd);
	pTime->microsecond = pTick->tick % 10000000ULL;
}

s32 cellRtcGetCurrentTick(vm::ptr<CellRtcTick> pTick)
{
	cellRtc.warning("cellRtcGetCurrentTick(pTick=*0x%x)", pTick);
	//pTick->tick = currentTimeToUtcTick();
	pTick->tick = get_system_time();
	cellRtc.warning("cellRtcGetCurrentTick %jd\n", pTick->tick);

	return CELL_OK;
}

s32 cellRtcGetCurrentClock(vm::ptr<CellRtcDateTime> pTime, s32 iTimeZone)//timezone is a minute offset
{//TODO not sure if this is from the current time, or from the time set to this module..
	cellRtc.warning("cellRtcGetCurrentClock(pTime=*0x%x, time_zone=%d)", pTime, iTimeZone);

	time_t currTime = time(NULL);
	currTime += 60 * iTimeZone;
	populatePTimeFromUnix(pTime, currTime);

	return CELL_OK;
}

s32 cellRtcGetCurrentClockLocalTime(vm::ptr<CellRtcDateTime> pClock)
{
	cellRtc.warning("cellRtcGetCurrentClockLocalTime(pClock=*0x%x)", pClock);
	populatePTimeFromCurrentTime(pClock);
	return CELL_OK;
}

s32 cellRtcFormatRfc2822(vm::ptr<char> pszDateTime, vm::ptr<CellRtcTick> pUtc, s32 iTimeZone)
{
	cellRtc.todo("cellRtcFormatRfc2822(pszDateTime=*0x%x, pUtc=*0x%x, time_zone=%d)", pszDateTime, pUtc, iTimeZone);

	return CELL_OK;
}

s32 cellRtcFormatRfc2822LocalTime(vm::ptr<char> pszDateTime, vm::ptr<CellRtcTick> pUtc)
{
	cellRtc.todo("cellRtcFormatRfc2822LocalTime(pszDateTime=*0x%x, pUtc=*0x%x)", pszDateTime, pUtc);

	return CELL_OK;
}

s32 cellRtcFormatRfc3339(vm::ptr<char> pszDateTime, vm::ptr<CellRtcTick> pUtc, s32 iTimeZone)
{
	cellRtc.todo("cellRtcFormatRfc3339(pszDateTime=*0x%x, pUtc=*0x%x, iTimeZone=%d)", pszDateTime, pUtc, iTimeZone);
	
	return CELL_OK;
}

s32 cellRtcFormatRfc3339LocalTime(vm::ptr<char> pszDateTime, vm::ptr<CellRtcTick> pUtc)
{
	cellRtc.todo("cellRtcFormatRfc3339LocalTime(pszDateTime=*0x%x, pUtc=*0x%x)", pszDateTime, pUtc);
	
	return CELL_OK;
}

s32 cellRtcParseDateTime(vm::ptr<CellRtcTick> pUtc, vm::cptr<char> pszDateTime)
{
	cellRtc.todo("cellRtcParseDateTime(pUtc=*0x%x, pszDateTime=%s)", pUtc, pszDateTime);

	return CELL_OK;
}

s32 cellRtcParseRfc3339(vm::ptr<CellRtcTick> pUtc, vm::cptr<char> pszDateTime)
{
	cellRtc.todo("cellRtcParseRfc3339(pUtc=*0x%x, pszDateTime=%s)", pUtc, pszDateTime);

	return CELL_OK;
}

s32 cellRtcGetTick(vm::ptr<CellRtcDateTime> pTime, vm::ptr<CellRtcTick> pTick)
{
	cellRtc.warning("cellRtcGetTick(pTime=*0x%x, pTick=*0x%x)", pTime, pTick);
	populatePTickFromPTime(pTime, pTick);
	cellRtc.warning("cellRtcGetTick(pTime=*0x%x, pTick=*0x%x) returning %jd", pTime, pTick, pTick->tick);

	return CELL_OK;
}

s32 cellRtcSetTick(vm::ptr<CellRtcDateTime> pTime, vm::ptr<CellRtcTick> pTick)//cellRtcConvertTickToDateTime
{
	cellRtc.warning("cellRtcSetTick(pTime=*0x%x, pTick=*0x%x)", pTime, pTick);
	populatePTimeFromPTick(pTime, pTick);	
	return CELL_OK;
}

s32 cellRtcTickAddTicks(vm::ptr<CellRtcTick> dest, vm::ptr<CellRtcTick> src, s64 lAdd)
{
	cellRtc.warning("cellRtcTickAddTicks(dest=*0x%x, src=*0x%x, lAdd=%lld)", dest, src, lAdd);
	dest->tick = src->tick + lAdd;
	return CELL_OK;
}

s32 cellRtcTickAddMicroseconds(vm::ptr<CellRtcTick> dest, vm::ptr<CellRtcTick> src, s64 lAdd)
{
	cellRtc.warning("cellRtcTickAddMicroseconds(pTick0=*0x%x, pTick1=*0x%x, lAdd=%lld)", dest, src, lAdd);
	dest->tick = src->tick + lAdd;
	return CELL_OK;
}

s32 cellRtcTickAddSeconds(vm::ptr<CellRtcTick> dest, vm::ptr<CellRtcTick> src, s64 lAdd)
{
	cellRtc.warning("cellRtcTickAddSeconds(pTick0=*0x%x, pTick1=*0x%x, lAdd=%lld)", dest, src, lAdd);
	dest->tick = src->tick + (lAdd * 1000000UL);
	return CELL_OK;
}

s32 cellRtcTickAddMinutes(vm::ptr<CellRtcTick> dest, vm::ptr<CellRtcTick> src, s64 lAdd)
{
	cellRtc.warning("cellRtcTickAddMinutes(pTick0=*0x%x, pTick1=*0x%x, lAdd=%lld)", dest, src, lAdd);
	dest->tick = src->tick + (lAdd * 60000000UL);
	return CELL_OK;
}

s32 cellRtcTickAddHours(vm::ptr<CellRtcTick> dest, vm::ptr<CellRtcTick> src, s32 iAdd)
{
	cellRtc.warning("cellRtcTickAddHours(pTick0=*0x%x, pTick1=*0x%x, iAdd=%d)", dest, src, iAdd);
	dest->tick = src->tick + (iAdd * 3600ULL * 1000000ULL);
	return CELL_OK;
}

s32 cellRtcTickAddDays(vm::ptr<CellRtcTick> dest, vm::ptr<CellRtcTick> src, s32 iAdd)
{
	cellRtc.warning("cellRtcTickAddDays(pTick0=*0x%x, pTick1=*0x%x, iAdd=%d)", dest, src, iAdd);
	dest->tick = src->tick + (iAdd * 86400ULL * 1000000ULL);
	return CELL_OK;
}

s32 cellRtcTickAddWeeks(vm::ptr<CellRtcTick> dest, vm::ptr<CellRtcTick> src, s32 iAdd)
{
	cellRtc.warning("cellRtcTickAddWeeks(pTick0=*0x%x, pTick1=*0x%x, iAdd=%d)", dest, src, iAdd);
	dest->tick = src->tick + (iAdd * 7ULL * 86400ULL * 1000000ULL);
	return CELL_OK;
}

s32 cellRtcTickAddMonths(vm::ptr<CellRtcTick> dest, vm::ptr<CellRtcTick> src, s32 iAdd)
{
	cellRtc.warning("cellRtcTickAddMonths(pTick0=*0x%x, pTick1=*0x%x, iAdd=%d)", dest, src, iAdd);

	using namespace date;
	using namespace std::chrono;

	//Fucking pain in the arse..

	auto pt = vm::ptr<CellRtcDateTime>::make(vm::alloc(sizeof(CellRtcDateTime), vm::main));
	memset(pt.get_ptr(), 0, sizeof(pt));
	populatePTimeFromPTick(pt, src);

	std::tm tmInput = {};
	if (src->tick < 1000000ULL)
	{
		pt->year = 1;
		pt->month = 1;
		pt->day = 1;
		pt->hour = 0;
		pt->minute = 0;
		pt->second = 0;
		pt->microsecond = src->tick % 1000000ULL;
		populatePTickFromPTime(pt, dest);
		return CELL_OK;
	}
	else
	{
		tmInput.tm_year = pt->year - 1900;
		tmInput.tm_mon = pt->month - 1;
		tmInput.tm_mday = pt->day;
		tmInput.tm_wday = -1;
		tmInput.tm_yday = -1;
		tmInput.tm_hour = pt->hour;
		tmInput.tm_min = pt->minute;
		tmInput.tm_sec = pt->second;
		tmInput.tm_isdst = 0;
	}

	//test
	s64 tickOffset = 0;
	s64 yearOffset = 0;
	while (tmInput.tm_year < 70)
	{
		tickOffset -= rtc400YearTicks;
		tmInput.tm_year += 400;
		yearOffset += 400;
	}
	while (tmInput.tm_year >= 470)
	{
		tickOffset += rtc400YearTicks;
		tmInput.tm_year -= 400;
		yearOffset -= 400;
	}
	//test

	auto inputTime = std::chrono::system_clock::from_time_t(std::mktime(&tmInput));//

	auto ymd = year_month_day{ floor<days>(inputTime) };
	auto ymdAdjusted = ymd + months{ iAdd };//Add dems months

	pt->year = ymdAdjusted.year().y_;
	pt->month = ymdAdjusted.month().m_;
	pt->day = ymdAdjusted.day().d_;
	
	cellRtc.todo("%d %d %d %d", yearOffset, pt->year, pt->month, pt->day);
	if (pt->year - yearOffset  > 0 && pt->year - yearOffset <= 9999)
	{
		if (pt->month == 2 && pt->day == 29 && !isLeapYear(pt->year))
		{
			pt->day = 28;
		}
		u64 check = getPTickFromPTime(pt, dest, tickOffset);
		cellRtc.todo("check %d", check);
		dest->tick = check;
		
	}

	return CELL_OK;
}

s32 cellRtcTickAddYears(vm::ptr<CellRtcTick> dest, vm::ptr<CellRtcTick> src, s32 iAdd)
{
	cellRtc.warning("cellRtcTickAddYears(pTick0=*0x%x, pTick1=*0x%x, iAdd=%d)", dest, src, iAdd);
	cellRtcTickAddMonths(dest, src, iAdd * 12);//naughty?
	return CELL_OK;
}

s32 cellRtcConvertUtcToLocalTime(vm::ptr<CellRtcTick> pUtc, vm::ptr<CellRtcTick> pLocalTime)
{
	cellRtc.todo("cellRtcConvertUtcToLocalTime(pUtc=*0x%x, pLocalTime=*0x%x)", pUtc, pLocalTime);

	return CELL_OK;
}

s32 cellRtcConvertLocalTimeToUtc(vm::ptr<CellRtcTick> pLocalTime, vm::ptr<CellRtcTick> pUtc)
{
	cellRtc.todo("cellRtcConvertLocalTimeToUtc(pLocalTime=*0x%x, pUtc=*0x%x)", pLocalTime, pUtc);

	return CELL_OK;
}

s32 cellRtcGetCurrentSecureTick()
{
	UNIMPLEMENTED_FUNC(cellRtc);
	return CELL_OK;
}

s32 cellRtcGetDosTime(vm::ptr<CellRtcDateTime> pDateTime, vm::ptr<u32> puiDosTime)
{
	cellRtc.todo("cellRtcGetDosTime(pDateTime=*0x%x, puiDosTime=*0x%x)", pDateTime, puiDosTime);

	return CELL_OK;
}

s32 cellRtcGetSystemTime()
{
	UNIMPLEMENTED_FUNC(cellRtc);
	return CELL_OK;
}

s32 cellRtcGetTime_t(vm::ptr<CellRtcDateTime> pDateTime, vm::ptr<s64> piTime)
{
	cellRtc.todo("cellRtcGetTime_t(pDateTime=*0x%x, piTime=*0x%x)", pDateTime, piTime);

	return CELL_OK;
}

s32 cellRtcGetWin32FileTime(vm::ptr<CellRtcDateTime> pDateTime, vm::ptr<u64> pulWin32FileTime)
{
	cellRtc.todo("cellRtcGetWin32FileTime(pDateTime=*0x%x, pulWin32FileTime=*0x%x)", pDateTime, pulWin32FileTime);

	return CELL_OK;
}

s32 cellRtcSetCurrentSecureTick()
{
	UNIMPLEMENTED_FUNC(cellRtc);
	return CELL_OK;
}

s32 cellRtcSetCurrentTick()
{
	UNIMPLEMENTED_FUNC(cellRtc);
	return CELL_OK;
}

s32 cellRtcSetConf()
{
	UNIMPLEMENTED_FUNC(cellRtc);
	return CELL_OK;
}

s32 cellRtcSetDosTime(vm::ptr<CellRtcDateTime> pDateTime, u32 uiDosTime)
{
	cellRtc.todo("cellRtcSetDosTime(pDateTime=*0x%x, uiDosTime=0x%x)", pDateTime, uiDosTime);

	return CELL_OK;
}

s32 cellRtcGetTickResolution()
{
	cellRtc.warning("cellRtcGetTickResolution()");
	return tickResolution;
}

s32 cellRtcSetTime_t(vm::ptr<CellRtcDateTime> pDateTime, u64 iTime)
{
	cellRtc.todo("cellRtcSetTime_t(pDateTime=*0x%x, iTime=0x%llx)", pDateTime, iTime);
	

	return CELL_OK;
}

s32 cellRtcSetSystemTime()
{
	UNIMPLEMENTED_FUNC(cellRtc);
	return CELL_OK;
}

s32 cellRtcSetWin32FileTime(vm::ptr<CellRtcDateTime> pDateTime, u64 ulWin32FileTime)
{
	cellRtc.todo("cellRtcSetWin32FileTime(pDateTime=*0x%x, ulWin32FileTime=0x%llx)", pDateTime, ulWin32FileTime);

	return CELL_OK;
}

s32 cellRtcIsLeapYear(s32 year)
{
	cellRtc.warning("cellRtcIsLeapYear(year=%d)", year);

	return isLeapYear(year);
}

s32 cellRtcGetDaysInMonth(s32 year, s32 month)
{
	cellRtc.todo("cellRtcGetDaysInMonth(year=%d, month=%d)", year, month);

	if (year == 0 || month == 0 || month > 12)
		return -1;

	switch (month)
	{
	case 4:
	case 6:
	case 9:
	case 11:
		return 30;
	case 2:
		if (isLeapYear(year))
			return 29;
		return 28;
	default:
		return 31;
	}

	return -1;
}

s32 cellRtcGetDayOfWeek(s32 year, s32 month, s32 day)
{
	cellRtc.todo("cellRtcGetDayOfWeek(year=%d, month=%d, day=%d)", year, month, day);

	return 0;
}

s32 cellRtcCheckValid(vm::ptr<CellRtcDateTime> pTime)
{
	cellRtc.todo("cellRtcCheckValid(pTime=*0x%x)", pTime);

	if (pTime->year < 1 || pTime->year > 9999)
	{
		return CELL_RTC_ERROR_INVALID_YEAR;
	}
	else if (pTime->month < 1 || pTime->month > 12)
	{
		return CELL_RTC_ERROR_INVALID_MONTH;
	}
	else if (pTime->day < 1 || pTime->day > 31)
	{
		return CELL_RTC_ERROR_INVALID_DAY;
	}
	else if (pTime->day > cellRtcGetDaysInMonth(pTime->year, pTime->month))
	{
		return CELL_RTC_ERROR_INVALID_DAY;
	}
	else if (pTime->hour < 0 || pTime->hour > 23)
	{
		return CELL_RTC_ERROR_INVALID_HOUR;
	}
	else if (pTime->minute < 0 || pTime->minute > 59)
	{
		return CELL_RTC_ERROR_INVALID_MINUTE;
	}
	else if (pTime->second < 0 || pTime->second > 59)
	{
		return CELL_RTC_ERROR_INVALID_SECOND;
	}
	else if (pTime->microsecond >= 1000000UL)
	{
		return CELL_RTC_ERROR_INVALID_MICROSECOND;
	}

	return CELL_OK;
}

s32 cellRtcCompareTick(vm::ptr<CellRtcTick> pTick0, vm::ptr<CellRtcTick> pTick1)
{
	cellRtc.warning("cellRtcCompareTick(pTick0=*0x%x, pTick1=*0x%x)", pTick0, pTick1);

	if (pTick0->tick > pTick1->tick)
		return 1;
	if (pTick0->tick < pTick1->tick)
		return -1;
	return 0;
}

DECLARE(ppu_module_manager::cellRtc)("cellRtc", []()
{
	REG_FUNC(cellRtc, cellRtcGetCurrentTick);
	REG_FUNC(cellRtc, cellRtcGetCurrentClock);
	REG_FUNC(cellRtc, cellRtcGetCurrentClockLocalTime);

	REG_FUNC(cellRtc, cellRtcFormatRfc2822);
	REG_FUNC(cellRtc, cellRtcFormatRfc2822LocalTime);
	REG_FUNC(cellRtc, cellRtcFormatRfc3339);
	REG_FUNC(cellRtc, cellRtcFormatRfc3339LocalTime);
	REG_FUNC(cellRtc, cellRtcParseDateTime);
	REG_FUNC(cellRtc, cellRtcParseRfc3339);

	REG_FUNC(cellRtc, cellRtcGetTick);
	REG_FUNC(cellRtc, cellRtcSetTick);

	REG_FUNC(cellRtc, cellRtcTickAddTicks);
	REG_FUNC(cellRtc, cellRtcTickAddMicroseconds);
	REG_FUNC(cellRtc, cellRtcTickAddSeconds);
	REG_FUNC(cellRtc, cellRtcTickAddMinutes);
	REG_FUNC(cellRtc, cellRtcTickAddHours);
	REG_FUNC(cellRtc, cellRtcTickAddDays);
	REG_FUNC(cellRtc, cellRtcTickAddWeeks);
	REG_FUNC(cellRtc, cellRtcTickAddMonths);
	REG_FUNC(cellRtc, cellRtcTickAddYears);

	REG_FUNC(cellRtc, cellRtcConvertUtcToLocalTime);
	REG_FUNC(cellRtc, cellRtcConvertLocalTimeToUtc);

	REG_FUNC(cellRtc, cellRtcGetCurrentSecureTick);
	REG_FUNC(cellRtc, cellRtcGetDosTime);
	REG_FUNC(cellRtc, cellRtcGetTickResolution);
	REG_FUNC(cellRtc, cellRtcGetSystemTime);
	REG_FUNC(cellRtc, cellRtcGetTime_t);
	REG_FUNC(cellRtc, cellRtcGetWin32FileTime);

	REG_FUNC(cellRtc, cellRtcSetConf);
	REG_FUNC(cellRtc, cellRtcSetCurrentSecureTick);
	REG_FUNC(cellRtc, cellRtcSetCurrentTick);
	REG_FUNC(cellRtc, cellRtcSetDosTime);
	REG_FUNC(cellRtc, cellRtcSetTime_t);
	REG_FUNC(cellRtc, cellRtcSetSystemTime);
	REG_FUNC(cellRtc, cellRtcSetWin32FileTime);

	REG_FUNC(cellRtc, cellRtcIsLeapYear);
	REG_FUNC(cellRtc, cellRtcGetDaysInMonth);
	REG_FUNC(cellRtc, cellRtcGetDayOfWeek);
	REG_FUNC(cellRtc, cellRtcCheckValid);

	REG_FUNC(cellRtc, cellRtcCompareTick);
});
