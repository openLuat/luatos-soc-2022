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

#ifndef LUAT_SMS_H
#define LUAT_SMS_H

#include "luat_base.h"

/**
 * @defgroup luatos_sms 短信功能
 * @{
 */
/**
 * @brief 设置SMS的配置信息
 * @param msg_mode
 * @return 0成功,-1失败
 */
int luat_set_sms_msg_mode(uint8_t msg_mode);

/**
 * @brief 获取SMS的配置信息
 * @param msg_mode
 * @return 0成功,-1失败
 */
int luat_get_sms_msg_mode(uint8_t msg_mode);

/**
 * @brief 设置TEXT格式信息的编码格式
 * @param codemode
 * @return 0成功,-1失败
 */
int luat_set_sms_code_mode(char* codemode);

/**
 * @brief 获取TEXT格式信息的编码格式
 * @param codemode
 * @return 0成功,-1失败
 */
int luat_get_sms_code_mode(char* codemode);

/**
 * @brief 发送TEXT格式的信息
 * @param phone_num 号码
 * @param data 信息内容
 * @param codemode 编码模式
 * @return 0成功,-1失败
 */
int luat_sms_send_text_msg(uint8_t *phone_num, uint8_t *data, char* codemode);

/**
 * @brief 发送PDU格式的信息
 * @param phone_num 号码
 * @param data 信息内容
 * @param len 长度 
 * @return 0成功,-1失败
 */
int luat_sms_send_pdu_msg(uint8_t *phone_num,uint8_t *data, int len);

/**
 * @brief 删除信息
 * @param index 信息编号
 * @return 0成功,-1失败
 */
int luat_sms_delete_msg(size_t index);

/**
 * @brief 获取中心地址
 * @param address 地址
 * @return 0成功,-1失败
 */
int luat_get_sms_center_address(uint8_t* address);

/**
 * @brief 设置中心地址
 * @param address 地址
 * @return 0成功,-1失败
 */
int luat_set_sms_center_address(uint8_t* address);

/**
 * @brief 设置短信保存的地址(MEM1/2/3)
 * @param address 地址
 * @return 0成功,-1失败
 */
int luat_set_sms_save_location(uint8_t* address);

/**
 * @brief 列举出所选类型的所有信息
 * @param msg_type 信息的类型
 * @param msg 信息
 * @return 0成功,-1失败
 */
int luat_sms_list_stored_in_sim_sms_msg(uint8_t msg_type, void* msg);

/**@}*/

#endif