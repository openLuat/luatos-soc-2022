/**
 * 本demo验证了HTTP下整包升级的可行性
 *
 */
#include "common_api.h"
#include "luat_rtos.h"
#include "luat_debug.h"
#include "luat_mobile.h"
#include "luat_network_adapter.h"
#include "luat_full_ota.h"
#include "networkmgr.h"
#include "luat_http.h"
luat_rtos_task_handle g_s_task_handle;

enum
{
	OTA_HTTP_GET_HEAD_DONE = 1,
	OTA_HTTP_GET_DATA,
	OTA_HTTP_GET_DATA_DONE,
	OTA_HTTP_FAILED,
};

static void luatos_mobile_event_callback(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status)
{
	if (LUAT_MOBILE_EVENT_NETIF == event)
	{
		if (LUAT_MOBILE_NETIF_LINK_ON == status)
		{
			luat_socket_check_ready(index, NULL);
		}
	}
}

static void luatos_http_cb(int status, void *data, uint32_t len, void *param)
{
	uint8_t *ota_data;
	if (status < 0)
	{
		LUAT_DEBUG_PRINT("http failed! %d", status);
		luat_rtos_event_send(param, OTA_HTTP_FAILED, 0, 0, 0, 0);
		return;
	}
	switch(status)
	{
	case HTTP_STATE_GET_BODY:
		if (data)
		{
			ota_data = malloc(len);
			memcpy(ota_data, data, len);
			luat_rtos_event_send(param, OTA_HTTP_GET_DATA, ota_data, len, 0, 0);
		}
		else
		{
			luat_rtos_event_send(param, OTA_HTTP_GET_DATA_DONE, 0, 0, 0, 0);
		}
		break;
	case HTTP_STATE_GET_HEAD:
		if (data)
		{
			LUAT_DEBUG_PRINT("%s", data);
		}
		else
		{
			luat_rtos_event_send(param, OTA_HTTP_GET_HEAD_DONE, 0, 0, 0, 0);
		}
		break;
	case HTTP_STATE_IDLE:
		break;
	case HTTP_STATE_SEND_BODY_START:
		//如果是POST，在这里发送POST的body数据，如果一次发送不完，可以在HTTP_STATE_SEND_BODY回调里继续发送
		break;
	case HTTP_STATE_SEND_BODY:
		//如果是POST，可以在这里发送POST剩余的body数据
		break;
	default:
		break;
	}
}

#define PROJECT_VERSION  "1.0.0"                  //使用合宙iot升级的话此字段必须存在，并且强制固定格式为x.x.x, x可以为任意的数字
#define PROJECT_KEY "AABBCCDDEEFF"  //修改为自己iot上面的PRODUCT_KEY，这里是一个错误的，使用合宙iot升级的话此字段必须存在
#define PROJECT_NAME "example_full_ota"                  //使用合宙iot升级的话此字段必须存在，可以任意修改，但和升级包的必须一致


static void luat_test_task(void *param)
{
	char version[20] = {0};
	luat_event_t event;
	int result, is_error;
	/* 
		出现异常后默认为死机重启
		demo这里设置为LUAT_DEBUG_FAULT_HANG_RESET出现异常后尝试上传死机信息给PC工具，上传成功或者超时后重启
		如果为了方便调试，可以设置为LUAT_DEBUG_FAULT_HANG，出现异常后死机不重启
		但量产出货一定要设置为出现异常重启！！！！！！！！！1
	*/
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG);
	uint32_t all,now_free_block,min_free_block,done_len;
	luat_http_ctrl_t *http = luat_http_client_create(luatos_http_cb, luat_rtos_get_current_handle(), -1);
	luat_full_ota_ctrl_t *fota = luat_full_ota_init(0, 0, NULL, NULL, 0);
	//自建服务器，就随意命名了
	const char remote_domain[] = "http://www.air32.cn/update.bin";
	//如果用合宙IOT服务器，需要按照IOT平台规则创建好相应的
#if 0
	const char remote_domain[200];
    char imei[16] = {0};
    luat_mobile_get_imei(0, imei, 15);
	snprintf_(remote_domain, 200, "http://iot.openluat.com/api/site/firmware_upgrade?project_key=%s&imei=%s&device_key=&firmware_name=%s_LuatOS_CSDK_EC618&version=%s", PROJECT_KEY, imei, PROJECT_NAME, PROJECT_VERSION);
#endif
	luat_http_client_start(http, remote_domain, 0, 0, 1);
	while (1)
	{
		luat_rtos_event_recv(g_s_task_handle, 0, &event, NULL, LUAT_WAIT_FOREVER);
		switch(event.id)
		{
		case OTA_HTTP_GET_HEAD_DONE:
			done_len = 0;
			DBG("status %d total %u", luat_http_client_get_status_code(http), http->total_len);
			break;
		case OTA_HTTP_GET_DATA:
			//对下载速度进行控制，如果下载速度过快，会导致ram耗尽出错
			luat_meminfo_sys(&all, &now_free_block, &min_free_block);
			if ((all - now_free_block) < 120 * 1024 )
			{
				if (!http->is_pause)
				{
					luat_http_client_pause(http, 1);
				}
				LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
			}
			else if ((all - now_free_block) > 180 * 1024 )
			{
				if (http->is_pause)
				{
					luat_http_client_pause(http, 0);
				}
			}
//			LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
			done_len += event.param2;
			result = luat_full_ota_write(fota, event.param1, event.param2);
			free(event.param1);
			break;
		case OTA_HTTP_GET_DATA_DONE:
			is_error = luat_full_ota_is_done(fota);
			if (is_error)
			{
				goto OTA_DOWNLOAD_END;
			}
			else
			{
				luat_pm_reboot();
			}
			break;
		case OTA_HTTP_FAILED:
			break;
		}
	}

OTA_DOWNLOAD_END:
	LUAT_DEBUG_PRINT("full ota 测试失败");
	free(fota);
	luat_http_client_close(http);
	luat_http_client_destroy(&http);
	luat_meminfo_sys(&all, &now_free_block, &min_free_block);
	LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
	while(1)
	{
		luat_rtos_task_sleep(60000);
	}

}

static void luat_test_init(void)
{
	luat_mobile_event_register_handler(luatos_mobile_event_callback);
	luat_mobile_set_period_work(0, 5000, 0);
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
	luat_rtos_task_create(&g_s_task_handle, 4 * 1024, 50, "test", luat_test_task, NULL, 16);

}

INIT_TASK_EXPORT(luat_test_init, "1");
