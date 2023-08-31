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
#include "common_api.h"
#include "luat_rtos.h"
#include "luat_debug.h"
#include "luat_wtd9520.h"
#include "luat_pm.h"
#include "luat_uart.h"
#include "luat_gpio.h"
#include "plat_config.h"

/*
    外部看门狗芯片实现看门狗
*/

#define WTD_NOT_FEED_TEST       1       //测试不喂狗
#define WTD_FEED_TEST           0       //测试喂狗
#define WTD_CLOSE_FEED          0       //关闭喂狗
#define CLOSE_FEED_AND_FEED     0       //关闭喂狗然后又打开
#define TEST_MODE_RESET_TEST    0       //测试模式复位
#define SETTING_TIME            0       //定时器模式设定时间定时开机
#define POWER_ON_HAND           0       //定时器模式关机状态下下拉INT主动开机

static luat_rtos_task_handle feed_wdt_task_handle;

// void soc_get_unilog_br(uint32_t *baudrate)
// {
// 	*baudrate = 3000000; //UART0做log口输出12M波特率，必须用高性能USB转TTL
// }

static void task_feed_wdt_run(void *param)
{   
    LUAT_DEBUG_PRINT("[DIO] ------------");

    /*
        以下 if 的代码是使用 UART 进行 LOG 调试
    */    
	// if (BSP_GetPlatConfigItemValue(PLAT_CONFIG_ITEM_LOG_PORT_SEL) != PLAT_CFG_ULG_PORT_UART)
	// {
	// 	BSP_SetPlatConfigItemValue(PLAT_CONFIG_ITEM_LOG_PORT_SEL, PLAT_CFG_ULG_PORT_UART);
	// 	BSP_SavePlatConfigToRawFlash();
	// }

    luat_wtd9520_cfg_init(28);//初始化看门狗，设置喂狗管脚
    luat_wtd9520_feed_wtd();//模块开机第一步需要喂狗一次
    luat_rtos_task_sleep(500);

    
    /*
        测试不喂狗
    */
    #if WTD_NOT_FEED_TEST
        int count = 0;
        LUAT_DEBUG_PRINT("[DIO] Feed WTD Test Start");
        luat_rtos_task_sleep(3000);
        while (1)
        {
            count++;
            luat_rtos_task_sleep(1000);
            LUAT_DEBUG_PRINT("[DIO]Timer count(1s):[%d]", count);
        }
    #endif

/*
    测试喂狗
*/
#if WTD_FEED_TEST
    LUAT_DEBUG_PRINT("[DIO] Feed WTD Test Start");
    luat_rtos_task_sleep(3000);
    while (1)
    {
        luat_wtd9520_feed_wtd();
        LUAT_DEBUG_PRINT("[DIO]Eat Dog");
        luat_rtos_task_sleep(120000);
    }
#endif

/*
    关闭喂狗
*/
#if WTD_CLOSE_FEED
    int flag = 0;
    LUAT_DEBUG_PRINT("[DIO] Close Feed WTD Test Start");
    luat_rtos_task_sleep(3000);
    while (1)
    {
        if (!flag)
        {
            flag = 1;
            luat_wtd9520_close();
        }
        flag++;
        LUAT_DEBUG_PRINT("[DIO]Timer count(1s):[%d]", flag);
        luat_rtos_task_sleep(1000);
    }
#endif


/*
    关闭喂狗然后又打开
*/
#if CLOSE_FEED_AND_FEED
    int flag = 0;
    LUAT_DEBUG_PRINT("[DIO] Close And Open Feed WTD Test Start");
    luat_rtos_task_sleep(3000);
    while (1)
    {
        if (!flag)
        {
            flag = 1;
            LUAT_DEBUG_PRINT("[DIO] Close Feed WTD!");
            luat_wtd9520_close();
            luat_rtos_task_sleep(1000);//方便观察设置的时间长一点
        }
        flag++;
        if (flag == 280){
            LUAT_DEBUG_PRINT("[DIO] Open Feed WTD!");
            luat_wtd9520_feed_wtd();
        }
        luat_rtos_task_sleep(1000);
        LUAT_DEBUG_PRINT("[DIO]Timer count(1s):[%d]", flag);
    }
#endif

/*
    测试模式复位
*/
#if TEST_MODE_RESET_TEST
    int count = 0;
    LUAT_DEBUG_PRINT("[DIO] Test Mode Test Start");
    luat_rtos_task_sleep(3000);
    while (1)
    {
        if (count == 15)
        {
            LUAT_DEBUG_PRINT("[DIO] Rrset Module");
            //设定时间实际需要时间，每次喂狗需要500ms，所以也要依次等待相应的时间
            luat_wtd9520_set_timeout(8);
            /*
                等待时间可以举例：
                time = 8 / 4 * 500
            */
            luat_rtos_task_sleep(1100);
            count = 0;
        }
        count++;
        luat_rtos_task_sleep(1000);
        LUAT_DEBUG_PRINT("[DIO]Timer count(1s):[%d]", count);
    }
#endif

    luat_rtos_task_delete(feed_wdt_task_handle);
    
}

static void task_feed_wdt_init(void)
{
    luat_rtos_task_create(&feed_wdt_task_handle, 2048, 50, "feed_wdt", task_feed_wdt_run, NULL, 15);
}


static luat_rtos_task_handle dead_loop_task_handle;

static void task_dead_loop_run(void *param)
{
    while (1)
    {
        LUAT_DEBUG_PRINT("dead loop");
    }
    luat_rtos_task_delete(dead_loop_task_handle);
    
}

static void task_dead_loop_init(void)
{   
    luat_rtos_task_create(&dead_loop_task_handle, 2048, 50, "dead_loop", task_dead_loop_run, NULL, NULL);
}

INIT_TASK_EXPORT(task_feed_wdt_init,"0");
// 此task通过执行一个死循环，来模拟软件异常，20秒后看门狗超时会自动重启
// 仅用来模拟测试使用，有需要可以自行打开
// INIT_TASK_EXPORT(task_dead_loop_init,"2");



