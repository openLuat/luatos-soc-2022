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
#ifndef LUAT_PWM_H
#define LUAT_PWM_H
/**
 *@version V1.0
 *@attention
 */
/**
 * @ingroup luatos_device 外设接口
 * @{
 */

/**
 * @defgroup luatos_device_PWM PWM接口
 * @{
*/
#include "luat_base.h"
/**
 * @brief PWM控制参数
*/
typedef struct luat_pwm_conf {
    int channel;       /**<PWM通道*/
    size_t period;   /**<频率, 1-26000hz*/
    size_t pulse;    /**<占空比*/
    size_t pnum;     /**<输出周期 0为持续输出, 1为单次输出, 其他为指定脉冲数输出*/
    size_t precision;  /**<分频精度, 100/256/1000, 默认为100, 若设备不支持会有日志提示*/
} luat_pwm_conf_t;


/**
 * @brief 配置pwm 参数
 * 
 * @param id i2c_id
 * @return int 
 */

int luat_pwm_setup(luat_pwm_conf_t* conf);

/**
 * @brief 打开pwm 通道
 * 
 * @param id i2c_id
 * @return int 
 */

int luat_pwm_open(int channel, size_t period, size_t pulse, int pnum);

/**
 * @brief 获取pwm 频率
 * 
 * @param id i2c_id
 * @return int 
 */

int luat_pwm_capture(int channel,int freq);

/**
 * @brief 关闭pwm 接口
 * 
 * @param id i2c_id
 * @return int 
 */
int luat_pwm_close(int channel);
/** @}*/
/** @}*/
#endif
