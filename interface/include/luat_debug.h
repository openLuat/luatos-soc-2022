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

#ifndef LUAT_DEBUG_H
#define LUAT_DEBUG_H
#include "luat_base.h"
/**
 * @defgroup luatos_debug  调式接口
 * @{
 */

/**
 * @brief 出现异常后系统处理
 * 
 */
typedef enum LUAT_DEBUG_FAULT_MODE
{
	LUAT_DEBUG_FAULT_RESET, /**< 出现异常后重启，批量产品强烈建议用 */
	LUAT_DEBUG_FAULT_HANG, /**< 出现异常后死机，测试阶段强烈建议用 */
	LUAT_DEBUG_FAULT_HANG_RESET, /**< 出现异常后尝试上传死机信息给PC工具，上传成功或者超时后重启，如果对死机复位时间没有要求的，可以在初期批量时选择用这个模式 */
	LUAT_DEBUG_FAULT_SAVE_RESET /**< 出现异常后保存信息到flash，然后重启。目前废弃，行为和LUAT_DEBUG_FAULT_HANG_RESET一致 */
}LUAT_DEBUG_FAULT_MODE_E;

/**
 * @brief 格式打印并输出到LOG口
 * 优先尝试打印soc log	 （USB口输出log    luatools查看）
 * 其次epat log			（USB口输出log    EPAT查看）
 * 最后uart0 log		（UART0口输出log  EPAT查看）
 * 用luatools查看日志时：在中断里最多打印64字节数据，在task内最多打印1k字节数据；在task内调用时，需要一定大小的栈空间
 * 
 * @param fmt 格式
 * @param ... 后续变量
 */
void luat_debug_print(const char *fmt, ...);
/**
 * @brief luat_debug_print宏定义为LUAT_DEBUG_PRINT
 * @param fmt 格式
 * @param ... 后续变量
 */
#define LUAT_DEBUG_PRINT(fmt, argv...) luat_debug_print("%s %d:"fmt, __FUNCTION__,__LINE__, ##argv)

/**
 * @brief 断言处理，并格式打印输出到LOG口
 * 
 * @param fun_name 断言的函数
 * @param line_no 行号
 * @param fmt 格式
 * @param ... 后续变量
 */
void luat_debug_assert(const char *fun_name, unsigned int line_no, const char *fmt, ...);


#define LUAT_DEBUG_ASSERT(condition, fmt, argv...)  do {  \
														{ \
															if((condition) == 0) \
															{ \
																luat_debug_assert(__FUNCTION__, __LINE__, fmt, ##argv); \
															}\
														} \
													} while(0) ///< luat_debug_assert宏定义为LUAT_DEBUG_ASSERT

/**
 * @brief 设置出现异常后系统处理模式
 * 
 * @param mode 处理模式  LUAT_DEBUG_FAULT_RESET 重启模式
                        LUAT_DEBUG_FAULT_HANG  死机模式
 */
void luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_MODE_E mode);

/**
 * @brief 是否开启/停止csdk log
 *
 * @param onoff 开关  0关闭 1打开，开机默认开状态
 */
void luat_debug_print_onoff(unsigned char onoff);

void luat_debug_dump(uint8_t *data, uint32_t len);
/** @}*/

#endif
