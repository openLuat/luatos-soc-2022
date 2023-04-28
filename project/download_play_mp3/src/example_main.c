/**
 * 通过HTTP的断点续传功能来实现边下边播MP3功能，由于开源库的解码性能，高品质的MP3无法播放，需要更换商用解码库
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
#define MP3_DATA_BUFFER_LEN	(40 * 1024)
#define MP3_FRAME_LEN (4 * 1152)
#define MP3_MAX_CODED_FRAME_SIZE 1792
static HANDLE g_s_delay_timer;
static luat_rtos_task_handle g_s_task_handle;
static network_ctrl_t *g_s_server_network_ctrl;
static Buffer_Struct g_s_mp3_buffer = {0};
static Buffer_Struct g_s_pcm_buffer = {0};
static void *g_s_mp3_decoder;	//不用的时候可以free掉，本demo由于是循环播放，没有free
static uint8_t g_s_mp3_downloading;
static uint8_t g_s_mp3_download_wait;
static luat_rtos_mutex_t mp3_buffer_mutex;
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
	run_mp3_play(0);
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
			len = (num_channels * sample_rate >> 2) + MP3_FRAME_LEN * 2;
			OS_ReInitBuffer(&g_s_pcm_buffer, len);
		}
		else
		{
			LUAT_DEBUG_PRINT("mp3 decode fail!");
			return -1;
		}
	}
	uint32_t pos = 0;
	uint32_t out_len, hz, used;
	while ((llist_num(&stream->DataHead) < 4) && (g_s_mp3_buffer.Pos > 2) )
	{
		while (( g_s_pcm_buffer.Pos < (g_s_pcm_buffer.MaxLen - MP3_FRAME_LEN * 2) ) && (g_s_mp3_buffer.Pos > (MP3_MAX_CODED_FRAME_SIZE * g_s_mp3_downloading + 1)))
		{
			pos = 0;
			do
			{
				result = mp3_decoder_get_data(g_s_mp3_decoder, g_s_mp3_buffer.Data + pos, g_s_mp3_buffer.Pos - pos,
						(int16_t *)&g_s_pcm_buffer.Data[g_s_pcm_buffer.Pos], &out_len, &hz, &used);
				if (result > 0)
				{
					g_s_pcm_buffer.Pos += out_len;
				}
				pos += used;

				if (g_s_pcm_buffer.Pos >= (g_s_pcm_buffer.MaxLen - MP3_FRAME_LEN * 2))
				{
					break;
				}
			} while ( ((g_s_mp3_buffer.Pos - pos) >= (MP3_MAX_CODED_FRAME_SIZE * g_s_mp3_downloading + 1)));
			luat_rtos_mutex_lock(mp3_buffer_mutex, 100);
			OS_BufferRemove(&g_s_mp3_buffer, pos);
			luat_rtos_mutex_unlock(mp3_buffer_mutex);
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
	if (g_s_mp3_downloading && g_s_mp3_download_wait && (g_s_mp3_buffer.Pos < (MP3_DATA_BUFFER_LEN/2)))
	{
		luat_rtos_event_send(g_s_task_handle, MP3_NEED_DATA, 0, 0, 0, 0);
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
   p_end=p_end+strlen("\r\n\r\n");
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
	if ( AUDIO_NEED_DATA == event->ID )
	{
		run_mp3_play(0);
	}
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
	luat_event_t event;
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG);
	luat_rtos_mutex_create(&mp3_buffer_mutex);
	luat_rtos_mutex_unlock(mp3_buffer_mutex);
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
	const char remote_ip[] = "www.air32.cn";
	const char mp3_file_name[] = "test_22K.mp3";	//测试服务器上的22.1Khz采样率文件。超过32Khz采样率的需要用效率更高的解码器
	int port = 80;
	uint32_t dummy_len, start, end, total, i, data_len, download_len;
	int tx_len = 0;
	uint8_t *tx_data = malloc(1024);
	Buffer_Struct rx_buffer = {0};
	OS_InitBuffer(&rx_buffer, MP3_DATA_BUFFER_LEN/2);
	int result, http_response, head_len;
	uint8_t is_break,is_timeout, first_play, is_error;
	const char head[] = "GET /%s HTTP/1.1\r\nHost: %s:%d\r\nRange: bytes=%u-%u\r\nAccept: application/octet-stream\r\n\r\n";
	while(1)
	{
		luat_meminfo_sys(&all, &now_free_block, &min_free_block);
		LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
		rx_buffer.Pos = 0;
		first_play = 1;
		is_error = 1;
		result = network_connect(netc, remote_ip, sizeof(remote_ip) - 1, NULL, port, 30000);
		//发送HTTP请求头，获取MP3文件的头部信息和总长度
		tx_len = sprintf_(tx_data, head, mp3_file_name, remote_ip, port, 0, 11);
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
		if ( head_len < dummy_len)
		{
			OS_BufferRemove(&rx_buffer, head_len);
		}
		LUAT_DEBUG_PRINT("%d", rx_buffer.Pos);
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
			goto MP3_DOWNLOAD_END;
		}
		LUAT_DEBUG_PRINT("MP3文件全局下载%u->%u", start, total - 1);
		OS_ReInitBuffer(&g_s_mp3_buffer, MP3_DATA_BUFFER_LEN + 1024);
		g_s_mp3_downloading = 1;
		while(start < total)
		{
			if (g_s_mp3_buffer.Pos < (MP3_DATA_BUFFER_LEN/2))
			{
				g_s_mp3_download_wait = 0;
				if (first_play)
				{
					end = (((total - start) > MP3_DATA_BUFFER_LEN)?(start + MP3_DATA_BUFFER_LEN):total) - 1;
				}
				else
				{
					end = (((total - start) > (MP3_DATA_BUFFER_LEN/2))?(start + (MP3_DATA_BUFFER_LEN/2)):total) - 1;
				}
				data_len = end - start + 1;
				LUAT_DEBUG_PRINT("MP3文件当前下载%u->%u,%ubyte", start, end, data_len);
				result = network_connect(netc, remote_ip, sizeof(remote_ip) - 1, NULL, port, 30000);
				//发送HTTP请求头，获取MP3文件的头部信息和总长度
				tx_len = sprintf_(tx_data, head, mp3_file_name, remote_ip, port, start, end);
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
				result = network_wait_rx(netc, 20000, &is_break, &is_timeout);
				if (result || is_timeout)
				{
					LUAT_DEBUG_PRINT("HTTP请求没有响应");
					goto MP3_DOWNLOAD_END;
				}
				result = network_rx(netc, rx_buffer.Data, 4 * 1024, 0, NULL, NULL, &dummy_len);
				if (result)
				{
					LUAT_DEBUG_PRINT("网络错误");
					goto MP3_DOWNLOAD_END;
				}
				rx_buffer.Data[dummy_len] = 0;
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
					luat_rtos_mutex_lock(mp3_buffer_mutex, 50);
					OS_BufferWrite(&g_s_mp3_buffer, rx_buffer.Data + head_len, dummy_len - head_len);
					luat_rtos_mutex_unlock(mp3_buffer_mutex);
					download_len += dummy_len - head_len;
				}

				do
				{
					result = network_rx(netc, rx_buffer.Data, MP3_DATA_BUFFER_LEN/2, 0, NULL, NULL, &dummy_len);
					if (dummy_len > 0)
					{
						luat_rtos_mutex_lock(mp3_buffer_mutex, 50);
						OS_BufferWrite(&g_s_mp3_buffer, rx_buffer.Data, dummy_len);
						luat_rtos_mutex_unlock(mp3_buffer_mutex);
						download_len += dummy_len;
					}
				}while(!result && dummy_len > 0);

				while(download_len < data_len)
				{
					result = network_wait_rx(netc, 20000, &is_break, &is_timeout);
					if (result || is_timeout)
					{
						LUAT_DEBUG_PRINT("HTTP请求没有响应");
						goto MP3_DOWNLOAD_END;
					}
					do
					{
						result = network_rx(netc, rx_buffer.Data, MP3_DATA_BUFFER_LEN/2, 0, NULL, NULL, &dummy_len);
						if (dummy_len > 0)
						{
							luat_rtos_mutex_lock(mp3_buffer_mutex, 50);
							OS_BufferWrite(&g_s_mp3_buffer, rx_buffer.Data, dummy_len);
							luat_rtos_mutex_unlock(mp3_buffer_mutex);
							download_len += dummy_len;
						}
					}while(!result && dummy_len > 0);
				};
				if (first_play)
				{
					if (run_mp3_play(1) < 0)
					{
						LUAT_DEBUG_PRINT("MP3解码错误");
						goto MP3_DOWNLOAD_END;
					}
				}
				LUAT_DEBUG_PRINT("等待新的下载请求");
				start += data_len;
				network_close(netc, 5000);
				first_play = 0;
				g_s_mp3_download_wait = 1;
				if (luat_rtos_event_recv(g_s_task_handle, MP3_NEED_DATA, &event, luat_test_socket_callback, 20000))
				{
					LUAT_DEBUG_PRINT("长时间没有等到新的下载请求，MP3播放可能异常了");
					goto MP3_DOWNLOAD_END;
				}
				LUAT_DEBUG_PRINT("新的下载请求");
				if (start >= total)
				{
					is_error = 0;
				}
			}
		}
MP3_DOWNLOAD_END:
		g_s_mp3_downloading = 0;
		LUAT_DEBUG_PRINT("本次MP3下载结束，60秒后重试");
		if (is_error && !first_play)
		{
			LUAT_DEBUG_PRINT("本次MP3边下边播发生错误，需要关闭播放");
			luat_audio_play_stop_raw(0);
			luat_rtos_timer_stop(g_s_delay_timer);
			luat_gpio_set(PA_PWR_PIN, 0);
			luat_gpio_set(CODEC_PWR_PIN, 0);
		}
		network_close(netc, 5000);
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
