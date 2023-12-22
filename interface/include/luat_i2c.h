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

#ifndef LUAT_I2C_H
#define LUAT_I2C_H

#include "luat_base.h"
#include "luat_i2c_legacy.h"

/**
 * @ingroup luatos_device 外设接口
 * @{
 */
/**
 * @defgroup luatos_device_i2c I2C接口
 * @{
*/

/**
 * @brief 检查i2c是否存在
 * 
 * @param id i2c_id
 * @return 1存在 0不存在
 */
int luat_i2c_exist(int id);


/**
 * @brief 设置i2c复用
 * @attention 如果 id 为1 value 为 1，复用到GPIO4，GPIO5(pin80,pin81)；value为其他,复用到 GPIO8,GPIO9(pin52,pin50);
 * @attention 如果 id 为0 value 为 1，复用到 GPIO12,GPIO13（pin58，pin57）;value 2,复用到GPIO16，GPIO17(pin22,pin23)，value为其他,复用到模块pin67，pin66
 * @param id i2c_id
 * @return -1 失败 其他正常
 */

int luat_i2c_set_iomux(int id, uint8_t value);

/**
 * @brief 初始化i2c
 * 
 * @param id i2c_id
 * @param speed i2c 速度
 * @param slaveaddr i2c 从地址
 * @return 0成功 其他失败
 */
int luat_i2c_setup(int id, int speed);

/**
 * @brief 关闭 i2c
 * 
 * @param id i2c_id
 * @return 0成功 其他失败
 */
int luat_i2c_close(int id);

/**
 * @brief I2C 发送数据
 * 
 * @param id i2c_id
 * @param addr 7位设备地址
 * @param buff 数据buff
 * @param len 数据长度
 * @param stop 是否发送停止位
 * @return 0成功 其他失败
 */
int luat_i2c_send(int id, int addr, void* buff, size_t len, uint8_t stop);
/**
 * @brief I2C 接受数据
 * 
 * @param id i2c_id
 * @param addr 7位设备地址
 * @param buff 数据buff
 * @param len 数据长度
 * @return 0成功 其他失败
 */
int luat_i2c_recv(int id, int addr, void* buff, size_t len);
/**
 * @brief I2C 数据收发函数，如果reg,reg_len 存在，为读取寄存器的值，不存在为发送数据
 * 
 * @param id i2c_id
 * @param addr 7位设备地址
 * @param reg 要读取的寄存器，可以是数组
 * @param reg_len 寄存器的长度
 * @param buff 数据buff，如果reg,reg_len 存在，为接收的数据，不存在为发送的数据
 * @param len 数据长度
 * @return 0成功 其他失败
 */
int luat_i2c_transfer(int id, int addr, uint8_t *reg, size_t reg_len, uint8_t *buff, size_t len);
/** @}*/
/** @}*/
#endif
