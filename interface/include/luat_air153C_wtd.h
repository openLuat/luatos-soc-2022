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

#ifndef LUAT_AIR153C_WTD_H
#define LUAT_AIR153C_WTD_H

#include "luat_base.h"
/**
 * @brief 开启看门狗
 * @return int =0成功，其他失败
 */
int luat_air153C_wtd_setup(void);

/**
 * @brief 喂狗
 * @return int =0成功，其他失败
 */
int luat_air153C_wtd_feed_wtd(void);

/**
 * @brief 关闭看门狗喂狗
 * @return int =0成功，其他失败
 */
int luat_air153C_wtd_close(void);

/**
 * @brief 看门狗初始化设置
 * @param wtd_feed_pin 模块的喂狗管脚
 */
void luat_air153C_wtd_cfg_init(int wtd_feed_pin);

/**
 * @brief 设置看门狗喂狗次数
 * @param timeout 次数以4的倍数，例如 8 就是喂狗2次
 */
int luat_air153C_wtd_set_timeout(size_t timeout);

#endif
