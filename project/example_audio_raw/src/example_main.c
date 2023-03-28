//demo用于AIR600EAC云喇叭开发
#include "common_api.h"

#include "luat_rtos.h"
#include "luat_audio_play_ec618.h"
#include "luat_i2s_ec618.h"
#include "power_audio.h"
#include "luat_gpio.h"
#include "luat_debug.h"
//AIR780E+TM8211开发板配置
#define CODEC_PWR_PIN HAL_GPIO_12
#define CODEC_PWR_PIN_ALT_FUN	4
#define PA_PWR_PIN HAL_GPIO_25
#define PA_PWR_PIN_ALT_FUN	0
#define LED2_PIN	HAL_GPIO_24
#define LED2_PIN_ALT_FUN	0
#define LED3_PIN	HAL_GPIO_23
#define LED3_PIN_ALT_FUN	0
#define LED4_PIN	HAL_GPIO_27
#define LED4_PIN_ALT_FUN	0
#define CHARGE_EN_PIN	HAL_GPIO_2
#define CHARGE_EN_PIN_ALT_FUN	0

//AIR600EAC开发板配置
//#define CODEC_PWR_PIN HAL_GPIO_12
//#define CODEC_PWR_PIN_ALT_FUN	4
//#define PA_PWR_PIN HAL_GPIO_10
//#define PA_PWR_PIN_ALT_FUN	0
//#define CODEC_PWR_PIN HAL_GPIO_12
//#define CODEC_PWR_PIN_ALT_FUN	4
//#define PA_PWR_PIN HAL_GPIO_10
//#define PA_PWR_PIN_ALT_FUN	0
//#define LED2_PIN	HAL_GPIO_26
//#define LED2_PIN_ALT_FUN	0
//#define LED3_PIN	HAL_GPIO_27
//#define LED3_PIN_ALT_FUN	0
//#define LED4_PIN	HAL_GPIO_20
//#define LED4_PIN_ALT_FUN	0
//#define CHARGE_EN_PIN	HAL_GPIO_NONE
//#define CHARGE_EN_PIN_ALT_FUN	0
static HANDLE g_s_delay_timer;
static uint32_t g_s_play_point;
static uint32_t g_s_once_len;
void app_pa_on(uint32_t arg)
{
	luat_gpio_set(PA_PWR_PIN, 1);
}

void audio_add_data(void)
{
	if (g_s_play_point < sizeof(Fqdqwer))
	{
	    if (g_s_once_len < (sizeof(Fqdqwer) - g_s_play_point))
	    {
	    	luat_audio_play_write_raw(0, &Fqdqwer[g_s_play_point], g_s_once_len);
	    	g_s_play_point += g_s_once_len;
	    }
	    else
	    {
	    	luat_audio_play_write_raw(0, &Fqdqwer[g_s_play_point], (sizeof(Fqdqwer) - g_s_play_point));
	    	g_s_play_point = sizeof(Fqdqwer);
	    }
	}

}

void audio_event_cb(uint32_t event, void *param)
{
//	PadConfig_t pad_config;
//	GpioPinConfig_t gpio_config;

	LUAT_DEBUG_PRINT("%d", event);
	switch(event)
	{
	case LUAT_MULTIMEDIA_CB_AUDIO_NEED_DATA:
		audio_add_data();
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


static void demo_task(void *arg)
{
	size_t total, used, max_used, pos;
	uint8_t num_channels = 1;
	uint32_t sample_rate = 0;
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG);
	luat_rtos_timer_create(&g_s_delay_timer);
	luat_audio_play_global_init(audio_event_cb, audio_data_cb, NULL, NULL, NULL);
//	如下配置可使用TM8211
    luat_i2s_base_setup(0, I2S_MODE_MSB, I2S_FRAME_SIZE_16_16);

    pos = 20; //演示PCM数据播放，快速跳过wav文件头
    num_channels = Fqdqwer[pos + 2];
    memcpy(&sample_rate, Fqdqwer+ pos + 4, 4);

    g_s_once_len = (sample_rate * num_channels) >> 1;	//一次放250ms的数据
    LUAT_DEBUG_PRINT("%d,%d,%d", num_channels, sample_rate,g_s_once_len);
    g_s_play_point = 44;//演示PCM数据播放，快速跳过wav文件头
    //初始化播放
    luat_audio_play_start_raw(0, AUSTREAM_FORMAT_PCM, num_channels, sample_rate, 16, 1);
    //打开外部DAC，由于要配合PA的启动，需要播放一段空白音
    luat_gpio_set(CODEC_PWR_PIN, 1);
    luat_audio_play_write_blank_raw(0, 2, 1);
    //延迟开启PA
    luat_rtos_timer_start(g_s_delay_timer, 100, 0, app_pa_on, NULL);
    //添加至少2段数据
    audio_add_data();
    audio_add_data();
    while(1)
    {
    	luat_rtos_task_sleep(5000);
    	luat_meminfo_sys(&total, &used, &max_used);
    	g_s_play_point = 44;
    	LUAT_DEBUG_PRINT("%d,%d,%d", total, used, max_used);
        luat_audio_play_start_raw(0, AUSTREAM_FORMAT_PCM, num_channels, sample_rate, 16, 1);
        //打开外部DAC，由于要配合PA的启动，需要播放一段空白音
        luat_gpio_set(CODEC_PWR_PIN, 1);
        luat_audio_play_write_blank_raw(0, 2, 1);
        //延迟开启PA
        luat_rtos_timer_start(g_s_delay_timer, 50, 0, app_pa_on, NULL);
        //添加至少2段数据
        audio_add_data();
        audio_add_data();

    }
}

static void test_audio_demo_init(void)
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
	luat_rtos_task_create(&task_handle, 2048, 20, "test", demo_task, NULL, 0);
}

INIT_TASK_EXPORT(test_audio_demo_init, "1");
