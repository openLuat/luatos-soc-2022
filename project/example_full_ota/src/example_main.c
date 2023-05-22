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
luat_rtos_task_handle g_s_task_handle;
int get_http_response_code( uint8_t *http_buf )
{
	int response_code=0;
    char * p_start = NULL;
    char* p_end =NULL;
    char re_code[10] ={0};
    memset(re_code,0,sizeof(re_code));

    p_start = (char*)strstr((const char*)http_buf, (char*)" " );
    if(NULL == p_start)
    {
        return -1;
    }
    p_end = (char*)strstr( (const char*)++p_start, (char*)" " );
    if(p_end)
    {
        if(p_end - p_start > sizeof(re_code))
        {
            return -1;
        }
        memcpy( re_code,p_start,(p_end-p_start) );
    }

    response_code = strtoul((char*)re_code, NULL, 10);
    return response_code;
}

int get_http_response_head_len( uint8_t *http_buf )
{
   char *p_start = NULL;
   char *p_end =NULL;
   int32_t headlen=0;
   p_start = (char *)http_buf;
   p_end = (char *)strstr( (char *)http_buf, "\r\n\r\n");
   if( p_end==NULL )
   {
       return -1;
   }
   p_end=p_end + 4;
   headlen = ((uint32_t)p_end-(uint32_t)p_start);
   return headlen;
}

int get_http_response_range( uint8_t *http_buf, uint32_t *range_start,  uint32_t *range_end, uint32_t *total_len)
{
   char *p_start = NULL;
   char *p_end =NULL;
   char Temp[12];
   p_start = (char *)strstr( (char *)http_buf,"Content-Range: bytes ");
   if( p_start==NULL ) return -1;
   p_start = p_start+strlen("Content-Range: bytes ");
   p_end = (char *)strchr((char*)p_start, '-');
   if( p_end==NULL )   return -1;
   memset(Temp, 0, 12);
   memcpy(Temp, p_start, (uint32_t)p_end - (uint32_t)p_start);
   *range_start = strtoul(Temp, NULL, 10);

   p_start = p_end + 1;
   p_end = (char *)strchr((char*)p_start, '/');
   if( p_end==NULL )   return -1;
   memset(Temp, 0, 12);
   memcpy(Temp, p_start, (uint32_t)p_end - (uint32_t)p_start);
   *range_end = strtoul(Temp, NULL, 10);

   p_start = p_end + 1;
   p_end = (char *)strstr((char*)p_start, "\r\n");
   if( p_end==NULL )   return -1;
   memset(Temp, 0, 12);
   memcpy(Temp, p_start, (uint32_t)p_end - (uint32_t)p_start);
   *total_len = strtoul(Temp, NULL, 10);
   return 0;
}


static int32_t luat_test_socket_callback(void *pdata, void *param)
{
	OS_EVENT *event = (OS_EVENT *)pdata;
//	LUAT_DEBUG_PRINT("%x", event->ID);
	return 0;
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
		}
	}
}


#define PROJECT_VERSION  "1.0.0"                  //使用合宙iot升级的话此字段必须存在，并且强制固定格式为x.x.x, x可以为任意的数字
#define PROJECT_KEY "AABBCCDDEEDDFFF110123"  //修改为自己iot上面的PRODUCT_KEY，这里是一个错误的，使用合宙iot升级的话此字段必须存在
#define PROJECT_NAME "example_full_ota"                  //使用合宙iot升级的话此字段必须存在，可以任意修改，但和升级包的必须一致


static void luat_test_task(void *param)
{
	char version[20] = {0};
	luat_event_t event;
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG);
	uint32_t all,now_free_block,min_free_block;
	network_ctrl_t *netc = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(netc, g_s_task_handle, luat_test_socket_callback, g_s_task_handle);
	network_set_base_mode(netc, 1, 15000, 0, 0, 0, 0);	//http基于TCP
	netc->is_debug = 0;
	luat_full_ota_ctrl_t *fota = luat_full_ota_init(0, 0, NULL, NULL, 0);
	//自建服务器，就随意命名了
	const char remote_domain[] = "www.air32.cn";
	const char ota_file_name[] = "update.bin";
	//如果用合宙IOT服务器，需要按照IOT平台规则创建好相应的
#if 0
	const char remote_domain[] = "iot.openluat.com";
	const char ota_file_name[200];
    char imei[16] = {0};
    luat_mobile_get_imei(0, imei, 15);
	snprintf_(ota_file_name, 200, "api/site/firmware_upgrade?project_key=%s&imei=%s&device_key=&firmware_name=%s_%s_LuatOS_CSDK_EC618&version=%s", PROJECT_KEY, imei, PROJECT_VERSION, PROJECT_NAME, PROJECT_VERSION);
#endif
	int port = 80;
	uint32_t dummy_len, start, end, total, i, download_len;
	int tx_len = 0;
	uint8_t *tx_data = malloc(1024);
	Buffer_Struct rx_buffer = {0};
	OS_InitBuffer(&rx_buffer, 8 * 1024);
	int result, http_response, head_len;
	uint8_t is_break,is_timeout, is_error, is_head_ok;
	const char head[] = "GET /%s HTTP/1.1\r\nHost: %s:%d\r\nRange: bytes=%u-\r\nAccept: application/octet-stream\r\n\r\n";
	start = 0;
	is_head_ok = 0;
	total = 0xffffffff;
	end = 0xffffffff;
	download_len = 0;
	while(download_len < total)
	{
RESTART:
		LUAT_DEBUG_PRINT("full ota 文件当前下载%u->%u", download_len, end);
		luat_meminfo_sys(&all, &now_free_block, &min_free_block);
		LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
		rx_buffer.Pos = 0;
		result = network_connect(netc, remote_domain, sizeof(remote_domain) - 1, NULL, port, 30000);
		//发送HTTP请求头
		tx_len = sprintf_(tx_data, head, ota_file_name, remote_domain, port, download_len);
		if (result)
		{
			LUAT_DEBUG_PRINT("服务器连不上");
			goto OTA_DOWNLOAD_END;
		}
		result = network_tx(netc, tx_data, tx_len, 0, NULL, 0, &dummy_len, 15000);
		if (result)
		{
			LUAT_DEBUG_PRINT("HTTP请求发送失败");
			goto OTA_DOWNLOAD_END;
		}
		result = network_wait_rx(netc, 20000, &is_break, &is_timeout);
		if (result || is_timeout)
		{
			LUAT_DEBUG_PRINT("HTTP请求没有响应");
			goto OTA_DOWNLOAD_END;
		}
		result = network_rx(netc, rx_buffer.Data, 1024 * 4, 0, NULL, NULL, &dummy_len);
		if (result)
		{
			LUAT_DEBUG_PRINT("网络错误");
			goto OTA_DOWNLOAD_END;
		}
		rx_buffer.Pos = dummy_len;
		rx_buffer.Data[dummy_len] = 0;
		http_response = get_http_response_code(rx_buffer.Data);
		if (http_response != 206)
		{
			LUAT_DEBUG_PRINT("HTTP响应码错误,%d", http_response);
			goto OTA_DOWNLOAD_END;
		}
		if (get_http_response_range(rx_buffer.Data, &start, &end, &total) < 0)
		{
			LUAT_DEBUG_PRINT("文件长度解析错误");
			goto OTA_DOWNLOAD_END;
		}
		head_len = get_http_response_head_len(rx_buffer.Data);
		if (head_len < 0)
		{
			LUAT_DEBUG_PRINT("HTTP头没有正确结束");
			goto OTA_DOWNLOAD_END;
		}
		OS_BufferRemove(&rx_buffer, head_len);
		if (rx_buffer.Pos)
		{
			result = luat_full_ota_write(fota, rx_buffer.Data, rx_buffer.Pos);
			if (result < 0)
			{
				LUAT_DEBUG_PRINT("full ota 文件写入失败");
				goto OTA_DOWNLOAD_END;
			}
			download_len += rx_buffer.Pos;
			LUAT_DEBUG_PRINT("写入 %u,%u", rx_buffer.Pos, download_len);
			if (download_len >= total)
			{
				is_error = luat_full_ota_is_done(fota);
				if (is_error)
				{
					goto OTA_DOWNLOAD_END;
				}
				else
				{
					luat_pm_reboot();
				}
			}
		}
		if (!is_head_ok)
		{
			if (OTA_STATE_WRITE_COMMON_DATA == fota->ota_state)
			{
				is_head_ok = 1;
				memcpy(version, fota->fota_file_head.MainVersion, 16);
				LUAT_DEBUG_PRINT("full ota 用户标识 %s", version);
				//用户标识有16个字节空间，可以存放版本号来作为升级控制
			}
		}
WAIT_DATA:
		dummy_len = 0;
		result = network_rx(netc, NULL, 0, 0, NULL, NULL, &dummy_len);
		if (dummy_len > 0)
		{
			;
		}
		else
		{
			result = network_wait_rx(netc, 5000, &is_break, &is_timeout);
			if (result || is_timeout)
			{
				network_close(netc, 5000);
				LUAT_DEBUG_PRINT("下载中断，进行断点补传");
				goto RESTART;
			}
		}
		do
		{
			result = network_rx(netc, rx_buffer.Data, rx_buffer.MaxLen, 0, NULL, NULL, &dummy_len);
			if (dummy_len > 0)
			{
				result = luat_full_ota_write(fota, rx_buffer.Data, dummy_len);
				if (result < 0)
				{
					LUAT_DEBUG_PRINT("full ota 文件写入失败");
					goto OTA_DOWNLOAD_END;
				}
				download_len += dummy_len;
				LUAT_DEBUG_PRINT("写入 %u,%u", dummy_len, download_len);

				if (download_len >= total)
				{
					is_error = luat_full_ota_is_done(fota);
					if (is_error)
					{
						goto OTA_DOWNLOAD_END;
					}
					else
					{
						luat_pm_reboot();
					}
				}
				if (!is_head_ok)
				{
					if (OTA_STATE_WRITE_COMMON_DATA == fota->ota_state)
					{
						is_head_ok = 1;
						memcpy(version, fota->fota_file_head.MainVersion, 16);
						LUAT_DEBUG_PRINT("full ota 用户标识 %s", version);
						//用户标识有16个字节空间，可以存放版本号来作为升级控制
					}
				}
			}
		}while(!result && (dummy_len > 0));
		goto WAIT_DATA;
	}


OTA_DOWNLOAD_END:
	LUAT_DEBUG_PRINT("full ota 测试失败");
	free(fota);
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
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
	luat_rtos_task_create(&g_s_task_handle, 4 * 1024, 50, "test", luat_test_task, NULL, 16);

}

INIT_TASK_EXPORT(luat_test_init, "1");
