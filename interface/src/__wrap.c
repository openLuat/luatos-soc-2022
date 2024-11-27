#include "common_api.h"
#include "time.h"
#include "osasys.h"
#include "mw_aon_info.h"
extern MidWareAonInfo      *pMwAonInfo;

void RTC_GetDateTime(Date_UserDataStruct *pDate, Time_UserDataStruct *pTime)
{
	Tamp2UTC(time(NULL), pDate, pTime, 0);
}

static struct tm prvTM;
extern const uint32_t DayTable[2][12];
struct tm *__wrap_localtime (const time_t *_timer)
{
	int64_t Sec = 0;
	int64_t tz = 32;
	if (pMwAonInfo)
	{
		uint32_t tick = slpManGet6P25HZGlobalCnt();
		uint32_t cr = OS_EnterCritical();
		if (pMwAonInfo->crc16 == CRC16Cal(&pMwAonInfo->utc_tamp, 9, CRC16_CCITT_SEED, CRC16_CCITT_GEN, 0))
		{
			Sec = pMwAonInfo->utc_tamp;
			uint64_t diff = (tick - pMwAonInfo->rtc_tamp);
			Sec += diff * 4 / 25;
			tz = pMwAonInfo->tz;
		}
		OS_ExitCritical(cr);
	}
	Time_UserDataStruct Time;
	Date_UserDataStruct Date;
	if (_timer)
	{
		Sec = *_timer;
	}

	Sec += tz * 900;
	if (Sec < 0)
	{
		Sec = 0;
	}
	Tamp2UTC(Sec, &Date, &Time, 0);

	prvTM.tm_year = Date.Year - 1900;
	prvTM.tm_mon = Date.Mon - 1;
	prvTM.tm_mday = Date.Day;
	prvTM.tm_hour = Time.Hour;
	prvTM.tm_min = Time.Min;
	prvTM.tm_sec = Time.Sec;
	prvTM.tm_wday = Time.Week;
	prvTM.tm_yday = Date.Day - 1;
	prvTM.tm_yday += DayTable[IsLeapYear(Date.Year)][Date.Mon - 1];
	return &prvTM;
}

struct tm *__wrap_gmtime (const time_t *_timer)
{
	Time_UserDataStruct Time;
	Date_UserDataStruct Date;
	if (_timer)
	{
		Tamp2UTC(*_timer, &Date, &Time, 0);
	}
	else
	{

		uint64_t Sec = 0;
		if (pMwAonInfo)
		{
			uint32_t tick = slpManGet6P25HZGlobalCnt();
			uint32_t cr = OS_EnterCritical();
			if (pMwAonInfo->crc16 == CRC16Cal(&pMwAonInfo->utc_tamp, 9, CRC16_CCITT_SEED, CRC16_CCITT_GEN, 0))
			{
				Sec = pMwAonInfo->utc_tamp;
				uint64_t diff = (tick - pMwAonInfo->rtc_tamp);
				Sec += diff * 4 / 25;
			}
			OS_ExitCritical(cr);
		}
		Tamp2UTC(Sec, &Date, &Time, 0);
	}

	prvTM.tm_year = Date.Year - 1900;
	prvTM.tm_mon = Date.Mon - 1;
	prvTM.tm_mday = Date.Day;
	prvTM.tm_hour = Time.Hour;
	prvTM.tm_min = Time.Min;
	prvTM.tm_sec = Time.Sec;
	prvTM.tm_wday = Time.Week;
	prvTM.tm_yday = Date.Day - 1;
	prvTM.tm_yday += DayTable[IsLeapYear(Date.Year)][Date.Mon - 1];
	return &prvTM;
}

clock_t	   __wrap_clock (void)
{
	return GetSysTickMS()/1000;
}

time_t	   __wrap_time (time_t *_Time)
{
	time_t Sec = 0;
	if (pMwAonInfo)
	{
		uint32_t tick = slpManGet6P25HZGlobalCnt();
		uint32_t cr = OS_EnterCritical();
		if (pMwAonInfo->crc16 == CRC16Cal(&pMwAonInfo->utc_tamp, 9, CRC16_CCITT_SEED, CRC16_CCITT_GEN, 0))
		{
			Sec = pMwAonInfo->utc_tamp;
			time_t diff = (tick - pMwAonInfo->rtc_tamp);
			Sec += diff * 4 / 25;
		}
		OS_ExitCritical(cr);
	}
  if (_Time != NULL) {
    *_Time = Sec;
  }
  return Sec;
}
