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
 * @brief 定时器模式设定开机时间
 * @param timeout 开机时间
 * @return int =0成功，其他失败
 */
int luat_wtd9527_set_timeout(size_t timeout);

/**
 * @brief 关闭看门狗喂狗
 * @return int =0成功，其他失败
 */
int luat_wtd9527_close(void);

/**
 * @brief 看门狗初始化设置
 * @param wtd_feed_pin 模块的喂狗管脚
 */
void luat_wtd9527_cfg_init(int wtd_feed_pin);

#endif
