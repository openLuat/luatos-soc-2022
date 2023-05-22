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
#include "luat_pm.h"
#include "luat_mobile.h"
#include "luat_gpio.h"
luat_rtos_task_handle pm_task_handle;

// 休眠前回调函数
luat_pm_sleep_callback_t sleep_pre_callback(int mode)
{
    LUAT_DEBUG_PRINT("pm demo sleep before %d", mode);
}

// 唤醒后回调函数
luat_pm_sleep_callback_t sleep_post_callback(int mode)
{
    LUAT_DEBUG_PRINT("pm demo sleep post %d", mode);
}

//wakepad回调函数
luat_pm_wakeup_pad_isr_callback_t pad_wakeup_callback(LUAT_PM_WAKEUP_PAD_E num)
{
    LUAT_DEBUG_PRINT("pm demo wakeup pad num %d", num);
}

void soc_get_unilog_br(uint32_t *baudrate)
{
	*baudrate = 6000000; //UART0做log口输出6M波特率，必须用高性能USB转TTL
}

static void task_test_pm(void *param)
{
	luat_gpio_cfg_t gpio_cfg = {0};
	luat_mobile_set_rrc_auto_release_time(1);
	luat_mobile_config(MOBILE_CONF_T3324MAXVALUE, 0);
	luat_mobile_config(MOBILE_CONF_PSM_MODE, 1);
	luat_pm_set_usb_power(0);
	luat_pm_set_sleep_mode(LUAT_PM_SLEEP_MODE_STANDBY, "tes");
	gpio_cfg.pin = HAL_GPIO_23;
	gpio_cfg.mode = LUAT_GPIO_INPUT;
	luat_gpio_open(&gpio_cfg);
    while (1)
    {
    	luat_rtos_task_sleep(1000000);
    }
}

static void pm(void)
{
    luat_rtos_task_create(&pm_task_handle, 4 *1024, 20, "pm", task_test_pm, NULL, NULL);
}

INIT_TASK_EXPORT(pm, "1");
