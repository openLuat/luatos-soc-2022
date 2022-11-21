#include "wdt.h"
#include "clock.h"

int luat_wdt_setup(size_t timeout)
{
    if(timeout < 1 || timeout > 60)
    {
        return -1;
    }
    GPR_setClockSrc(FCLK_WDG, FCLK_WDG_SEL_32K);
    GPR_setClockDiv(FCLK_WDG, timeout);

    WdtConfig_t wdtConfig;
    wdtConfig.mode = WDT_INTERRUPT_RESET_MODE;
    wdtConfig.timeoutValue = 32768U;
    WDT_init(&wdtConfig);
    WDT_start();
    return 0;
}

int luat_wdt_set_timeout(size_t timeout)
{
    if(timeout < 1 || timeout > 60)
    {
        return -1;
    }
    WDT_kick();
    WDT_stop();
    WDT_deInit();
    luat_wdt_setup(timeout);
    return 0;
}

int luat_wdt_feed(void)
{
    WDT_kick();
    return 0;
}

int luat_wdt_close(void)
{
    WDT_stop();
    WDT_deInit();
    return 0;
}