#include "luat_base.h"
#include "luat_lcd.h"
#include "luat_gpio.h"
#include "luat_spi.h"
// #include "luat_malloc.h"
#include "common_api.h"

// #define LUAT_LOG_TAG "ili9486"
// #include "luat_log.h"

#define LCD_W 320
#define LCD_H 480
#define LCD_DIRECTION 0

static int ili9486_init(luat_lcd_conf_t* conf) {
    if (conf->w == 0)
        conf->w = LCD_W;
    if (conf->h == 0)
        conf->h = LCD_H;
    if (conf->direction == 0)
        conf->direction = LCD_DIRECTION;

    if (conf->pin_pwr != 255)
        luat_gpio_mode(conf->pin_pwr, Luat_GPIO_OUTPUT, Luat_GPIO_DEFAULT, Luat_GPIO_LOW); // POWER
    luat_gpio_mode(conf->pin_dc, Luat_GPIO_OUTPUT, Luat_GPIO_DEFAULT, Luat_GPIO_HIGH); // DC
    luat_gpio_mode(conf->pin_rst, Luat_GPIO_OUTPUT, Luat_GPIO_DEFAULT, Luat_GPIO_LOW); // RST

    if (conf->pin_pwr != 255)
        luat_gpio_set(conf->pin_pwr, Luat_GPIO_LOW);
    luat_gpio_set(conf->pin_rst, Luat_GPIO_LOW);
    luat_rtos_task_sleep(100);
    luat_gpio_set(conf->pin_rst, Luat_GPIO_HIGH);
    luat_rtos_task_sleep(120);

    // // 发送初始化命令
    lcd_write_cmd(conf,0xE0);
    lcd_write_data(conf,0x00);
    lcd_write_data(conf,0x07);
    lcd_write_data(conf,0x0f);
    lcd_write_data(conf,0x0D);
    lcd_write_data(conf,0x1B);
    lcd_write_data(conf,0x0A);
    lcd_write_data(conf,0x3c);
    lcd_write_data(conf,0x78);
    lcd_write_data(conf,0x4A);
    lcd_write_data(conf,0x07);
    lcd_write_data(conf,0x0E);
    lcd_write_data(conf,0x09);
    lcd_write_data(conf,0x1B);
    lcd_write_data(conf,0x1e);
    lcd_write_data(conf,0x0f);

    lcd_write_cmd(conf,0xE1);
    lcd_write_data(conf,0x00);
    lcd_write_data(conf,0x22);
    lcd_write_data(conf,0x24);
    lcd_write_data(conf,0x06);
    lcd_write_data(conf,0x12);
    lcd_write_data(conf,0x07);
    lcd_write_data(conf,0x36);
    lcd_write_data(conf,0x47);
    lcd_write_data(conf,0x47);
    lcd_write_data(conf,0x06);
    lcd_write_data(conf,0x0a);
    lcd_write_data(conf,0x07);
    lcd_write_data(conf,0x30);
    lcd_write_data(conf,0x37);
    lcd_write_data(conf,0x0f);

    lcd_write_cmd(conf,0xC0);
    lcd_write_data(conf,0x10);
    lcd_write_data(conf,0x10);

    lcd_write_cmd(conf,0xC1);
    lcd_write_data(conf,0x41);

    lcd_write_cmd(conf,0xC5);
    lcd_write_data(conf,0x00);
    lcd_write_data(conf,0x22);
    lcd_write_data(conf,0x80);

    lcd_write_cmd(conf,0x36);
    if(conf->direction==0)lcd_write_data(conf,0x48);
    else if(conf->direction==1)lcd_write_data(conf,0x88);
    else if(conf->direction==2)lcd_write_data(conf,0x28);
    else lcd_write_data(conf,0xE8);

    lcd_write_cmd(conf,0x3A);
    lcd_write_data(conf,0x55);

    lcd_write_cmd(conf,0XB0);
    lcd_write_data(conf,0x00);
    lcd_write_cmd(conf,0xB1);
    lcd_write_data(conf,0xB0);
    lcd_write_data(conf,0x11);
    lcd_write_cmd(conf,0xB4);
    lcd_write_data(conf,0x02);
    lcd_write_cmd(conf,0xB6);
    lcd_write_data(conf,0x02);
    lcd_write_data(conf,0x02);

    lcd_write_cmd(conf,0xB7);
    lcd_write_data(conf,0xC6);

    lcd_write_cmd(conf,0xE9);
    lcd_write_data(conf,0x00);

    lcd_write_cmd(conf,0XF7);
    lcd_write_data(conf,0xA9);
    lcd_write_data(conf,0x51);
    lcd_write_data(conf,0x2C);
    lcd_write_data(conf,0x82);

    /* Sleep Out */
    lcd_write_cmd(conf,0x11);
    /* wait for power stability */
    luat_rtos_task_sleep(100);
    luat_lcd_clear(conf,BLACK);
    /* display on */
    luat_lcd_display_on(conf);
    return 0;
};

const luat_lcd_opts_t lcd_opts_ili9486 = {
    .name = "ili9486",
    .init = ili9486_init,
};

