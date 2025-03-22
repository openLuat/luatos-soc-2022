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
#include "FreeRTOS.h"
#include "task.h"
#include "wdt.h"
#include "luat_pm.h"
#include "luat_rtos.h"
#include "luat_mobile.h"
#include "luat_sms.h"
#include "luat_network_adapter.h"
#include "ps_event_callback.h"
#include "cmidev.h"
#include "networkmgr.h"
#include "plat_config.h"
#include "driver_gpio.h"
#include "luat_uart.h"
#ifdef LUAT_USE_LVGL
#include "lvgl.h"
#include "luat_lvgl.h"
#endif
#include "luat_errdump.h"
#include "luat_netdrv.h"
#include "cmisim.h"

extern int luat_main(void);
extern void luat_heap_init(void);
extern void luat_pm_init(void);
extern void luat_wlan_done_callback_ec618(void *param);
const char *soc_get_chip_name(void)
{
#ifdef LUAT_EC618_LITE_MODE
	return "EC618";
#endif
#ifdef LUAT_USE_TTS
	return "EC618_TTS";
#endif
	return "EC618_FULL";
}

const char *soc_get_sdk_type(void)
{
	return "LuatOS-SoC";
}

const char *soc_get_sdk_version(void)
{
	return LUAT_BSP_VERSION;
}

#ifdef LUAT_USE_LVGL
luat_rtos_timer_t lvgl_timer_handle;
#define LVGL_TICK_PERIOD	10
unsigned int g_lvgl_flash_time;
static uint32_t lvgl_tick_cnt;

static int luat_lvg_handler(lua_State* L, void* ptr) {
//	DBG("%u", lv_tick_get());
	if (lvgl_tick_cnt) lvgl_tick_cnt--;
    lv_task_handler();
    return 0;
}

static void luat_lvgl_callback(void *param){
	if (lvgl_tick_cnt < 5)
	{
		lvgl_tick_cnt++;
	    rtos_msg_t msg = {0};
	    msg.handler = luat_lvg_handler;
	    luat_msgbus_put(&msg, 0);
	}
}

void luat_lvgl_tick_sleep(uint8_t OnOff)
{
	if (!OnOff)
	{
		luat_rtos_timer_start(lvgl_timer_handle, LVGL_TICK_PERIOD, true, luat_lvgl_callback, NULL);
	}
	else
	{
		luat_rtos_timer_stop(lvgl_timer_handle);
	}
}

#else
void luat_lvgl_tick_sleep(uint8_t OnOff)
{
	(void)OnOff;
}
#endif
extern int soc_mobile_get_default_pdp_part_info(uint8_t *ip_type, uint8_t *apn,uint8_t *apn_len, uint8_t *dns_num, ip_addr_t *dns_ip);

extern int soc_get_model_name(char *model, uint8_t is_full);

static void self_info(void)
{
	char temp[40] = {0};
	char imei[22] = {0};
	luat_mobile_get_imei(0, imei, 22);
	soc_get_model_name(temp, 1);
	DBG("model %s imei %s", temp, imei);

}

static void luatos_task(void *param)
{
#ifdef LUAT_UART0_FORCE_USER
	BSP_SetPlatConfigItemValue(PLAT_CONFIG_ITEM_LOG_PORT_SEL, PLAT_CFG_ULG_PORT_USB);
#endif
	(void)param;
	BSP_SetPlatConfigItemValue(PLAT_CONFIG_ITEM_FAULT_ACTION, EXCEP_OPTION_DUMP_FLASH_EPAT_RESET);
    if(BSP_GetPlatConfigItemValue(PLAT_CONFIG_ITEM_FAULT_ACTION) == EXCEP_OPTION_SILENT_RESET)
        ResetLockupCfg(true, true);
    else
        ResetLockupCfg(false, false);
	luat_heap_init();
    ShareInfoWakeupCP4Version();
	self_info();
#ifdef LUAT_USE_MEDIA
	luat_audio_global_init();
#endif

#ifdef LUAT_USE_LVGL
	g_lvgl_flash_time = 33;
    lv_init();
	luat_rtos_timer_create(&lvgl_timer_handle);

#ifdef __LVGL_SLEEP_ENABLE__
    luat_lvgl_tick_sleep(1);
#else
    luat_rtos_timer_start(lvgl_timer_handle, LVGL_TICK_PERIOD, true, luat_lvgl_callback, NULL);
#endif

#endif
	luat_pm_init();
	#ifdef LUAT_EC618_RNDIS_ENABLED
	DBG("RNDIS feature enabled");
	#endif

	luat_main();
	while (1) {
		DBG("LuatOS exit");
		luat_rtos_task_sleep(15000);
		luat_os_reboot(0);
	}
}

void luat_mobile_event_cb(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status, void* ptr);
void luat_sms_recv_cb(uint32_t event, void *param);

void soc_service_misc_callback(uint8_t *data, uint32_t len)
{
	uint8_t sg_id = (((len)>>12)&0x000F);
	uint16_t prim_id = ((len)&0x0FFF);
	PV_Union uPV;
	switch(sg_id)
	{
	case CAM_DEV:
		switch(prim_id)
		{
		case CMI_DEV_SET_WIFISCAN_CNF:
			luat_wlan_done_callback_ec618(data);
			break;
		}
		break;
	case CAM_SIM:
		switch (prim_id)
		{
		case CMI_SIM_SET_SIM_WRITE_COUNTER_CNF:
			memcpy(uPV.u8, data, 4);
			luat_mobile_event_cb(LUAT_MOBILE_EVENT_SIM, 0, LUAT_MOBILE_SIM_WC, (void*)uPV.u32);
			break;
		}
	}
}

static void luatos_mobile_event_callback(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status)
{
	if (LUAT_MOBILE_EVENT_NETIF == event)
	{
		if (LUAT_MOBILE_NETIF_LINK_ON == status)
		{
			ip_addr_t dns_ip[2];
			uint8_t type, dns_num;
			dns_num = 2;
			soc_mobile_get_default_pdp_part_info(&type, NULL, NULL, &dns_num, dns_ip);
			if (type & 0x80)
			{
				if (index != 4)
				{
					return;
				}
				else
				{
					NmAtiNetifInfo *pNetifInfo = malloc(sizeof(NmAtiNetifInfo));
					NetMgrGetNetInfo(0xff, pNetifInfo);
					if (pNetifInfo->ipv6Cid != 0xff)
					{
						net_lwip_set_local_ip6(&pNetifInfo->ipv6Info.ipv6Addr);

					}
					free(pNetifInfo);
				}
			}
			if (dns_num > 0)
			{
				network_set_dns_server(NW_ADAPTER_INDEX_LWIP_GPRS, 2, &dns_ip[0]);
				if (dns_num > 1)
				{
					network_set_dns_server(NW_ADAPTER_INDEX_LWIP_GPRS, 3, &dns_ip[1]);
				}
			}
			net_lwip_set_link_state(NW_ADAPTER_INDEX_LWIP_GPRS, 1);
			#ifdef LUAT_USE_NETDRV
			extern luat_netdrv_t netdrv_gprs;
			extern struct netif * net_lwip_get_netif(uint8_t adapter_index);
			netdrv_gprs.netif = net_lwip_get_netif(NW_ADAPTER_INDEX_LWIP_GPRS);
			#endif
		}
	}
	luat_mobile_event_cb(event, index, status, NULL);
}

static void luatos_task_init(void)
{
	GPIO_GlobalInit(NULL);
extern void soc_aon_gpio_save_state_enable(uint8_t on_off);
	soc_aon_gpio_save_state_enable(1);
// https://gitee.com/openLuat/LuatOS/issues/I6H86Q
#ifdef LUAT_UART0_FORCE_USER
	extern void soc_uart0_set_log_off(uint8_t is_off);
    soc_uart0_set_log_off(1);
#endif
#ifdef LUAT_UART0_FORCE_ALT1
	luat_uart_pre_setup(0, 1);
#endif
	luat_mobile_event_register_handler(luatos_mobile_event_callback);
	luat_sms_init();
	luat_sms_recv_msg_register_handler(luat_sms_recv_cb);
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
	#ifdef LUAT_USE_NETDRV
	extern void luat_napt_native_init(void);
	luat_napt_native_init();
	#endif
	luat_rtos_task_handle task_handle;
	luat_rtos_task_create(&task_handle, 16 * 1024, 80, "luatos", luatos_task, NULL, 32);

}
extern void luat_pm_preinit(void);
INIT_DRV_EXPORT(luat_pm_preinit, "1");
INIT_HW_EXPORT(luatos_task_init, "1");


void soc_get_unilog_br(uint32_t *baudrate)
{
#ifdef LUAT_UART0_LOG_BR_12M
	*baudrate = 12000000; //UART0做log口输出12M波特率，必须用高性能USB转TTL
#else
	*baudrate = 6000000; //UART0做log口输出6M波特率，必须用高性能USB转TTL
#endif
}

#ifdef LUAT_UART0_FORCE_ALT1
extern int32_t soc_unilog_callback(void *pdata, void *param);
bool soc_init_unilog_uart(uint8_t port, uint32_t baudrate, bool startRecv)
{
	soc_get_unilog_br(&baudrate);
	GPIO_IomuxEC618(GPIO_ToPadEC618(HAL_GPIO_16, 0), 3, 0, 0);
	GPIO_IomuxEC618(GPIO_ToPadEC618(HAL_GPIO_17, 0), 3, 0, 0);
	GPIO_PullConfig(GPIO_ToPadEC618(HAL_GPIO_16, 0), 1, 1);
	GPIO_IomuxEC618(GPIO_ToPadEC618(HAL_GPIO_14, 0), 0, 0, 0);	//原先的UART0 TXRX变回GPIO功能
	GPIO_IomuxEC618(GPIO_ToPadEC618(HAL_GPIO_15, 0), 0, 0, 0);
	Uart_BaseInitEx(port, baudrate, 0, 256, UART_DATA_BIT8, UART_PARITY_NONE, UART_STOP_BIT1, soc_unilog_callback);
	return true;
}
#endif
