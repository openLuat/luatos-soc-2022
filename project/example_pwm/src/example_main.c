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
#include "common_api.h"
#include "luat_rtos.h"
#include "luat_debug.h"

#include "luat_pwm.h"

/*
    1.PWM的3、5通道不能使用
    2.PWM引脚复用使用说明https://doc.openluat.com/wiki/37?wiki_page_id=4785
*/
luat_rtos_task_handle pwm_task_handle;

static int pwm_test_callback(void *pdata, void *param)
{
	LUAT_DEBUG_PRINT("pwm done!");
}

static void task_test_pwm(void *param)
{
	/* 
		出现异常后默认为死机重启
		demo这里设置为LUAT_DEBUG_FAULT_HANG_RESET出现异常后尝试上传死机信息给PC工具，上传成功或者超时后重启
		如果为了方便调试，可以设置为LUAT_DEBUG_FAULT_HANG，出现异常后死机不重启
		但量产出货一定要设置为出现异常重启！！！！！！！！！1
	*/
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG_RESET); 
	luat_rtos_task_sleep(2000);

	/* 
		pwm重构后，精度为千分之一，使用100%占空比时，需要填入1000，50%占空比时候，填入500

		channel: pwm引脚的选择，pwm3，pwm5底层占用，不可使用
		例：  
			传入channel为4， 对10取余得4，和10相除得0，表示使用的是引脚为GPIO27的PWM4
		    传入channel为12，对10取余得2，和10相除得1，表示使用的是引脚为GPIO16的PWM2

			pwm引脚的选择定义参考luat_pwm_ec618.c中  g_s_pwm_table
	*/

	uint8_t channel = 4;
	luat_pwm_set_callback(channel, pwm_test_callback, NULL);
	//测试13M, 50%占空比连续输出，看示波器
	LUAT_DEBUG_PRINT("测试13MHz, 50占空比连续输出，看示波器");
	luat_pwm_open(channel, 13000000, 500, 0);
	luat_rtos_task_sleep(10000);
	LUAT_DEBUG_PRINT("测试1Hz, 50占空比连续输出，输出10个波形停止");
	luat_pwm_open(channel, 1, 500, 10);
	luat_rtos_task_sleep(20000);
	LUAT_DEBUG_PRINT("测试26KHz, 连续输出，占空比每5秒增加1，从0循环到100");
	luat_pwm_open(channel, 26000, 0, 0);
	uint32_t pulse_rate = 0;
    while(1)
	{
        luat_rtos_task_sleep(5000);
        pulse_rate += 10;
        if (pulse_rate > 1000)
        {
        	pulse_rate = 0;
        }
        LUAT_DEBUG_PRINT("当前占空比%u", pulse_rate/10);
        luat_pwm_update_dutycycle(channel, pulse_rate);
	} 
    
}

static void task_demo_pwm(void)
{
    luat_rtos_task_create(&pwm_task_handle, 2048, 20, "pwm", task_test_pwm, NULL, NULL);
}

INIT_TASK_EXPORT(task_demo_pwm,"1");



