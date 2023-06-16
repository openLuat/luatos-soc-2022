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
#include "luat_network_adapter.h"
#include "common_api.h"
#include "luat_gpio.h"
#include "luat_rtos.h"
#include "luat_mobile.h"
#include "networkmgr.h"
#define PM_TEST_DEEP_SLEEP_TIMER_ID	3
#define PM_TEST_SERVICE_IP	"47.99.172.211" //换成自己的服务器，为了方便演示PSM模式，demo使用UDP
#define PM_TEST_SERVICE_PORT	45433 //换成自己的服务器
#define PM_TEST_PERIOD	1800 //单个项目测试时间30分钟，如果有需要可以修改
luat_rtos_task_handle pm_task_handle;
static uint8_t pm_wakeup_by_timer_flag;
luat_rtos_task_handle g_s_task_handle;
static int32_t luat_test_socket_callback(void *pdata, void *param)
{
	OS_EVENT *event = (OS_EVENT *)pdata;
	LUAT_DEBUG_PRINT("%x", event->ID);
	return 0;
}

static void pm_deep_sleep_timer_callback(uint8_t id)
{
	if (PM_TEST_DEEP_SLEEP_TIMER_ID == id)
	{
		LUAT_DEBUG_PRINT("deep sleep timer %d", id);
		pm_wakeup_by_timer_flag = 1;
	}
}

static int pm_deep_sleep_gpio_wakeup(void *pdata, void *param)
{

}

static void luatos_mobile_event_callback(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status)
{
	LUAT_DEBUG_PRINT("%d,%d,%d", event, index, status);
	if (LUAT_MOBILE_EVENT_NETIF == event)
	{
		if (LUAT_MOBILE_NETIF_LINK_ON == status)
		{
			char imsi[20];
			char iccid[24] = {0};
			luat_mobile_get_iccid(0, iccid, sizeof(iccid));
			LUAT_DEBUG_PRINT("ICCID %s", iccid);
			luat_mobile_get_imsi(0, imsi, sizeof(imsi));
			LUAT_DEBUG_PRINT("IMSI %s", imsi);
			char imei[20] = {0};
			luat_mobile_get_imei(0, imei, sizeof(imei));
			LUAT_DEBUG_PRINT("IMEI %s", imei);

			luat_socket_check_ready(index, NULL);
		}
	}
}



static void socket_task(void *param)
{
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG_RESET);
	const char remote_ip[] = PM_TEST_SERVICE_IP;
	static network_ctrl_t *g_s_network_ctrl;
	const char hello[] = "hello, luatos!";
	int result;
	uint8_t retry = 0;
	uint8_t *rx_data = malloc(1024);
	uint32_t tx_len, rx_len;
	uint8_t is_break,is_timeout;
	g_s_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_network_ctrl, g_s_task_handle, luat_test_socket_callback, NULL);
	network_set_base_mode(g_s_network_ctrl, 0, 15000, 0, 0, 0, 0);	//UDP

	if (luat_pm_get_wakeup_reason() > LUAT_PM_WAKEUP_FROM_POR)
	{
		luat_pm_deep_sleep_mode_timer_stop(PM_TEST_DEEP_SLEEP_TIMER_ID);
		luat_pm_set_power_mode(LUAT_PM_POWER_MODE_POWER_SAVER, 0);
		LUAT_DEBUG_PRINT("wakeup from deep sleep");
	}

	while(1)
	{
RETRY:
		result = network_connect(g_s_network_ctrl, remote_ip, sizeof(remote_ip) - 1, NULL, PM_TEST_SERVICE_PORT, 30000);
		if (!result)
		{
			result = network_tx(g_s_network_ctrl, hello, sizeof(hello) - 1, 0, NULL, 0, &tx_len, 15000);
			while (!result)
			{

				if (luat_pm_get_wakeup_reason() > LUAT_PM_WAKEUP_FROM_POR)
				{
					result = network_wait_rx(g_s_network_ctrl, 5000, &is_break, &is_timeout);
					if (!result)
					{
						if (!is_timeout && !is_break)
						{
							do
							{
								result = network_rx(g_s_network_ctrl, rx_data, 1024, 0, NULL, NULL, &rx_len);
								if (rx_len > 0)
								{
									LUAT_DEBUG_PRINT("rx %d", rx_len);
								}
							}while(!result && rx_len > 0);
						}
						else if (is_timeout)
						{
							network_close(g_s_network_ctrl, 5000);
							if (!retry)
							{
								LUAT_DEBUG_PRINT("无应答，也许协议栈已经异常了，需要重启协议栈");
								luat_mobile_reset_stack();
								goto RETRY;
							}
							else
							{
								LUAT_DEBUG_PRINT("无应答，协议栈也重启过了，放弃本次传输，重新休眠");
								goto SLEEP;
							}

						}
					}
SLEEP:
					LUAT_DEBUG_PRINT("发送完成，再次休眠");
					luat_pm_deep_sleep_mode_register_timer_cb(PM_TEST_DEEP_SLEEP_TIMER_ID, pm_deep_sleep_timer_callback);
					luat_pm_deep_sleep_mode_timer_start(PM_TEST_DEEP_SLEEP_TIMER_ID, PM_TEST_PERIOD * 1000);
					luat_pm_set_power_mode(LUAT_PM_POWER_MODE_POWER_SAVER, 0);
					//最低功耗要关闭GPIO23输出
					luat_gpio_cfg_t gpio_cfg = {0};
					gpio_cfg.pin = HAL_GPIO_23;
					gpio_cfg.mode = LUAT_GPIO_INPUT;
					luat_gpio_open(&gpio_cfg);
					//用GPIO20来唤醒
					gpio_cfg.pin = HAL_GPIO_20;
					gpio_cfg.mode = LUAT_GPIO_IRQ;
					gpio_cfg.irq_cb = pm_deep_sleep_gpio_wakeup;
					gpio_cfg.pull = LUAT_GPIO_PULLUP;
					gpio_cfg.irq_type = LUAT_GPIO_FALLING_IRQ;
					luat_gpio_open(&gpio_cfg);

					//powerkey接地的，还要关闭powerkey的上拉
					pwrKeyHwDeinit(0);
					luat_rtos_task_sleep(3000);
					LUAT_DEBUG_PRINT("未进入极致功耗模式，也许是协议栈出问题了，重启协议栈来恢复一下");
					luat_pm_deep_sleep_mode_timer_stop(PM_TEST_DEEP_SLEEP_TIMER_ID);
					luat_pm_deep_sleep_mode_timer_start(PM_TEST_DEEP_SLEEP_TIMER_ID, PM_TEST_PERIOD * 1000);
					luat_pm_set_power_mode(LUAT_PM_POWER_MODE_NORMAL, 0);
					result = network_wait_link_up(g_s_network_ctrl, 15000);
					luat_rtos_task_sleep(1000);
					luat_pm_set_power_mode(LUAT_PM_POWER_MODE_POWER_SAVER, 0);
					luat_rtos_task_sleep(15000);
					LUAT_DEBUG_ASSERT(0, "!!");

				}

				result = network_wait_rx(g_s_network_ctrl, 300000, &is_break, &is_timeout);
				if (!result)
				{
					if (!is_timeout && !is_break)
					{
						do
						{
							result = network_rx(g_s_network_ctrl, rx_data, 1024, 0, NULL, NULL, &rx_len);
							if (rx_len > 0)
							{
								LUAT_DEBUG_PRINT("rx %d", rx_len);
							}
						}while(!result && rx_len > 0);
					}
					else if (is_timeout)
					{
						result = network_tx(g_s_network_ctrl, hello, sizeof(hello) - 1, 0, NULL, 0, &tx_len, 15000);
					}
				}
			}
		}
		LUAT_DEBUG_PRINT("网络断开，立刻重试");
		network_close(g_s_network_ctrl, 5000);
//		luat_rtos_task_sleep(15000);
	}


}

static void pm_task(void *param)
{

	LUAT_DEBUG_PRINT("开始演示联网低功耗功能");
	LUAT_DEBUG_PRINT("响应优先模式测试%d分钟", PM_TEST_PERIOD/60);
	luat_pm_set_power_mode(LUAT_PM_POWER_MODE_HIGH_PERFORMANCE, 0);
	luat_rtos_task_sleep(PM_TEST_PERIOD * 1000);
	LUAT_DEBUG_PRINT("均衡模式测试%d分钟", PM_TEST_PERIOD/60);
	luat_pm_set_power_mode(LUAT_PM_POWER_MODE_BALANCED, 0);
	luat_rtos_task_sleep(PM_TEST_PERIOD * 1000);
	LUAT_DEBUG_PRINT("无低功耗模式测试%d分钟", PM_TEST_PERIOD/60);
	luat_pm_set_power_mode(LUAT_PM_POWER_MODE_NORMAL, 0);
	luat_rtos_task_sleep(PM_TEST_PERIOD * 1000);
	LUAT_DEBUG_PRINT("PSM+模式，每%d分钟唤醒一次发送数据", PM_TEST_PERIOD/60);
	luat_pm_deep_sleep_mode_register_timer_cb(PM_TEST_DEEP_SLEEP_TIMER_ID, pm_deep_sleep_timer_callback);
	luat_pm_deep_sleep_mode_timer_start(PM_TEST_DEEP_SLEEP_TIMER_ID, PM_TEST_PERIOD * 1000);
	luat_pm_set_power_mode(LUAT_PM_POWER_MODE_POWER_SAVER, 0);
	//最低功耗要关闭GPIO23输出
	luat_gpio_cfg_t gpio_cfg = {0};
	gpio_cfg.pin = HAL_GPIO_23;
	gpio_cfg.mode = LUAT_GPIO_INPUT;
	luat_gpio_open(&gpio_cfg);

	//用GPIO20来唤醒
	gpio_cfg.pin = HAL_GPIO_20;
	gpio_cfg.mode = LUAT_GPIO_IRQ;
	gpio_cfg.irq_cb = pm_deep_sleep_gpio_wakeup;
	gpio_cfg.pull = LUAT_GPIO_PULLUP;
	gpio_cfg.irq_type = LUAT_GPIO_FALLING_IRQ;
	luat_gpio_open(&gpio_cfg);

	//powerkey接地的，还要关闭powerkey的上拉
	pwrKeyHwDeinit(0);
	luat_rtos_task_sleep(3000);
	LUAT_DEBUG_PRINT("未进入极致功耗模式，也许是协议栈出问题了，重启协议栈来恢复一下");
	luat_pm_deep_sleep_mode_timer_stop(PM_TEST_DEEP_SLEEP_TIMER_ID);
	luat_pm_deep_sleep_mode_timer_start(PM_TEST_DEEP_SLEEP_TIMER_ID, PM_TEST_PERIOD * 1000);
	luat_pm_set_power_mode(LUAT_PM_POWER_MODE_NORMAL, 0);
	luat_rtos_task_sleep(1000);
	luat_pm_set_power_mode(LUAT_PM_POWER_MODE_POWER_SAVER, 0);
	luat_rtos_task_sleep(15000);
	LUAT_DEBUG_ASSERT(0, "!!!");

}

/*
 * 本demo几乎所有log都从uart0输出，必须提高波特率
 */
void soc_get_unilog_br(uint32_t *baudrate)
{
	*baudrate = 6000000; //UART0做log口输出6M波特率，必须用高性能USB转TTL，比如CH343
}


static void pm_pre_init(void)
{
	luat_pm_deep_sleep_mode_register_timer_cb(PM_TEST_DEEP_SLEEP_TIMER_ID, pm_deep_sleep_timer_callback);
}

static void pm(void)
{
	luat_mobile_event_register_handler(luatos_mobile_event_callback);
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
	luat_rtos_task_create(&g_s_task_handle, 4 * 1024, 50, "socket", socket_task, NULL, 16);
	if (LUAT_PM_WAKEUP_FROM_POR == luat_pm_get_wakeup_reason())
	{
		luat_rtos_task_create(&pm_task_handle, 2 * 1024, 50, "pm", pm_task, NULL, 16);
	}



}
INIT_HW_EXPORT(pm_pre_init, "1");
INIT_TASK_EXPORT(pm, "1");
