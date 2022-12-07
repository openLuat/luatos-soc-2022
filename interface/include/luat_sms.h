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

#define LUAT_SMS_MAX_PDU_SIZE 180
#define LUAT_SMS_MAX_PDU_SIZE                180
#define LUAT_SMS_MAX_LENGTH_OF_ADDRESS_VALUE 40
#define LUAT_SMS_MAX_ADDR_STR_MAX_LEN ((LUAT_SMS_MAX_LENGTH_OF_ADDRESS_VALUE + 1) * 4)
typedef void (*LUAT_SMS_HANDLE_CB)(void*);

typedef struct
{
    LUAT_SMS_HANDLE_CB cb;
}LUAT_SMS_MAIN_CFG_T;

/**
 * @defgroup luatos_sms 短信功能
 * @{
 */
/**
 * @brief 初始化短信
 */
void luat_sms_init(void);

/**
 * @brief 发送短信
 * @param p_input           短信的内容(当 is_pdu = false 时, 只支持英文，数字以及常用符号)
 * @param p_des             接收短信的手机号
 * @param is_pdu            是否是PDU格式的短信(当 false 时, 有效参数为 p_input & pdes, 当 true 时, 有效参数为 p_input & pudLen)
 * @param input_pdu_len     PDU格式短信的长度，注意和p_input长度没有关系
 * @return 0成功,-1失败
 */
int luat_send_msg(uint8_t *p_input, char *p_des, bool is_pdu, int input_pdu_len);

/**
 * @brief 接受短信回调
 * @param callback_fun    回调函数
 */
void luat_sms_recv_msg_register_handler(LUAT_SMS_HANDLE_CB callback_fun);

#endif