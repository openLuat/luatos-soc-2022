#include "luat_network_adapter.h"
#include "common_api.h"
#include "luat_debug.h"

#include "luat_rtos.h"
#include "luat_mobile.h"
#include "networkmgr.h"

#include "luat_mem.h"

#include "cJSON.h"
#include "andlink.h"

/* 修改下面参数可以快速验证,更多参数自定义需往下详细阅读demo */
#define PRODUCT_ID           "XXX"
#define ANDLINK_TOKEN        "XXX"
#define PRODUCT_TOKEN        "XXX"
#define IMEI        		 "XXX"
#define SN 					 "XXX"

#define ANDLINK_QUEUE_SIZE 	 32
static luat_is_link_up = 0;
static luat_rtos_task_handle andlink_init_task_handle;
static luat_rtos_task_handle andlink_task_handle;
static luat_rtos_queue_t andlink_queue_handle;

// 通知设备状态
int andlink_set_led_callback(ADL_DEV_STATE_e state){
	return luat_rtos_queue_send(andlink_queue_handle, &state, NULL, 0);
}

// 下行管控
int andlink_dn_send_cmd_callback(RESP_MODE_e mode, dn_dev_ctrl_frame_t *ctrlFrame, char *eventType, char *respData, int respBufSize){

	return 0;
}

// 获取设备IP
int andlink_get_device_ipaddr(char *ip, char *broadAddr){
	char *ipaddr;
	ip_addr_t ipv4;
	ip_addr_t ipv6;
	while (luat_is_link_up == 0){
		luat_rtos_task_sleep(100);
	}
	luat_mobile_get_local_ip(0, 1, &ipv4, &ipv6);
	if (ipv4.type != 0xff){
		ipaddr = ip4addr_ntoa(&ipv4.u_addr.ip4);
	}
	if (ipv6.type != 0xff){
		ipaddr = ip4addr_ntoa(&ipv4.u_addr.ip6);
	}
	memcpy(ip, ipaddr, strlen(ipaddr));
	return 0;
}


static void luat_andlink_init_task(void *param)
{
	luat_rtos_task_sleep(2000);
	cJSON* chips_json;
	cJSON* extInfo_json = cJSON_CreateObject();
	cJSON_AddStringToObject(extInfo_json, "cmei", IMEI);				// 设备唯一标识,必选
	cJSON_AddNumberToObject(extInfo_json, "authMode", 0);				// 0表示类型认证，1表示设备认证，设备认证时，需使用authId和authKey
	cJSON_AddStringToObject(extInfo_json, "sn", SN);					// 设备SN，必选
	cJSON_AddStringToObject(extInfo_json, "manuDate", "2023-07");		// 设备生产日期，格式为年-月
	cJSON_AddStringToObject(extInfo_json, "OS", "Freertos");			// 操作系统
	cJSON* chips_array = cJSON_CreateArray();
	cJSON_AddItemToArray(chips_array,chips_json = cJSON_CreateObject());
	cJSON_AddItemToObject(extInfo_json, "chips", chips_array);			// 芯片信息,可包含多组
	cJSON_AddStringToObject(chips_json, "type", "4G");					// 芯片类型，如Main/WiFi/Zigbee/BLE等
	cJSON_AddStringToObject(chips_json, "factory", "airm2m");			// 芯片厂商
	cJSON_AddStringToObject(chips_json, "model", "air780");				// 芯片型号
	char* extInfo = cJSON_Print(extInfo_json);
	cJSON_Delete(extInfo_json);
	adl_dev_attr_t devAttr = {
		.cfgNetMode = NETWOKR_MODE_4G,
		.deviceType = PRODUCT_ID,
		.deviceMac =  IMEI,
		.andlinkToken = ANDLINK_TOKEN,
		.productToken = PRODUCT_TOKEN,
		.firmWareVersion = "0.0.1",
		.softWareVersion = "0.0.1",
		.extInfo = extInfo,
	};
	adl_dev_callback_t devCbs = {
		.set_led_callback = andlink_set_led_callback,
		.dn_send_cmd_callback = andlink_dn_send_cmd_callback,
		.get_device_ipaddr = andlink_get_device_ipaddr,
	};
	andlink_init(&devAttr, &devCbs);
	cJSON_free(extInfo);
	luat_rtos_task_delete(andlink_init_task_handle);
}

static void luat_andlink_task(void *param){
	ADL_DEV_STATE_e andlink_state;
	while (1){
        if (luat_rtos_queue_recv(andlink_queue_handle, &andlink_state, NULL, 5000) == 0){
			switch (andlink_state)
			{
			case ADL_BOOTSTRAP:
				LUAT_DEBUG_PRINT("设备开始注册状态");
				break;
			case ADL_BOOTSTRAP_SUC:
				LUAT_DEBUG_PRINT("设备注册成功状态");
				break;
			case ADL_BOOTSTRAP_FAIL:
				LUAT_DEBUG_PRINT("设备注册失败状态");
				break;
			case ADL_BOOT:
				LUAT_DEBUG_PRINT("设备开始上线状态");
				break;
			case ADL_BOOT_SUC:
				LUAT_DEBUG_PRINT("设备上线成功状态");
				break;
			case ADL_BOOT_FAIL:
				LUAT_DEBUG_PRINT("设备上线失败状态");
				break;
			case ADL_ONLINE:
				LUAT_DEBUG_PRINT("设备在线状态");
				break;
			case ADL_RESET:
				LUAT_DEBUG_PRINT("设备复位状态");
				break;
			case ADL_OFFLINE:
				LUAT_DEBUG_PRINT("设备离线状态");
				break;
			}
        }
	}
}


static void luatos_mobile_event_callback(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status)
{
	if (LUAT_MOBILE_EVENT_NETIF == event)
	{
		if (LUAT_MOBILE_NETIF_LINK_ON == status)
		{
			luat_is_link_up = 1;
			luat_socket_check_ready(index, NULL);
		}
        else if(LUAT_MOBILE_NETIF_LINK_OFF == status || LUAT_MOBILE_NETIF_LINK_OOS == status)
        {
            luat_is_link_up = 0;
        }
	}
}

static void luat_libandlink_init(void)
{
	luat_mobile_event_register_handler(luatos_mobile_event_callback);
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
	luat_rtos_queue_create(&andlink_queue_handle, ANDLINK_QUEUE_SIZE, sizeof(ADL_DEV_STATE_e));
	luat_rtos_task_create(&andlink_init_task_handle, 4 * 1024, 10, "andlink_init", luat_andlink_init_task, NULL, 16);
	luat_rtos_task_create(&andlink_task_handle, 4 * 1024, 10, "andlink", luat_andlink_task, NULL, 16);
}

INIT_TASK_EXPORT(luat_libandlink_init, "1");
