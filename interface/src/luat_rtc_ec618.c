
#include "luat_base.h"
#include "luat_rtc.h"
#include "common_api.h"
#include "time.h"
#include "osasys.h"

int luat_rtc_set(struct tm *tblock){
    uint32_t Timer1 = (((tblock->tm_year)<<16)&0xfff0000) | (((tblock->tm_mon)<<8)&0xff00) | ((tblock->tm_mday)&0xff);
    uint32_t Timer2 = ((tblock->tm_hour<<24)&0xff000000) | ((tblock->tm_min<<16)&0xff0000) | ((tblock->tm_sec<<8)&0xff00) | ((0)&0xff);
    uint32_t ret = OsaTimerSync(0, SET_LOCAL_TIME, Timer1, Timer2);
    if (ret == 0){
        mwAonSetUtcTimeSyncFlag(1);
    }
    return 0;
}

int luat_rtc_get(struct tm *tblock){
    utc_timer_value_t *timeUtc = OsaSystemTimeReadUtc();
    if(timeUtc != NULL){
        gmtime_ec((time_t *)&timeUtc->UTCsecs,tblock);
        tblock->tm_year += 1900;
        return 0;
    }
    return -1;
}

#ifdef __LUATOS__

int luat_rtc_timer_start(int id, struct tm *tblock){
    return -1;
}

int luat_rtc_timer_stop(int id){
    return -1;
}

#endif


