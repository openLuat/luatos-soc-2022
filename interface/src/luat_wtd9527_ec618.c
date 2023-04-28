
#include "clock.h"
#include "slpman.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "luat_pm.h"
#include "luat_gpio.h"
#include "luat_rtos.h"
#include "common_api.h"
#include "luat_debug.h"
#include "luat_wtd9527.h"
#include "platform_define.h"

#define WTD_IRQ_OUTPUT_PIN HAL_GPIO_1   //开发板12口    对应看门狗芯片的中断输入
#define WTD_FEED_PIN HAL_GPIO_11        //开发板14口    对应看门狗芯片的喂狗输入
#define WTD_RESET_PIN HAL_GPIO_9        //开发板13口    对应看门狗芯片的复位输出
#define WTD_MODE_PIN HAL_GPIO_27        //开发板27口    对应看门狗芯片的喂狗模式输入


luat_gpio_cfg_t irq_output_gpio_cfg;
luat_gpio_cfg_t feed_gpio_cfg;
luat_gpio_cfg_t reset_gpio_cfg;
luat_gpio_cfg_t mode_gpio_cfg;

static void* feed_timer;//喂狗输出定时器
static void* irq_poweron_timer;//中断输出定时器
static void* feed_count_timer;//复位定时器

static int s_time_set = 0;

static void wtd_feed_count_cb(uint32_t arg)
{
    luat_wtd9527_feed_wtd();
}

//喂狗回调
static void feed_wtd_cb(uint32_t arg)
{
	luat_gpio_set(WTD_FEED_PIN, 0);
    LUAT_DEBUG_PRINT("Feed Over");
    if (s_time_set)
        s_time_set--;
    LUAT_DEBUG_PRINT("[DIO]s_time_set [%d]", s_time_set);
    if (s_time_set)
    {
        luat_start_rtos_timer(feed_count_timer, 10, 0);
    }
}

//定时器模式主动开机
static void wtd_irq_cb(uint32_t arg)
{
    luat_gpio_set(WTD_IRQ_OUTPUT_PIN, 1);
}

int luat_wtd9527_setup(void)
{
    luat_wtd9527_feed_wtd();
}

int luat_wtd9527_set_wtdmode(int mode)
{
    if (mode)
        luat_gpio_set(WTD_MODE_PIN, 1);
    else
        luat_gpio_set(WTD_MODE_PIN, 0);
        
    return 0;
}

int luat_wtd9527_get_wtdmode(void)
{
    return luat_gpio_get(WTD_MODE_PIN);
}

int luat_wtd9527_feed_wtd(void)
{
	luat_gpio_set(WTD_FEED_PIN, 1);
    luat_start_rtos_timer(feed_timer, 210, 0);
    return 0;
}


int luat_wtd9527_get_wtd_reset_output(void)
{
    return luat_gpio_get(WTD_RESET_PIN);
}


int luat_wtd9527_set_timeout(size_t timeout)
{
    s_time_set = 0;
    if ((timeout % 4 != 0) || !timeout || timeout > 24)
        return 1;

    s_time_set = timeout / 4;
    luat_wtd9527_feed_wtd();
    return 0;
}

int luat_wtd9527_close(void)
{
	luat_gpio_set(WTD_FEED_PIN, 1);
    luat_start_rtos_timer(feed_timer, 410, 0);
    return 0;
}

int luat_wtd9527_irqPinSet(int status)
{
    if (status)
    {
        luat_gpio_set(WTD_IRQ_OUTPUT_PIN, 1);
    }
    else
    {
        luat_gpio_set(WTD_IRQ_OUTPUT_PIN, 0);
        luat_start_rtos_timer(irq_poweron_timer, 100, 0);
    }
    
    return 0;
}

int luat_wtd9527_in_time_mode_powerOn()
{
    luat_wtd9527_irqPinSet(0);
    return 0;
}

/*
    默认模块GPIO引脚配置初始化
*/
void luat_wtd9527_cfg_init()
{
    slpManAONIOPowerOn();
    luat_gpio_set_default_cfg(&irq_output_gpio_cfg);
    luat_gpio_set_default_cfg(&feed_gpio_cfg);
    luat_gpio_set_default_cfg(&reset_gpio_cfg);
    luat_gpio_set_default_cfg(&mode_gpio_cfg);

	irq_output_gpio_cfg.pin = WTD_IRQ_OUTPUT_PIN;//对应XC8P9527的中断输入管脚

	feed_gpio_cfg.pin = WTD_FEED_PIN;//对应XC8P9527的喂狗管脚
    
	reset_gpio_cfg.pin = WTD_RESET_PIN;//对应XC8P9527的复位管脚
	reset_gpio_cfg.mode = LUAT_GPIO_INPUT;//默认输入
    
	mode_gpio_cfg.pin = WTD_MODE_PIN;//对应XC8P9527的模式选择管脚
    
	luat_gpio_open(&irq_output_gpio_cfg);
	luat_gpio_open(&feed_gpio_cfg);
	luat_gpio_open(&reset_gpio_cfg);
	luat_gpio_open(&mode_gpio_cfg);
    
    feed_timer = luat_create_rtos_timer(feed_wtd_cb, NULL, NULL);
    irq_poweron_timer = luat_create_rtos_timer(wtd_irq_cb, NULL, NULL);
    feed_count_timer = luat_create_rtos_timer(wtd_feed_count_cb, NULL, NULL);

	luat_gpio_set(WTD_IRQ_OUTPUT_PIN, 1);
	luat_gpio_set(WTD_FEED_PIN, 0);
	luat_gpio_set(WTD_MODE_PIN, 1);//模式设置
}