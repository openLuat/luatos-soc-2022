/*
 * Copyright (c) 2022 OpenLuat & AirM2M
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "luat_base.h"
#include "luat_rtc.h"
#include "common_api.h"
#include "time.h"
#include "osasys.h"
#include "luat_debug.h"
#include "mw_aon_info.h"
extern MidWareAonInfo      *pMwAonInfo;
extern void soc_save_rtc_tamp_u32_with_tz(uint32_t utc, int8_t tz);
extern void soc_set_rtc_time_u32_with_tz(uint16_t year, uint8_t mon, uint8_t day, uint8_t h, uint8_t m, uint8_t s, int8_t tz);
extern void soc_save_tz(int8_t tz);
void soc_save_rtc_tamp_u32(uint32_t utc);

int luat_rtc_set(struct tm *tblock){
	int8_t tz = 32;
	if (pMwAonInfo)
	{
		uint32_t cr = OS_EnterCritical();
		if (pMwAonInfo->crc16 == CRC16Cal(&pMwAonInfo->utc_tamp, 9, CRC16_CCITT_SEED, CRC16_CCITT_GEN, 0))
		{
			tz = pMwAonInfo->tz;
		}
		OS_ExitCritical(cr);
	}
	Date_UserDataStruct Date;
	Time_UserDataStruct Time;
	Date.Year = tblock->tm_year+1900;
	Date.Mon = tblock->tm_mon+1;
	Date.Day = tblock->tm_mday;
	Time.Hour = tblock->tm_hour;
	Time.Min = tblock->tm_min;
	Time.Sec = tblock->tm_sec;
	soc_save_rtc_tamp_u32_with_tz(UTC2Tamp(&Date, &Time), tz);
    return 0;
}

int luat_rtc_get(struct tm *tblock){
    struct tm *t = gmtime(NULL);
    memcpy(tblock,t,sizeof(struct tm));
    return 0;
}

#ifdef __LUATOS__

void luat_rtc_set_tamp32(uint32_t tamp) {
	soc_save_rtc_tamp_u32(tamp);
}

int luat_rtc_timer_start(int id, struct tm *tblock){
    (void)id;
    (void)tblock;
    DBG("rtc timer isn't support");
    return -1;
}

int luat_rtc_timer_stop(int id){
    (void)id;
    return -1;
}

int luat_rtc_timezone(int* timezone) {
    if (timezone != NULL) {
        soc_save_tz(*timezone);
    }
    return pMwAonInfo->tz;
}

#endif


