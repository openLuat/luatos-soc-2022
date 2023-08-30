#include "common_api.h"
#include "luat_rtos.h"
#include "luat_gpio.h"
#include "luat_debug.h"
#include "luat_mobile.h"
#include "luat_network_adapter.h"
#include "networkmgr.h"
#include "luat_http.h"
#include "luat_fota.h"

/*
FOTA应用开发可以参考：https://doc.openluat.com/wiki/37?wiki_page_id=4727
*/


luat_fota_img_proc_ctx_ptr test_luat_fota_handle = NULL;

#define IOT_FOTA_URL "http://iot.openluat.com"
#define PROJECT_VERSION  "1.0.1"                  			//使用合宙iot升级的话此字段必须存在，并且强制固定格式为x.x.x, x可以为任意的数字
#define PROJECT_KEY "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  			//修改为自己iot上面的PRODUCT_KEY，这里是一个错误的，使用合宙iot升级的话此字段必须存在
#define PROJECT_NAME "TEST_FOTA"                  			//使用合宙iot升级的话此字段必须存在，可以任意修改，但和升级包的必须一致

/*
    注意事项！！！！！！！！！！！！！

    重要说明！！！！！！！！！！！！！！
    设备如果升级的话，设备运行的版本必须和差分文件时所选的旧版固件一致
    举例，用1.0.0和3.0.0差分生成的差分包，必须也只能给运行1.0.0软件的设备用
*/

/* 
    第一种升级方式 ！！！！！！！！！！
    这种方式通过设备自身上报的版本名称修改，不需要特别对设备和升级包做版本管理                  用合宙iot平台升级时，推荐使用此种升级方式

    PROJECT_VERSION:用于区分不同软件版本，同时也用于区分固件差分基础版本

    假设：
        现在有两批模块在运行不同的基础版本，一个版本号为1.0.0，另一个版本号为2.0.0
        现在这两个版本都需要升级到3.0.0，则需要做两个差分包，一个是从1.0.0升级到3.0.0，另一个是从2.0.0升级到3.0.0
        
        但是因为合宙IOT是通过firmware来区分不同版本固件，只要请求时firmware相同，版本号比设备运行的要高，就会下发升级文件

        所以需要对firmware字段做一点小小的操作，将PROJECT_VERSION加入字段中来区分基础版本不同的差分包

        添加字段前的fireware字段
                从1.0.0升级到3.0.0生成的firmware字段为TEST_FOTA_CSDK_EC618
                从2.0.0升级到3.0.0生成的firmware字段为TEST_FOTA_CSDK_EC618

        添加字段后的fireware字段
                从1.0.0升级到3.0.0生成的firmware字段为1.0.0_TEST_FOTA_CSDK_EC618
                从2.0.0升级到3.0.0生成的firmware字段为2.0.0_TEST_FOTA_CSDK_EC618

        这样操作后，当两个升级包放上去，1.0.0就算被放进了2.0.0_TEST_FOTA_CSDK_EC618的升级列表里，也会因为自身上报的字段和所在升级列表的固件名称不一致而被服务器拒绝升级
*/



/* 
    第二种升级方式 ！！！！！！！！！！
    这种方式可以在合宙iot平台统计升级成功的设备数量，但是需要用户自身对设备和升级包做版本管理     用合宙iot平台升级时，不推荐使用此种升级方式

    PROJECT_VERSION:用于区分不同软件版本

    假设：
        现在有两批模块在运行不同的基础版本，一个版本号为1.0.0，另一个版本号为2.0.0
        现在这两个版本都需要升级到3.0.0，则需要做两个差分包，一个是从1.0.0升级到3.0.0，另一个是从2.0.0升级到3.0.0

        合宙IOT通过firmware来区分不同版本固件，只要请求时firmware相同，版本号比设备运行的要高，就会下发升级文件
        
        从1.0.0升级到3.0.0生成的firmware字段为TEST_FOTA_CSDK_EC618
        从2.0.0升级到3.0.0生成的firmware字段为TEST_FOTA_CSDK_EC618

        如果将运行1.0.0的设备imei放进了2.0.0-3.0.0的升级列表中，因为设备上报的firmware字段相同，服务器也会将2.0.0-3.0.0的差分软件下发给运行1.0.0软件的设备
        所以用户必须自身做好设备版本区分，否则会一直请求错误的差分包，导致流量损耗
*/


/* 
    第二种升级方式相对于第一种方式，多了一个合宙iot平台统计升级成功设备数量的功能，但需要用户自身做好设备、软件版本管理
    
    总体来说弊大于利

    推荐使用第一种方式进行升级，此demo也默认使用第一种方式进行升级演示！！！！！！
*/

static luat_rtos_task_handle g_s_task_handle;

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
    if(status < 0) 
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


static void luat_test_task(void *param)
{
	luat_event_t event;
	int result, is_error;
	/* 
		出现异常后默认为死机重启
		demo这里设置为LUAT_DEBUG_FAULT_HANG_RESET出现异常后尝试上传死机信息给PC工具，上传成功或者超时后重启
		如果为了方便调试，可以设置为LUAT_DEBUG_FAULT_HANG，出现异常后死机不重启
		但量产出货一定要设置为出现异常重启！！！！！！！！！1
	*/
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG_RESET);
	uint32_t all,now_free_block,min_free_block,done_len;
	luat_http_ctrl_t *http = luat_http_client_create(luatos_http_cb, luat_rtos_get_current_handle(), -1);
	const char remote_domain[200];
    char imei[16] = {0};
    luat_mobile_get_imei(0, imei, 15);
    // 第一种升级方式
    snprintf(remote_domain, 200, "%s/api/site/firmware_upgrade?project_key=%s&imei=%s&device_key=&firmware_name=%s_%s_%s_%s&version=%s", IOT_FOTA_URL, PROJECT_KEY, imei, PROJECT_VERSION, PROJECT_NAME, soc_get_sdk_type(), "EC618", PROJECT_VERSION);

    // 第二种升级方式
    // snprintf(remote_domain, 200, "%s/api/site/firmware_upgrade?project_key=%s&imei=%s&device_key=&firmware_name=%s_%s_%s&version=%s", IOT_FOTA_URL, PROJECT_KEY, imei, PROJECT_NAME, soc_get_sdk_type(), "EC618", PROJECT_VERSION);
    LUAT_DEBUG_PRINT("print url %s", remote_domain);


	test_luat_fota_handle = luat_fota_init();
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
			done_len += event.param2;
			result = luat_fota_write(test_luat_fota_handle, event.param1, event.param2);
			free(event.param1);
			break;
		case OTA_HTTP_GET_DATA_DONE:
			is_error = luat_fota_done(test_luat_fota_handle);
            if(is_error != 0)
            {
                LUAT_DEBUG_PRINT("image_verify error");
                goto OTA_DOWNLOAD_END;
            }
			else
			{
				LUAT_DEBUG_PRINT("image_verify ok");
            	luat_pm_reboot();
			}
			break;
		case OTA_HTTP_FAILED:
			break;
		}
	}

OTA_DOWNLOAD_END:
	LUAT_DEBUG_PRINT("ota 测试失败");
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
