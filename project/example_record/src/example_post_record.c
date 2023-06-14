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


/* 
	此demo演示的功能为，录音后将录音数据上传至服务器
	打印record start后表示走到开始录音的流程，录音时长默认五秒，可自行修改
	打印amr encode stop表示录音+amr编码结束

	默认的录音文件名为“设备imei_上传前的设备本地时间.amr”
	上传的录音文件可至http://tools.openluat.com/tools/device-upload-test界面查看
*/

#include "common_api.h"

#include "luat_rtos.h"
#include "luat_audio_play_ec618.h"
#include "luat_i2s_ec618.h"
#include "luat_gpio.h"
#include "luat_debug.h"
#include "luat_i2c.h"
#include "interf_enc.h"
#include "HTTPClient.h"
#include "luat_mobile.h"



#define TEST_HOST "http://tools.openluat.com/api/site/device_upload_file"

#define HTTP_RECV_BUF_SIZE      (1501)
#define HTTP_HEAD_BUF_SIZE      (800)
#define HTTP_HEAD_CONTYPE_SIZE	(140)

//AIR780E+TM8211开发板配置
#define CODEC_PWR_PIN HAL_GPIO_12
#define CODEC_PWR_PIN_ALT_FUN	4
#define PA_PWR_PIN HAL_GPIO_25
#define PA_PWR_PIN_ALT_FUN	0
#define LED2_PIN	HAL_GPIO_24
#define LED2_PIN_ALT_FUN	0
#define LED3_PIN	HAL_GPIO_28
#define LED3_PIN_ALT_FUN	0
#define LED4_PIN	HAL_GPIO_27
#define LED4_PIN_ALT_FUN	0
#define CHARGE_EN_PIN	HAL_GPIO_2
#define CHARGE_EN_PIN_ALT_FUN	0
#define RECORD_TIME	(5)	//设置5秒录音，只要ram够，当然可以更长
static HANDLE g_s_delay_timer;
static HANDLE g_s_amr_encoder_handler;
static uint32_t g_s_record_time;
static Buffer_Struct g_s_amr_rom_file;

luat_rtos_semaphore_t net_semaphore_handle;

void mobile_event_callback(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status){
    if (event == LUAT_MOBILE_EVENT_NETIF && status == LUAT_MOBILE_NETIF_LINK_ON){
        LUAT_DEBUG_PRINT("network ready");
        luat_rtos_semaphore_release(net_semaphore_handle);
    }
}

typedef struct
{
	uint8_t reg;
	uint8_t value;
}i2c_reg_t;

static const i2c_reg_t es8218_reg_table[] =
	{
		{0x00, 0x00},
		// Ratio=MCLK/LRCK=256：12M288-48K；4M096-16K; 2M048-8K
		{0x01, 0x2F + (0 << 7)}, //(0x01,0x2F + (MSMode_MasterSelOn<<7))
		{0x02, 0x01},
		{0x03, 0x20},
		{0x04, 0x01},		  // LRCKDIV
		{0x05, 0x00},		  // LRCKDIV=256
		{0x06, 4 + (0 << 5)}, //(0x06,SCLK_DIV + (SCLK_INV<<5))
		{0x10, 0x18 + 0},	  //(0x10,0x18 + Dmic_Selon)

		{0x07, 0 + (3 << 2)}, //(0X07,NORMAL_I2S + (Format_Len<<2));//IIS 16BIT
		{0x09, 0x00},
		{0x0A, 0x22 + (0xCC * 0)}, //(0x0A,0x22 + (0xCC*VDDA_VOLTAGE)) 0 = 3.3V 1 = 1.8V
		{0x0B, 0x02 - 0},		   //(0x0B,0x02 - VDDA_VOLTAGE)0 = 3.3V 1 = 1.8V
		{0x14, 0xA0},
		{0x0D, 0x30},
		{0x0E, 0x20},
		{0x23, 0x00},
		{0x24, 0x00},
		{0x18, 0x04},
		{0x19, 0x04},
		{0x0F, (0 << 5) + (1 << 4) + 7}, //(0x0F,(ADCChannelSel<<5) + (ADC_PGA_DF2SE_18DB<<4) + ADC_PGA_GAIN);
		{0x08, 0x00},
		{0x00, 0x80},
		{0x12, 0x1C}, // ALC OFF
		{0x11, 0},	  // ADC_Volume
};

void app_pa_on(uint32_t arg)
{
	luat_gpio_set(PA_PWR_PIN, 1);
}

static void record_encode_amr(uint8_t *data, uint32_t len)
{
	uint8_t outbuf[64];
	int16_t *pcm = (int16_t *)data;
	uint32_t total_len = len >> 1;
	uint32_t done_len = 0;
	int out_len;
	while ((total_len - done_len) >= 160)
	{
		out_len = Encoder_Interface_Encode(g_s_amr_encoder_handler, MR122, &pcm[done_len], outbuf, 0);
		if (out_len <= 0)
		{
			LUAT_DEBUG_PRINT("encode error in %d,result %d", done_len, out_len);
		}
		else
		{
			OS_BufferWrite(&g_s_amr_rom_file, outbuf, out_len);
		}
		done_len += 160;
	}
	free(data);
}

static void record_stop_encode_amr(uint8_t *data, uint32_t len)
{
	Encoder_Interface_exit(g_s_amr_encoder_handler);
	g_s_amr_encoder_handler = NULL;
	LUAT_DEBUG_PRINT("amr encode stop");
}

static int32_t record_cb(void *pdata, void *param)
{
	Buffer_Struct *buffer = (Buffer_Struct *)pdata;
	if (buffer && (buffer->Pos >= 320))
	{
		void *buff = malloc(buffer->Pos);
		memcpy(buff, buffer->Data, buffer->Pos);
		//复杂耗时的操作不可以在回调里处理，这里放到audio task，当然也可以放到用户自己的task里
		soc_call_function_in_audio(record_encode_amr, buff, buffer->Pos, LUAT_WAIT_FOREVER);
		g_s_record_time++;
		if (g_s_record_time >= (RECORD_TIME * 5))	//15秒
		{
			luat_i2s_rx_stop(I2S_ID0);
			soc_call_function_in_audio(record_stop_encode_amr, NULL, NULL, LUAT_WAIT_FOREVER);
		}


		buffer->Pos = 0;

	}

	return 0;
}

void audio_event_cb(uint32_t event, void *param)
{
//	PadConfig_t pad_config;
//	GpioPinConfig_t gpio_config;

	LUAT_DEBUG_PRINT("%d", event);
	switch(event)
	{
	case LUAT_MULTIMEDIA_CB_AUDIO_DECODE_START:
		//luat_gpio_set(CODEC_PWR_PIN, 1);
		luat_audio_play_write_blank_raw(0, 6, 1);
		break;
	case LUAT_MULTIMEDIA_CB_AUDIO_OUTPUT_START:
		luat_rtos_timer_start(g_s_delay_timer, 200, 0, app_pa_on, NULL);
		break;
	case LUAT_MULTIMEDIA_CB_TTS_INIT:
		break;
	case LUAT_MULTIMEDIA_CB_TTS_DONE:
		if (!luat_audio_play_get_last_error(0))
		{
			luat_audio_play_write_blank_raw(0, 1, 0);
		}
		break;
	case LUAT_MULTIMEDIA_CB_AUDIO_DONE:
		luat_rtos_timer_stop(g_s_delay_timer);
		LUAT_DEBUG_PRINT("audio play done, result=%d!", luat_audio_play_get_last_error(0));
		luat_gpio_set(PA_PWR_PIN, 0);
		//luat_gpio_set(CODEC_PWR_PIN, 0);
		break;
	}
}

void audio_data_cb(uint8_t *data, uint32_t len, uint8_t bits, uint8_t channels)
{
	//这里可以对音频数据进行软件音量缩放，或者直接清空来静音
	//软件音量缩放参考HAL_I2sSrcAdjustVolumn
	//LUAT_DEBUG_PRINT("%x,%d,%d,%d", data, len, bits, channels);
}

int postMultipartFormData(HttpClientData *clientData, int contentTypeLen, char *data, uint32_t dataLen, char *fileName, int postBufMaxLen)
{	
	// boundary给个长度100，应该够了，实际上应该到不了100，加上filename的长度，再加64， 再加8个固定字节，再加上数据长度，够了
	// 总的postbuf长度， 100+strlen(filename)+64+8+datalen
	if(clientData == NULL || data == NULL || fileName == NULL)
	{
		return -1;
	}
	time_t nowtime;
	time(&nowtime);
	uint64_t tick = luat_mcu_tick64_ms();
	char boundary[100] = {0};
	snprintf(boundary, 100, "--------------------------%lld%lld", nowtime, tick);
	snprintf(clientData->postContentType, contentTypeLen, "multipart/form-data; boundary=%s", boundary);
	snprintf(clientData->postBuf, postBufMaxLen, "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n\r\n", boundary, fileName);
	clientData->postBufLen = strlen(clientData->postBuf);
	memcpy(clientData->postBuf + clientData->postBufLen, data, dataLen);
	clientData->postBufLen = clientData->postBufLen + dataLen;
	memcpy(clientData->postBuf + clientData->postBufLen, "\r\n--", 4);
	clientData->postBufLen = clientData->postBufLen + 4;
	memcpy(clientData->postBuf + clientData->postBufLen, boundary, strlen(boundary));
	clientData->postBufLen = clientData->postBufLen + strlen(boundary);
	memcpy(clientData->postBuf + clientData->postBufLen, "--\r\n", 4);
	clientData->postBufLen = clientData->postBufLen + 4;
	return 0;
}


static void es8218_demo_task(void *arg)
{

    luat_rtos_semaphore_create(&net_semaphore_handle, 1);
    luat_mobile_event_register_handler(mobile_event_callback);
    luat_rtos_semaphore_take(net_semaphore_handle, LUAT_WAIT_FOREVER);
	uint32_t total, total_free, min_free;
	uint32_t i;
	uint16_t i2c_address = 0x10;
	uint8_t tx_buf[2];
	uint8_t rx_buf[2];
	luat_audio_play_info_t info[1] = {0};
	char imeiBuf [20] = {0};
	char fileName[50] = {0};
	HttpClientContext    gHttpClient = {0};
	HttpClientData data = {0};

	luat_audio_play_global_init(audio_event_cb, audio_data_cb, luat_audio_play_file_default_fun, NULL, NULL);
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG);
	luat_i2c_setup(I2C_ID0, 400000);

	luat_rtos_timer_create(&g_s_delay_timer);
	luat_i2s_base_setup(I2S_ID0, I2S_MODE_I2S, I2S_FRAME_SIZE_16_16);
	g_s_amr_encoder_handler = Encoder_Interface_init(0);
	OS_InitBuffer(&g_s_amr_rom_file, RECORD_TIME * 1604 + 6); // 1秒最高音质的AMRNB编码是1600
	OS_BufferWrite(&g_s_amr_rom_file, "#!AMR\n", 6);
	// 如果有休眠操作，且控制codec的电源的IO不是AONGPIO，又没有外部上拉保持IO电平，则在唤醒时必须重新初始化codec
	for (i = 0; i < sizeof(es8218_reg_table) / sizeof(i2c_reg_t); i++)
	{
		luat_i2c_transfer(I2C_ID0, i2c_address, NULL, 0, (uint8_t *)&es8218_reg_table[i], 2);
		rx_buf[0] = ~es8218_reg_table[i].value;
		luat_i2c_transfer(I2C_ID0, i2c_address, (uint8_t *)&es8218_reg_table[i].reg, 1, rx_buf, 1);
		if (rx_buf[0] != es8218_reg_table[i].value)
		{
			LUAT_DEBUG_PRINT("write reg %x %x %x", es8218_reg_table[i].reg, es8218_reg_table[i].value, rx_buf[0]);
		}
	}
	LUAT_DEBUG_PRINT("record start");
	luat_i2s_start(I2S_ID0, 0, 8000, 1);
	luat_i2s_no_block_rx(I2S_ID0, 320 * 10, record_cb, NULL); // 单声道8K的amr编码一次320字节，这里每200ms回调一次
	tx_buf[0] = 0x01;
	tx_buf[1] = (0x2f) + (1 << 7);
	luat_i2c_transfer(I2C_ID0, i2c_address, NULL, 0, tx_buf, 2);
	luat_rtos_task_sleep((RECORD_TIME + 1) * 1000);
	tx_buf[0] = 0x01;
	tx_buf[1] = (0x2f) + (0 << 7);
	luat_i2c_transfer(I2C_ID0, i2c_address, NULL, 0, tx_buf, 2);
	
	gHttpClient.caCert= NULL;
    gHttpClient.caCertLen= 0;
    gHttpClient.timeout_s = 2;
    gHttpClient.timeout_r = 20;
	

	data.headerBuf = malloc(HTTP_HEAD_BUF_SIZE);
	data.headerBufLen = HTTP_HEAD_BUF_SIZE;
	memset(data.headerBuf, 0, HTTP_HEAD_BUF_SIZE);

	data.respBuf = malloc(HTTP_RECV_BUF_SIZE);
	data.respBufLen = HTTP_RECV_BUF_SIZE;
	memset(data.respBuf, 0, HTTP_RECV_BUF_SIZE);

	data.postContentType = malloc(HTTP_HEAD_CONTYPE_SIZE);
	memset(data.postContentType, 0, HTTP_HEAD_CONTYPE_SIZE);

	time_t nowtime;
	time(&nowtime);
	struct tm *timeInfo = NULL;
	timeInfo = localtime(&nowtime);
	if (luat_mobile_get_imei(0, imeiBuf, 20) > 0 )
	{
		// timeInfo->tm
		snprintf(fileName, 50, "%s_%4d%02d%02d_%02d%02d%02d.amr", imeiBuf, 1900 + timeInfo->tm_year, timeInfo->tm_mon + 1, timeInfo->tm_mday, timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
	}
	else
	{
		snprintf(fileName, 50, "%s_%4d%02d%02d_%02d%02d%02d.amr", "record",1900 + timeInfo->tm_year, timeInfo->tm_mon + 1, timeInfo->tm_mday, timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
	}
	LUAT_DEBUG_PRINT("file name %s", fileName);
	int bufMaxLen = 172 + strlen(fileName) + g_s_amr_rom_file.Pos;
	data.postBuf = malloc(bufMaxLen);
	memset(data.postBuf, 0, bufMaxLen);

	if(0 == postMultipartFormData(&data, HTTP_HEAD_CONTYPE_SIZE, g_s_amr_rom_file.Data, g_s_amr_rom_file.Pos, fileName, bufMaxLen))
    {
        HTTPResult result = httpPostURL(&gHttpClient, TEST_HOST, &data, 30000);
		LUAT_DEBUG_PRINT("http post result %d", result);
		if(HTTP_OK == result)
		{
			LUAT_DEBUG_PRINT("http post success");
		}
	    else
		{
			LUAT_DEBUG_PRINT("http post fail");
		}
    }
	httpClose(&gHttpClient);
    free(data.headerBuf);
    free(data.respBuf);
    free(data.postContentType);
    free(data.postBuf);
	while (1)
	{
		soc_get_heap_info(&total, &total_free, &min_free);
		LUAT_DEBUG_PRINT("free heap %d", total_free);
		luat_rtos_task_sleep(5000);
	}
}

static void test_record_demo_init(void)
{
	luat_gpio_cfg_t gpio_cfg;
	luat_gpio_set_default_cfg(&gpio_cfg);
	luat_rtos_task_handle task_handle;

	gpio_cfg.pin = LED2_PIN;
	gpio_cfg.pull = LUAT_GPIO_DEFAULT;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.pin = LED3_PIN;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.pin = LED4_PIN;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.pin = CHARGE_EN_PIN;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.pin = PA_PWR_PIN;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.pin = CODEC_PWR_PIN;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.alt_fun = CODEC_PWR_PIN_ALT_FUN;
	luat_gpio_open(&gpio_cfg);
	luat_gpio_set(CODEC_PWR_PIN, 1);
	//demo用于AIR780e + es8218e录音, 将录音数据上传到服务器
	luat_rtos_task_create(&task_handle, 4096, 20, "es8218", es8218_demo_task, NULL, 0);
}

INIT_TASK_EXPORT(test_record_demo_init, "1");