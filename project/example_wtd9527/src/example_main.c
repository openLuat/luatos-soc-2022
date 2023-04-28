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
#include "luat_wtd9527.h"

/*
    外部看门狗芯片实现看门狗
*/

#define WTD_FEED_TEST           0       //测试喂狗
#define WTD_CLOSE_FEED          0       //关闭喂狗
#define CLOSE_FEED_AND_FEED     0       //关闭喂狗然后又打开
#define TEST_MODE_RESET_TEST    0       //测试模式复位
#define WTD_ASSERT_RESET        0       //模块死机看门狗重启
#define SETTING_TIME            0       //定时器模式设定时间定时开机
#define POWER_ON_HAND           1       //定时器模式关机状态下下拉INT主动开机

static luat_rtos_task_handle feed_wdt_task_handle;

static void* reset_timer;//复位定时器

static void wtd_reset_cb(uint32_t arg)
{
    luat_pm_reboot();
}


static void task_feed_wdt_run(void *param)
{   
    luat_wtd9527_cfg_init();
    reset_timer = luat_create_rtos_timer(wtd_reset_cb, NULL, NULL);

/*
    测试喂狗
*/
#if WTD_FEED_TEST
    LUAT_DEBUG_PRINT("[DIO] Feed WTD Test Start");
    LUAT_DEBUG_PRINT("[DIO]The Bef Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(2000);
    luat_wtd9527_set_wtdmode(1);
    LUAT_DEBUG_PRINT("[DIO]The Cur Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(3000);
    while (1)
    {
        luat_wtd9527_feed_wtd();
        luat_rtos_task_sleep(1000);
    }
#endif

/*
    关闭喂狗
*/
#if WTD_CLOSE_FEED
    LUAT_DEBUG_PRINT("[DIO] Close Feed WTD Test Start");
    LUAT_DEBUG_PRINT("[DIO]The Bef Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(2000);
    luat_wtd9527_set_wtdmode(1);
    LUAT_DEBUG_PRINT("[DIO]The Cur Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(3000);
    while (1)
    {
        luat_wtd9527_close();
        luat_rtos_task_sleep(5000);
    }
#endif


/*
    关闭喂狗然后又打开
*/
#if CLOSE_FEED_AND_FEED
    LUAT_DEBUG_PRINT("[DIO] Close And Open Feed WTD Test Start");
    LUAT_DEBUG_PRINT("[DIO]The Bef Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(2000);
    luat_wtd9527_set_wtdmode(1);
    LUAT_DEBUG_PRINT("[DIO]The Cur Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(3000);
    while (1)
    {
        LUAT_DEBUG_PRINT("[DIO] Close Feed WTD!");
        luat_wtd9527_close();
        luat_rtos_task_sleep(10000);//方便观察设置的时间长一点
        LUAT_DEBUG_PRINT("[DIO] Open Feed WTD!");
        luat_wtd9527_feed_wtd();
        luat_rtos_task_sleep(10000);
    }
#endif

/*
    测试模式复位
*/
#if TEST_MODE_RESET_TEST
    int count = 30;
    int reset_count = 0;
    bool isSeted = false;
    LUAT_DEBUG_PRINT("[DIO] Test Mode Test Start");
    LUAT_DEBUG_PRINT("[DIO]The Bef Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(2000);
    luat_wtd9527_set_wtdmode(1);
    LUAT_DEBUG_PRINT("[DIO]The Cur Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(3000);
    while (1)
    {
        if (count == 30)
        {
            //设定时间实际需要时间，每次喂狗需要250ms，所以也要依次等待相应的时间
            luat_wtd9527_set_timeout(8);
            /*
                等待时间可以举例：
                time = 24 / 4 * 250
            */
            luat_rtos_task_sleep(500);
            count = 0;
            isSeted = true;
        }
        int status = luat_wtd9527_get_wtd_reset_output();
        if (!status && isSeted)
        {
            reset_count++;
        }
        else
        {
            reset_count = 0;
        }
        if (reset_count >= 5)
        {
            reset_count = 0;
            LUAT_DEBUG_PRINT("[DIO]ReBoot!!!");
            // luat_pm_reboot();
        }
        LUAT_DEBUG_PRINT("[DIO]The Current is [%s]", status? "Power off" : "Power on");
        count++;
        luat_rtos_task_sleep(100);
    }
#endif


/*
    模块死机看门狗重启
*/
#if WTD_ASSERT_RESET
    int count = 0;
    LUAT_DEBUG_PRINT("[DIO] Feed WTD Test Start");
    LUAT_DEBUG_PRINT("[DIO]The Bef Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(2000);
    luat_wtd9527_set_wtdmode(1);
    LUAT_DEBUG_PRINT("[DIO]The Cur Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(3000);
    while (1)
    {
        int status = luat_wtd9527_get_wtd_reset_output();
        if (status)
        {
            count++;
            if (count > 50)
            {
                LUAT_DEBUG_PRINT("[DIO]The Module is Reboot!!!");
                luat_pm_reboot();
            }
        }
        LUAT_DEBUG_PRINT("[DIO]The Current is [%s]", status? "rebooting" : "Power on");
        luat_rtos_task_sleep(10);
    }
#endif

/*
    定时器模式设定时间定时开机
*/
#if SETTING_TIME
    int count = 10;
    LUAT_DEBUG_PRINT("[DIO] Setting Time Test Start");
    LUAT_DEBUG_PRINT("[DIO]The Bef Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(2000);
    luat_wtd9527_set_wtdmode(0);
    LUAT_DEBUG_PRINT("[DIO]The Cur Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(3000);
    while (1)
    {
        if (count == 10)
        {
            //设定时间实际需要时间，每次喂狗需要250ms，所以也要依次等待相应的时间
            luat_wtd9527_set_timeout(24);
            /*
                等待时间可以举例：
                time = 24 / 4 * 250；
            */
            luat_rtos_task_sleep(1500);
            count = 0;
        }
        int status = luat_wtd9527_get_wtd_reset_output();
        LUAT_DEBUG_PRINT("[DIO]The Current is [%s]", status? "Power off" : "Power on");
        count++;
        luat_rtos_task_sleep(500);
    }
#endif

/*
    定时器模式关机状态下下拉INT主动开机
*/
#if POWER_ON_HAND
    int count = 150;
    int irqCount = 0;
    LUAT_DEBUG_PRINT("[DIO] Power on Test Start");
    LUAT_DEBUG_PRINT("[DIO]The Bef Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(2000);
    luat_wtd9527_set_wtdmode(0);
    LUAT_DEBUG_PRINT("[DIO]The Cur Mode is [%d]", luat_wtd9527_get_wtdmode());
    luat_rtos_task_sleep(3000);
    while (1)
    {
        if (count == 150)
        {
            luat_wtd9527_set_timeout(16);
            luat_rtos_task_sleep(2000);
            count = 0;
        }
        if (irqCount == 70)
        {
            LUAT_DEBUG_PRINT("[DIO]The Irq is low");
            luat_wtd9527_irqPinSet(0);//下拉 INT 管脚需要时间 100ms 的时间，所以等待 200ms 使其生效
            luat_rtos_task_sleep(200);
            irqCount = 0;
        }
        int status = luat_wtd9527_get_wtd_reset_output();
        LUAT_DEBUG_PRINT("[DIO]The Current is [%s]", status? "Power off" : "Power on");
        count++;
        irqCount++;
        luat_rtos_task_sleep(100);
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



