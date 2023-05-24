/**
 * 通过HTTP在线播放MP3功能，由于44.1K的文件需要更多ram空间，必须开启低速模式编译！！！更低码率的，可以不开低速模式编译
 * HTTP下载并不是一个很好的流媒体播放方式，MP3格式也不是很好的流媒体播放格式
 * 本demo验证了边下边播的可行性
 *
 */
#include "common_api.h"
#include "luat_rtos.h"
#include "luat_audio_play_ec618.h"
#include "luat_i2s_ec618.h"
#include "luat_gpio.h"
#include "luat_debug.h"
#include "luat_mobile.h"
#include "luat_network_adapter.h"
#include "networkmgr.h"
//AIR780E+TM8211开发板配置
#define CODEC_PWR_PIN HAL_GPIO_12
#define CODEC_PWR_PIN_ALT_FUN	4
#define PA_PWR_PIN HAL_GPIO_25
#define PA_PWR_PIN_ALT_FUN	0
#define MP3_BUFFER_LEN_LOW	(20 * 1024)
#define MP3_BUFFER_LEN_HIGH	(30 * 1024)
#define MP3_FRAME_LEN (4 * 1152)
#define MP3_MAX_CODED_FRAME_SIZE 1792
static HANDLE g_s_delay_timer;
static luat_rtos_task_handle g_s_task_handle;
static network_ctrl_t *g_s_server_network_ctrl;
static Buffer_Struct g_s_mp3_buffer = {0};
static Buffer_Struct g_s_pcm_buffer = {0};
static uint32_t g_s_mp3_downloading;
static uint32_t g_s_mp3_buffer_wait_len;
static uint8_t g_s_mp3_error;
static uint8_t g_s_mp3_wait_start;
static uint8_t g_s_mp3_pause;
static void *g_s_mp3_decoder;	//不用的时候可以free掉，本demo由于是循环播放，没有free
enum
{
	AUDIO_NEED_DATA = 1,
	MP3_NEED_DATA,
};
int run_mp3_play(uint8_t is_start);

void app_pa_on(uint32_t arg)
{
	luat_gpio_set(PA_PWR_PIN, 1);
}

void run_mp3_decode(uint8_t *data, uint32_t len)
{
	if (data)
	{
		OS_BufferWrite(&g_s_mp3_buffer, data, len);
		if (g_s_mp3_buffer_wait_len >= len)
		{
			g_s_mp3_buffer_wait_len -= len;
		}
		else
		{
			LUAT_DEBUG_PRINT("error !");
			g_s_mp3_buffer_wait_len = 0;
		}
		free(data);
		if (g_s_mp3_wait_start)
		{
			if (g_s_mp3_buffer.Pos > 4096)
			{
				g_s_mp3_wait_start = 0;
				run_mp3_play(1);

			}
			return;
		}
		if (g_s_mp3_pause)
		{
			g_s_mp3_pause = 0;
			run_mp3_play(0);
		}
	}
	run_mp3_play(0);

}

void run_mp3_add_blank(uint8_t *data, uint32_t len)
{
	luat_audio_play_write_blank_raw(0, 6, 1);
}

void audio_event_cb(uint32_t event, void *param)
{
//	PadConfig_t pad_config;
//	GpioPinConfig_t gpio_config;
//	LUAT_DEBUG_PRINT("%d", event);
	switch(event)
	{
	case LUAT_MULTIMEDIA_CB_AUDIO_NEED_DATA:
		soc_call_function_in_audio(run_mp3_decode, NULL, 0, LUAT_WAIT_FOREVER);
		break;
	case LUAT_MULTIMEDIA_CB_AUDIO_DONE:
		if (g_s_mp3_downloading)
		{
			LUAT_DEBUG_PRINT("pause");
			g_s_mp3_pause = 1;
			soc_call_function_in_audio(run_mp3_add_blank, NULL, 0, LUAT_WAIT_FOREVER);
			return;
		}
		luat_audio_play_stop_raw(0);
		luat_rtos_timer_stop(g_s_delay_timer);
		luat_gpio_set(PA_PWR_PIN, 0);
		luat_gpio_set(CODEC_PWR_PIN, 0);
		break;
	}
}

void audio_data_cb(uint8_t *data, uint32_t len, uint8_t bits, uint8_t channels)
{
	//这里可以对音频数据进行软件音量缩放，或者直接清空来静音
	//软件音量缩放参考HAL_I2sSrcAdjustVolumn
	//LUAT_DEBUG_PRINT("%x,%d,%d,%d", data, len, bits, channels);
}


int run_mp3_play(uint8_t is_start)
{
	Audio_StreamStruct *stream = (Audio_StreamStruct *)luat_audio_play_get_stream(0);
	uint8_t num_channels = 1;
	uint32_t sample_rate = 0;
	int result, len;
	if (!g_s_mp3_decoder)
	{
		g_s_mp3_decoder = mp3_decoder_create();
	}
	if (is_start)
	{
		mp3_decoder_init(g_s_mp3_decoder);
		result = mp3_decoder_get_info(g_s_mp3_decoder, g_s_mp3_buffer.Data, g_s_mp3_buffer.Pos, &sample_rate, &num_channels);
		if (result)
		{
			mp3_decoder_init(g_s_mp3_decoder);
			LUAT_DEBUG_PRINT("mp3 %d,%d", sample_rate, num_channels);
			len = (num_channels * sample_rate >> 2) + MP3_FRAME_LEN * 2;
			OS_ReInitBuffer(&g_s_pcm_buffer, len);
		}
		else
		{
			LUAT_DEBUG_PRINT("mp3 decode fail!");
			g_s_mp3_error = 1;
			return -1;
		}
	}
	uint32_t pos = 0;
	uint32_t out_len, hz, used;
	if (!g_s_mp3_buffer.Pos)
	{
		return -1;
	}
	while ((llist_num(&stream->DataHead) < 3) && (g_s_mp3_buffer.Pos > (MP3_MAX_CODED_FRAME_SIZE * g_s_mp3_downloading + 1)) )
	{
		while (( g_s_pcm_buffer.Pos < (g_s_pcm_buffer.MaxLen - MP3_FRAME_LEN * 2) ) && (g_s_mp3_buffer.Pos > (MP3_MAX_CODED_FRAME_SIZE * g_s_mp3_downloading + 1)))
		{
			pos = 0;
			do
			{
				result = mp3_decoder_get_data(g_s_mp3_decoder, g_s_mp3_buffer.Data + pos, g_s_mp3_buffer.Pos - pos,
						(int16_t *)&g_s_pcm_buffer.Data[g_s_pcm_buffer.Pos], &out_len, &hz, &used);
				luat_wdt_feed();
				if (result > 0)
				{
					g_s_pcm_buffer.Pos += out_len;
				}
				pos += used;

				if (!result || (g_s_pcm_buffer.Pos >= (g_s_pcm_buffer.MaxLen - MP3_FRAME_LEN * 2)))
				{
					break;
				}
			} while ( ((g_s_mp3_buffer.Pos - pos) >= (MP3_MAX_CODED_FRAME_SIZE * g_s_mp3_downloading + 1)));
			OS_BufferRemove(&g_s_mp3_buffer, pos);
		}
		if (is_start)
		{
			luat_audio_play_start_raw(0, AUSTREAM_FORMAT_PCM, num_channels, sample_rate, 16, 1);
			//打开外部DAC，由于要配合PA的启动，需要播放一段空白音
			luat_gpio_set(CODEC_PWR_PIN, 1);
			luat_audio_play_write_blank_raw(0, 6, 1);
			is_start = 0;
			luat_rtos_timer_start(g_s_delay_timer, 200, 0, app_pa_on, NULL);
		}
		luat_audio_play_write_raw(0, g_s_pcm_buffer.Data, g_s_pcm_buffer.Pos);
		g_s_pcm_buffer.Pos = 0;
	}
}

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

static void luat_test_task(void *param)
{
	uint32_t water;
	luat_event_t event;
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG);
	uint32_t all,now_free_block,min_free_block;
	luat_rtos_timer_create(&g_s_delay_timer);
	luat_audio_play_global_init_with_task_priority(audio_event_cb, audio_data_cb, NULL, NULL, NULL, 90);
//	Audio_StreamStruct *stream = (Audio_StreamStruct *)luat_audio_play_get_stream(0);
//	如下配置可使用TM8211
    luat_i2s_base_setup(0, I2S_MODE_MSB, I2S_FRAME_SIZE_16_16);

	network_ctrl_t *netc = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(netc, g_s_task_handle, luat_test_socket_callback, g_s_task_handle);
	network_set_base_mode(netc, 1, 15000, 0, 0, 0, 0);	//http基于TCP
	netc->is_debug = 0;
	luat_ip_addr_t remote_ip;
	const char remote_domain[] = "www.air32.cn";
	const char mp3_file_name[] = "test_44K.mp3";
	// https测试
//	const char remote_domain[] = "xz.tingmall.com";
//	const char mp3_file_name[] = "preview/66/796/66796732-MP3-64K-FTD-P.mp3?sign=7e741f41bd9df27673da780ad073333a&t=64814ab6&transDeliveryCode=HW@2147483647@1686194870@S@8f00b204e9800998";
	int port = 80;
	// 如果是HTTPS，走443端口，并配置ssl
//	port = 443;
//	network_init_tls(netc, MBEDTLS_SSL_VERIFY_NONE);
	uint32_t dummy_len, start, end, total, i, data_len, download_len;
	int tx_len = 0;
	uint8_t *tx_data = malloc(1024);
	uint8_t *mp3_data;
	Buffer_Struct rx_buffer = {0};
	OS_InitBuffer(&rx_buffer, 8 * 1024);
	int result, http_response, head_len;
	uint8_t is_break,is_timeout, is_error;
	const char head[] = "GET /%s HTTP/1.1\r\nHost: %s:%d\r\nRange: bytes=%u-%u\r\nAccept: application/octet-stream\r\n\r\n";
	while(1)
	{
		luat_meminfo_sys(&all, &now_free_block, &min_free_block);
		LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
		rx_buffer.Pos = 0;
		is_error = 1;
		result = network_connect(netc, remote_domain, sizeof(remote_domain) - 1, NULL, port, 30000);
		//发送HTTP请求头，获取MP3文件的头部信息和总长度
		tx_len = sprintf_(tx_data, head, mp3_file_name, remote_domain, port, 0, 11);
		if (result)
		{
			LUAT_DEBUG_PRINT("服务器连不上");
			goto MP3_DOWNLOAD_END;
		}
		else
		{
			remote_ip = netc->dns_ip[netc->dns_ip_cnt].ip;
		}
		result = network_tx(netc, tx_data, tx_len, 0, NULL, 0, &dummy_len, 15000);
		if (result)
		{
			LUAT_DEBUG_PRINT("HTTP请求发送失败");
			goto MP3_DOWNLOAD_END;
		}
		result = network_wait_rx(netc, 20000, &is_break, &is_timeout);
		if (result || is_timeout)
		{
			LUAT_DEBUG_PRINT("HTTP请求没有响应");
			goto MP3_DOWNLOAD_END;
		}
		result = network_rx(netc, rx_buffer.Data, 1024 * 4, 0, NULL, NULL, &dummy_len);
		if (result)
		{
			LUAT_DEBUG_PRINT("网络错误");
			goto MP3_DOWNLOAD_END;
		}
		rx_buffer.Pos = dummy_len;
		rx_buffer.Data[dummy_len] = 0;
		http_response = get_http_response_code(rx_buffer.Data);
		if (http_response != 206)
		{
			LUAT_DEBUG_PRINT("HTTP响应码错误,%d", http_response);
			goto MP3_DOWNLOAD_END;
		}
		if (get_http_response_range(rx_buffer.Data, &start, &end, &total) < 0)
		{
			LUAT_DEBUG_PRINT("文件长度解析错误");
			goto MP3_DOWNLOAD_END;
		}
		head_len = get_http_response_head_len(rx_buffer.Data);
		if (head_len < 0)
		{
			LUAT_DEBUG_PRINT("HTTP头没有正确结束");
			goto MP3_DOWNLOAD_END;
		}
		if ( head_len <= dummy_len)
		{
			OS_BufferRemove(&rx_buffer, head_len);
		}
		if (rx_buffer.Pos < 12)
		{
			LUAT_DEBUG_PRINT("HTTP下载剩余部分");
			result = network_wait_rx(netc, 20000, &is_break, &is_timeout);
			if (result || is_timeout)
			{
				LUAT_DEBUG_PRINT("HTTP请求没有响应");
				goto MP3_DOWNLOAD_END;
			}

			result = network_rx(netc, rx_buffer.Data + rx_buffer.Pos, 12, 0, NULL, NULL, &dummy_len);
			if (result)
			{
				LUAT_DEBUG_PRINT("网络错误");
				goto MP3_DOWNLOAD_END;
			}
			rx_buffer.Pos += dummy_len;
		}
		if (rx_buffer.Pos < 12)
		{
			LUAT_DEBUG_PRINT("HTTP下载不足12字节");
			goto MP3_DOWNLOAD_END;
		}
		network_close(netc, 5000);
		if (!memcmp(rx_buffer.Data, "ID3", 3) || (rx_buffer.Data[0] == 0xff))
		{
			start = 0;
			if (rx_buffer.Data[0] != 0xff)
			{
				//跳过无用的数据
				for(i = 0; i < 4; i++)
				{
					start <<= 7;
					start |= rx_buffer.Data[6 + i] & 0x7f;
				}
			}
		}
		else
		{
			LUAT_DEBUG_PRINT("不是MP3文件，退出");
			rx_buffer.Data[rx_buffer.Pos] = 0;
			LUAT_DEBUG_PRINT("%.*s", rx_buffer.Pos, rx_buffer.Data);
			goto MP3_DOWNLOAD_END;
		}
		end = total - 1;
		LUAT_DEBUG_PRINT("MP3文件全局下载%u->%u", start, end);
		g_s_mp3_downloading = 1;
		g_s_mp3_error = 0;
		g_s_mp3_wait_start = 1;
		g_s_mp3_buffer_wait_len = 0;
		is_error = 0;
		luat_audio_play_set_user_lock(0, 1);
MP3_DOWNLOAD_GO_ON:
		while(start < total)
		{

			LUAT_DEBUG_PRINT("MP3文件当前下载%u->%u", start, end);
			result = network_connect(netc, remote_domain, sizeof(remote_domain) - 1, &remote_ip, port, 30000);
			//发送HTTP请求头，获取MP3文件的头部信息和总长度
			tx_len = sprintf_(tx_data, head, mp3_file_name, remote_domain, port, start, end);
			if (result)
			{
				LUAT_DEBUG_PRINT("服务器连不上");
				goto MP3_DOWNLOAD_END;
			}
			result = network_tx(netc, tx_data, tx_len, 0, NULL, 0, &dummy_len, 15000);
			if (result)
			{
				LUAT_DEBUG_PRINT("HTTP请求发送失败");
				goto MP3_DOWNLOAD_END;
			}
			rx_buffer.Pos = 0;
HTTP_GET_MORE_HEAD_DATA:
			result = network_wait_rx(netc, 20000, &is_break, &is_timeout);
			if (result || is_timeout)
			{
				LUAT_DEBUG_PRINT("HTTP请求没有响应");
				goto MP3_DOWNLOAD_END;
			}
			result = network_rx(netc, rx_buffer.Data + rx_buffer.Pos, rx_buffer.MaxLen - rx_buffer.Pos - 4, 0, NULL, NULL, &dummy_len);
			if (result)
			{
				LUAT_DEBUG_PRINT("网络错误,%d", result);
				goto MP3_DOWNLOAD_END;
			}
			rx_buffer.Pos += dummy_len;
			if (rx_buffer.Pos < 20)
			{
				LUAT_DEBUG_PRINT("http head len %d", rx_buffer.Pos);
				goto HTTP_GET_MORE_HEAD_DATA;
			}
			rx_buffer.Data[rx_buffer.Pos] = 0;
			http_response = get_http_response_code(rx_buffer.Data);
			if (http_response != 206)
			{
				LUAT_DEBUG_PRINT("HTTP响应码错误,%d", http_response);
				goto MP3_DOWNLOAD_END;
			}
			head_len = get_http_response_head_len(rx_buffer.Data);
			if (head_len < 0)
			{
				LUAT_DEBUG_PRINT("HTTP头没有正确结束");
				goto MP3_DOWNLOAD_END;
			}
			download_len = 0;
			if (head_len < dummy_len)
			{
				download_len = dummy_len - head_len;
				mp3_data = malloc(download_len);
				memcpy(mp3_data, rx_buffer.Data + head_len, download_len);
				g_s_mp3_buffer_wait_len += download_len;
				soc_call_function_in_audio(run_mp3_decode, mp3_data, download_len, LUAT_WAIT_FOREVER);
			}
			start += download_len;
//			LUAT_DEBUG_PRINT("start %u", start);
			if (start >= total)
			{
				is_error = 0;
				goto MP3_DOWNLOAD_END;
			}
			while(!g_s_mp3_error)
			{
				if ( (g_s_mp3_buffer.Pos + g_s_mp3_buffer_wait_len) < MP3_BUFFER_LEN_LOW )
				{
					LUAT_DEBUG_PRINT("获取更多mp3数据 %d", g_s_mp3_buffer.Pos + g_s_mp3_buffer_wait_len);
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
							goto MP3_DOWNLOAD_GO_ON;
						}
					}
					do
					{
						result = network_rx(netc, rx_buffer.Data, rx_buffer.MaxLen, 0, NULL, NULL, &dummy_len);
						if (dummy_len > 0)
						{
							mp3_data = malloc(dummy_len);
							memcpy(mp3_data, rx_buffer.Data, dummy_len);
							g_s_mp3_buffer_wait_len += dummy_len;
							soc_call_function_in_audio(run_mp3_decode, mp3_data, dummy_len, LUAT_WAIT_FOREVER);
							start += dummy_len;
//							LUAT_DEBUG_PRINT("start %u", start);
							if (start >= total)
							{
								is_error = 0;
								goto MP3_DOWNLOAD_END;
							}
						}
					}while(!result && (dummy_len > 0) && ((g_s_mp3_buffer.Pos + g_s_mp3_buffer_wait_len) < MP3_BUFFER_LEN_HIGH));
				}
				else
				{
					if (!luat_rtos_event_recv(g_s_task_handle, 0, &event, NULL, 100))
					{
						if (event.id != EV_NW_RESULT_EVENT)
						{
							LUAT_DEBUG_PRINT("new event id %x", event.id);	//这里可以加入其它操作
						}
					}
				}
			}
		}
MP3_DOWNLOAD_END:
		g_s_mp3_downloading = 0;
		luat_audio_play_set_user_lock(0, 0);
		LUAT_DEBUG_PRINT("本次MP3下载结束，60秒后重试");
		if (is_error || g_s_mp3_error)
		{
			LUAT_DEBUG_PRINT("本次MP3边下边播发生错误，需要关闭播放");
			luat_audio_play_stop_raw(0);
			luat_rtos_timer_stop(g_s_delay_timer);
			luat_gpio_set(PA_PWR_PIN, 0);
			luat_gpio_set(CODEC_PWR_PIN, 0);
		}
		network_close(netc, 5000);
		OS_DeInitBuffer(&g_s_mp3_buffer);
		luat_meminfo_sys(&all, &now_free_block, &min_free_block);
		LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
		luat_rtos_task_sleep(60000);
	}
}

static void luat_test_init(void)
{
	luat_gpio_cfg_t gpio_cfg;
	luat_gpio_set_default_cfg(&gpio_cfg);
	gpio_cfg.pull = LUAT_GPIO_DEFAULT;
	gpio_cfg.pin = PA_PWR_PIN;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.pin = CODEC_PWR_PIN;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.alt_fun = CODEC_PWR_PIN_ALT_FUN;
	luat_gpio_open(&gpio_cfg);
	luat_mobile_event_register_handler(luatos_mobile_event_callback);
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
	luat_rtos_task_create(&g_s_task_handle, 4 * 1024, 50, "test", luat_test_task, NULL, 16);

}

INIT_TASK_EXPORT(luat_test_init, "1");
