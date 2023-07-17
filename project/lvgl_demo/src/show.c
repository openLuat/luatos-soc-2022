#include "common_api.h"
#include "soc_lcd.h"

#include "luat_base.h"
#include "luat_rtos.h"
#include "luat_mcu.h"
#include "luat_spi.h"
#include "luat_debug.h"
#include "luat_gpio.h"

#include "spi_lcd_common.h"

#include "lv_conf.h"
#include "lvgl.h"

#define SPI_LCD_FPS		(16)		//最高只能
#define SPI_LCD_BUS_ID	0
#define SPI_LCD_SPEED	25600000
#define SPI_LCD_CS_PIN	HAL_GPIO_24
#define SPI_LCD_DC_PIN	HAL_GPIO_8
#define SPI_LCD_POWER_PIN HAL_GPIO_25
#define SPI_LCD_BL_PIN HAL_GPIO_26
#define SPI_LCD_SDA_PIN HAL_GPIO_9
#define SPI_LCD_SCL_PIN HAL_GPIO_11
#define SPI_LCD_W	240
#define SPI_LCD_H	320
#define SPI_LCD_X_OFFSET	0
#define SPI_LCD_Y_OFFSET	0
#define LVGL_FLUSH_TIME	(50)

enum
{
	LVGL_FLUSH_EVENT = 1,
};

static spi_lcd_ctrl_t g_s_spi_lcd =
{
		SPI_LCD_W,
		SPI_LCD_H,
		SPI_LCD_BUS_ID,
		SPI_LCD_CS_PIN,
		SPI_LCD_DC_PIN,
		SPI_LCD_POWER_PIN,
		SPI_LCD_BL_PIN,
		0,
		SPI_LCD_X_OFFSET,
		SPI_LCD_Y_OFFSET
};

static const LCD_DrawStruct g_s_draw =
{
		.Speed = SPI_LCD_SPEED,
		.SpiID = SPI_LCD_BUS_ID,
		.Mode = 0,
		.ColorMode = 0,
		.DCPin = SPI_LCD_DC_PIN,
		.CSPin = SPI_LCD_CS_PIN,
		.xoffset = SPI_LCD_X_OFFSET,
		.yoffset = SPI_LCD_Y_OFFSET,
};

typedef struct
{
	luat_rtos_task_handle h_lvgl_task;
	luat_rtos_timer_t h_lvgl_timer;
	uint8_t is_sleep;
	uint8_t wait_flush;
}lvgl_ctrl_t;

static lvgl_ctrl_t g_s_lvgl;

static LUAT_RT_RET_TYPE lvgl_flush_timer_cb(LUAT_RT_CB_PARAM)
{
	if (g_s_lvgl.wait_flush < 2)
	{
		g_s_lvgl.wait_flush++;
		luat_send_event_to_task(g_s_lvgl.h_lvgl_task, LVGL_FLUSH_EVENT, 0, 0, 0);
	}
}

static void lvgl_lcd_init(void)
{
	int result = spi_lcd_detect(SPI_LCD_SDA_PIN, SPI_LCD_SCL_PIN, SPI_LCD_CS_PIN, SPI_LCD_DC_PIN, SPI_LCD_POWER_PIN);
	DBG("%d", result);
	luat_spi_t spi_cfg = {
			.id = SPI_LCD_BUS_ID,
			.bandrate = SPI_LCD_SPEED,
			.CPHA = 0,
			.CPOL = 0,
			.mode = 1,
			.dataw = 8,
			.cs = SPI_LCD_CS_PIN,
	};
	luat_spi_setup(&spi_cfg);
	spi_lcd_init_auto(&g_s_spi_lcd, result);
}

//static void lvgl_task_test(void *param)
//{
//	size_t total, used, max_used;
//	uint16_t pos, color[3], i,j;
//	LCD_DrawStruct *draw;
//	PV_Union u_color;
//	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG_RESET);
//	lvgl_lcd_init();
//	color[0] = 0xf800;
//	color[1] = 0x07e0;
//	color[2] = 0x001f;
//	i = 0;
//	while(1)
//	{
//
//    	luat_meminfo_sys(&total, &used, &max_used);
//    	LUAT_DEBUG_PRINT("meminfo total %d, used %d, max_used%d",total, used, max_used);
//		for(pos = 0; pos < g_s_spi_lcd.h_max; pos+=40)
//		{
//			draw = calloc(sizeof(LCD_DrawStruct), 1);
//			*draw = g_s_draw;
//			u_color.p = malloc(g_s_spi_lcd.w_max * 40 * 2);
//			draw->Data = u_color.pu8;
//			for(j = 0; j < g_s_spi_lcd.w_max * 40; j++)
//			{
//				u_color.pu16[j] = color[i];
//			}
//			draw->x1 = 0;
//			draw->x2 = g_s_spi_lcd.w_max - 1;
//			draw->y1 = pos;
//			draw->y2 = pos + 39;
//			draw->Size = g_s_spi_lcd.w_max * 40 * 2;
//			LCD_Draw(draw);
//		}
//		luat_rtos_task_sleep(55);
//		i++;
//		i = i%3;
//	}
//}

static void lvgl_task(void *param)
{
	luat_event_t event;
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG_RESET);
	lvgl_lcd_init();
	luat_start_rtos_timer(g_s_lvgl.h_lvgl_timer, 30, 1);
	lv_init();
	while(1)
	{
		luat_wait_event_from_task(g_s_lvgl.h_lvgl_task, 0, &event, NULL, LUAT_WAIT_FOREVER);
		if (g_s_lvgl.wait_flush) g_s_lvgl.wait_flush--;
		lv_timer_handler();
	}
}


void lvgl_init(void)
{

	luat_gpio_cfg_t gpio_cfg;

	LCD_ServiceInit(60);
	luat_rtos_task_create(&g_s_lvgl.h_lvgl_task, 8196, 50, "lvgl", lvgl_task, NULL, 16);
	g_s_lvgl.h_lvgl_timer = luat_create_rtos_timer(lvgl_flush_timer_cb, NULL, NULL);

	luat_gpio_set_default_cfg(&gpio_cfg);
	gpio_cfg.pin = SPI_LCD_CS_PIN;
	gpio_cfg.output_level = LUAT_GPIO_HIGH;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.pin = SPI_LCD_DC_PIN;
	gpio_cfg.output_level = LUAT_GPIO_LOW;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.pin = SPI_LCD_POWER_PIN;
	luat_gpio_open(&gpio_cfg);
	gpio_cfg.pin = SPI_LCD_BL_PIN;
	luat_gpio_open(&gpio_cfg);


}

INIT_TASK_EXPORT(lvgl_init, "1");
