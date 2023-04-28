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
#include "luat_uart.h"
#include "luat_gpio.h"
#include "plat_config.h"
/*
 * 强制LOG从UART0输出，并提高波特率，减少LOG丢失，强制进入浅休眠状态
 */
void soc_get_unilog_br(uint32_t *baudrate)
{
	*baudrate = 12000000; //UART0做log口输出12M波特率，必须用高性能USB转TTL
}

static void task_run(void *param)
{
	if (BSP_GetPlatConfigItemValue(PLAT_CONFIG_ITEM_LOG_PORT_SEL) != PLAT_CFG_ULG_PORT_UART)
	{
		BSP_SetPlatConfigItemValue(PLAT_CONFIG_ITEM_LOG_PORT_SEL, PLAT_CFG_ULG_PORT_UART);
		BSP_SavePlatConfigToRawFlash();
	}
	//要求达到最低的外设消耗状态
	luat_gpio_close(HAL_WAKEUP_PWRKEY);
	luat_gpio_close(HAL_GPIO_23);
	luat_pm_set_sleep_mode(LUAT_PM_SLEEP_MODE_LIGHT, "test");
	luat_pm_set_usb_power(0);
	while(1)
	{
		luat_rtos_task_sleep(86400000);
	}
}

void task_init(void)
{

	luat_rtos_task_handle task_handle;
	luat_rtos_task_create(&task_handle, 8*1024, 50, "test", task_run, NULL, 32);
}

//启动task_demoE_init，启动位置任务1级
INIT_TASK_EXPORT(task_init, "1");
