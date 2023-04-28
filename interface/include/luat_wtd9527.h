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

#ifndef LUAT_WTD9527_H
#define LUAT_WTD9527_H

#include "luat_base.h"
/**
 * @brief 开启看门狗
 * @return int =0成功，其他失败
 */
int luat_wtd9527_setup(void);

/**
 * @brief 喂狗
 * @return int =0成功，其他失败
 */
int luat_wtd9527_feed_wtd(void);

/**
 * @brief 设置看门狗模式
 * @param mode 1: 看门狗模式/0: 定时器模式
 * @return int =0成功，其他失败
 */
int luat_wtd9527_set_wtdmode(int mode);

/**
 * @brief 获取看门狗模式
 * @return 1: 看门狗模式 | 0: 定时器模式
 */
int luat_wtd9527_get_wtdmode(void);

/**
 * @brief 获取复位管脚输出
 * @return 1: 输出高电平 | 0: 输出低电平
 */
int luat_wtd9527_get_wtd_reset_output(void);

/**
 * @brief 定时器模式设定开机时间
 * @param timeout 开机时间
 * @return int =0成功，其他失败
 */
int luat_wtd9527_set_timeout(size_t timeout);

/**
 * @brief 定时器模式主动开机
 * @return int =0成功，其他失败
 */
int luat_wtd9527_in_time_mode_powerOn();

/**
 * @brief 中断输出设置上下拉
 * @param status 0：下拉 | 1：上拉
 * @return int =0成功，其他失败
 */
int luat_wtd9527_irqPinSet(int status);

/**
 * @brief 关闭看门狗喂狗
 * @return int =0成功，其他失败
 */
int luat_wtd9527_close(void);


void luat_wtd9527_cfg_init();

#endif
