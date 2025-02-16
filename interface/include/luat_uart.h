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

#ifndef LUAT_UART_H
#define LUAT_UART_H

#include "luat_base.h"
#include "luat_uart_legacy.h"
/**
 *@version V1.0
 *@attention
 *上报接收数据中断的逻辑：
 * 1.串口初始化时，新建一个缓冲区
 * 2.可以考虑多为用户申请几百字节的缓冲长度，用户处理时防止丢包
 * 3.每次串口收到数据时，先存入缓冲区，记录长度
 * 4.遇到以下情况时，再调用串口中断
    a)缓冲区满（帮用户多申请的的情况）/缓冲区只剩几百字节（按实际长度申请缓冲区的情况）
    b)收到fifo接收超时中断（此时串口数据应该是没有继续收了）
 * 5.触发收到数据中断时，返回的数据应是缓冲区的数据
 * 6.关闭串口时，释放缓冲区资源
 */
/**
 * @ingroup luatos_device 外设接口
 * @{
 */
/**
 * @defgroup luatos_device_uart UART接口
 * @{
 */

/**
 * @brief 校验位
 */
#define LUAT_PARITY_NONE                     0  /**< 无校验 */
#define LUAT_PARITY_ODD                      1  /**< 奇校验 */
#define LUAT_PARITY_EVEN                     2  /**< 偶校验 */

/**
 * @brief 高低位顺序
 */
#define LUAT_BIT_ORDER_LSB                   0  /**< 低位有效 */
#define LUAT_BIT_ORDER_MSB                   1  /**< 高位有效 */

/**
 * @brief 停止位
 */
#define LUAT_UART_STOP_BIT1                   1   /**<1 */
#define LUAT_UART_STOP_BIT2                   2   /**<2 */

#define LUAT_VUART_ID_0						0x20    
#define LUAT_UART_RX_ERROR_DROP_DATA		(0xD6)
#define LUAT_UART_DEBUG_ENABLE				(0x3E)
/**
 * @brief luat_uart
 * @attention uart0 为底层日志口接口，如果确需使用并明白所带来的后果， 请调用soc_uart0_set_log_off(1)关闭底层日志口，具体实例参见project/example_uart demo;
 */
typedef struct luat_uart {
    int id;                     /**< 串口id */
    int baud_rate;              /**< 波特率 */

    uint8_t data_bits;          /**< 数据位 */
    uint8_t stop_bits;          /**< 停止位,可设置为1或2，1：1位停止位，2：2位停止位 */
    uint8_t bit_order;          /**< 高低位 */
    uint8_t parity;             /**< 奇偶校验位 0 不进行奇偶校验 1 奇校验 2 偶校验 */

    size_t bufsz;               /**< 接收数据缓冲区大小，最小支持2046(默认值) 最大支持8K(8*1024) */
    uint32_t pin485;            /**< 转换485的pin, 如果没有则是0xffffffff*/
    uint32_t delay;             /**< 485翻转延迟时间，单位us */
    uint8_t rx_level;           /**< 接收方向的电平 */
    uint8_t debug_enable;		/**< 是否开启debug功能 ==LUAT_UART_DEBUG_ENABLE开启，其他不开启 */
    uint8_t error_drop;			/**< 遇到错误是否放弃数据 ==LUAT_UART_RX_ERROR_DROP_DATA 放弃，其他不放弃*/
} luat_uart_t;

/**
 * @brief uart初始化
 * 
 * @param uart luat_uart结构体
 * @return int 
 */
int luat_uart_setup(luat_uart_t* uart);

/**
 * @brief 串口写数据
 * 
 * @param uart_id 串口id
 * @param data 数据
 * @param length 数据长度
 * @return int 实际写入的长度,通常不用检查,只有uart_id错误才会有问题
 */
int luat_uart_write(int uart_id, void* data, size_t length);

/**
 * @brief 串口读数据
 * 
 * @param uart_id 串口id
 * @param buffer 数据
 * @param length 数据长度
 * @return int 读取到的实际长度，如果buffer为NULL，为接收缓存里当前的数据长度
 */
int luat_uart_read(int uart_id, void* buffer, size_t length);

/**
 * @brief 清除uart的接收缓存数据
 * @return 无
 */
void luat_uart_clear_rx_cache(int uart_id);

/**
 * @brief 关闭串口
 * 
 * @param uart_id 串口id
 * @return int 
 */
int luat_uart_close(int uart_id);

/**
 * @brief 检测串口是否存在
 * 
 * @param uart_id 串口id
 * @return int 
 */
int luat_uart_exist(int uart_id);

/**
 * @brief 串口控制参数
 */
typedef enum LUAT_UART_CTRL_CMD
{
    LUAT_UART_SET_RECV_CALLBACK,/**< 接收回调 */
    LUAT_UART_SET_SENT_CALLBACK/**< 发送回调 */
}LUAT_UART_CTRL_CMD_E;

/**
 * @brief 接收回调函数，对于带有休眠唤醒功能的uart，如果data_len==0说明唤醒了，但是用来唤醒uart的数据可能会丢失，需要再次发送
 * @note ec618的uart1在600,1200,2400,4800,9600波特率下可以休眠继续接收数据，因此不会有唤醒提醒，直接返回接收到数据，不丢数据。其他波特率则只有唤醒功能
 * 
 */
typedef void (*luat_uart_recv_callback_t)(int uart_id, uint32_t data_len);

/**
 * @brief 发送回调函数
 * 
 */
typedef void (*luat_uart_sent_callback_t)(int uart_id, void *param);

/**
 * @brief 串口控制参数
 * 
 */
typedef struct luat_uart_ctrl_param
{
    luat_uart_recv_callback_t recv_callback_fun;/**< 接收回调函数 */
    luat_uart_sent_callback_t sent_callback_fun;/**< 发送回调函数 */
}luat_uart_ctrl_param_t;

/**
 * @brief 串口控制
 * 
 * @param uart_id 串口id
 * @param cmd 串口控制命令
 * @param param 串口控制参数
 * @return int 
 */
int luat_uart_ctrl(int uart_id, LUAT_UART_CTRL_CMD_E cmd, void* param);

/**
 * @brief 串口复用函数，目前支持UART0，UART2
 * 
 * @param uart_id 串口id
 * @param use_alt_type 如果为1，UART0，复用到GPIO16,GPIO17;UART2复用到GPIO12 GPIO13；如果为2，UART2复用到GPIO6 GPIO7
 * @return int 0 失败，其他成功
 */
int luat_uart_pre_setup(int uart_id, uint8_t use_alt_type);

/** @}*/
/** @}*/
#endif
