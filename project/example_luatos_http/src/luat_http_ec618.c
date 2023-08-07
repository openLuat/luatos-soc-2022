#include "common_api.h"
#include "luat_rtos.h"
#include "luat_gpio.h"
#include "luat_debug.h"
#include "luat_mobile.h"
#include "luat_network_adapter.h"
#include "networkmgr.h"
#include "luat_http.h"
#include "text_1024k.h"

/* 
	此demo演示的功能为，上传1m数据到服务器

	默认的上传文件名为“设备imei_test.txt”
	上传的文件可至http://tools.openluat.com/tools/device-upload-test界面查看
*/

static luat_http_ctrl_t *g_s_http_client;
static luat_rtos_task_handle g_s_task_handle;

enum
{
    HTTP_DEMO_GET_HEAD_DONE = 1,
    HTTP_DEMO_SEND_BODY_START,
    HTTP_DEMO_SEND_BODY,
	HTTP_DEMO_GET_BODY_DONE,
    HTTP_DEMO_FAILED,
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

static void luatos_http_cb(int status, void *data, uint32_t data_len, void *param)
{
	LUAT_DEBUG_PRINT("this is http status %d", status);
    if(status < 0) 
    {
        LUAT_DEBUG_PRINT("http failed! %d", status);
		luat_rtos_event_send(param, HTTP_DEMO_FAILED, 0, 0, 0, 0);
		return;
    }
	switch(status)
	{
	case HTTP_STATE_GET_BODY:
		if (data)
		{
		}
		else
		{
			luat_rtos_event_send(param, HTTP_DEMO_GET_BODY_DONE, 0, 0, 0, 0);
		}
		break;
	case HTTP_STATE_GET_HEAD:
		if (data)
		{
			LUAT_DEBUG_PRINT("%s", data);
		}
		else
		{
			luat_rtos_event_send(param, HTTP_DEMO_GET_HEAD_DONE, 0, 0, 0, 0);
		}
		break;
	case HTTP_STATE_IDLE:
		break;
	case HTTP_STATE_SEND_BODY_START:
		//如果是POST，在这里发送POST的body数据，如果一次发送不完，可以在HTTP_STATE_SEND_BODY回调里继续发送
        luat_rtos_event_send(param, HTTP_DEMO_SEND_BODY_START, 0, 0, 0, 0);
		break;
	case HTTP_STATE_SEND_BODY:
		//如果是POST，可以在这里发送POST剩余的body数据
        luat_rtos_event_send(param, HTTP_DEMO_SEND_BODY, 0, 0, 0, 0);
		break;
	default:
		break;
	}
}


static void luat_test_task(void *param)
{
	luat_event_t event;
	/* 
		出现异常后默认为死机重启
		demo这里设置为LUAT_DEBUG_FAULT_HANG_RESET出现异常后尝试上传死机信息给PC工具，上传成功或者超时后重启
		如果为了方便调试，可以设置为LUAT_DEBUG_FAULT_HANG，出现异常后死机不重启
		但量产出货一定要设置为出现异常重启！！！！！！！！！1
	*/
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG_RESET); 

	int get_imei_result = 0;

	uint8_t is_close = 0;
	uint8_t is_final_data = 0;
	uint32_t post_buf_len = 0;
	uint32_t wait_send_len = 0;
	uint32_t data_total_len = sizeof(text_1024k_array);
	uint32_t all,now_free_block,min_free_block;

	uint64_t tick;

	time_t nowtime;

	char boundary[100] = {0};
	char con_type[140] = {0};
	char body_head_buf[200] = {0};
	char body_tail_buf[200] = {0};
	char total_len[16] = {0};
	char imei[16] = {0};

	const char remote_domain[] = "http://tools.openluat.com/api/site/device_upload_file";

    g_s_http_client = luat_http_client_create(luatos_http_cb, luat_rtos_get_current_handle(), -1);
	luat_http_client_base_config(g_s_http_client, 300000, 0, 3);
	g_s_http_client->debug_onoff = 1;

	time(&nowtime);
	tick = luat_mcu_tick64_ms();
	snprintf(boundary, 100, "--------------------------%lld%lld", nowtime, tick);
	snprintf(con_type, 140,"multipart/form-data; boundary=%s", boundary);
	luat_http_client_set_user_head(g_s_http_client, "Content-Type", con_type);

	get_imei_result = luat_mobile_get_imei(0, imei, 16);

	if(get_imei_result > 0)
	{
		post_buf_len += snprintf(body_head_buf, 200, "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s_%s\"\r\n\r\n", boundary, imei, "test.txt");
	}
	else
	{
		post_buf_len += snprintf(body_head_buf, 200, "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n\r\n", boundary, "unknown_device_test.txt");
	}
	post_buf_len += data_total_len;
	post_buf_len += snprintf(body_tail_buf, 200, "\r\n--%s--\r\n", boundary);
	itoa(post_buf_len, total_len, 10);

	luat_http_client_set_user_head(g_s_http_client, "Content-Length", total_len);
	luat_http_client_start(g_s_http_client, remote_domain, 1, 0, 0);


    while(!is_close)
	{
		luat_rtos_event_recv(g_s_task_handle, 0, &event, NULL, LUAT_WAIT_FOREVER);
		switch(event.id)
		{
		case HTTP_DEMO_GET_HEAD_DONE:
			LUAT_DEBUG_PRINT("status %d total %u", luat_http_client_get_status_code(g_s_http_client), g_s_http_client->total_len);
			is_close = 1;
			break;
		case HTTP_DEMO_SEND_BODY_START:
			luat_http_client_post_body(g_s_http_client, body_head_buf, strlen(body_head_buf));
			break;
		case HTTP_DEMO_SEND_BODY:
			if (wait_send_len < data_total_len)
			{
				if ((wait_send_len + 32 * 1024) < data_total_len)
				{
					LUAT_DEBUG_PRINT("this is test send len %d", luat_http_client_post_body(g_s_http_client, text_1024k_array + wait_send_len, 32 * 1024));
				}
				else
				{
					LUAT_DEBUG_PRINT("this is test send len %d", luat_http_client_post_body(g_s_http_client, text_1024k_array + wait_send_len, data_total_len - wait_send_len));
				}
				wait_send_len += 32 * 1024;
			}
			else
			{
				if(!is_final_data)
				{
					is_final_data = 1;
					LUAT_DEBUG_PRINT("this is test send len %d", luat_http_client_post_body(g_s_http_client, body_tail_buf, strlen(body_tail_buf)));
				}
			}
			break;
		case HTTP_DEMO_FAILED:
			is_close = 1;
			break;
		case HTTP_DEMO_GET_BODY_DONE:
			is_close = 1;
			break;
		}
	}
	luat_http_client_close(g_s_http_client);
	while(1)
	{
		luat_rtos_task_sleep(1000);
		luat_meminfo_sys(&all, &now_free_block, &min_free_block);
		LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
	}
	luat_rtos_task_delete(g_s_task_handle);
}

static void luat_test_init(void)
{
	luat_mobile_event_register_handler(luatos_mobile_event_callback);
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
	luat_rtos_task_create(&g_s_task_handle, 4 * 1024, 50, "test", luat_test_task, NULL, 16);

}

INIT_TASK_EXPORT(luat_test_init, "1");
