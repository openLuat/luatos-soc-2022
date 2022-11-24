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


luat_rtos_task_handle pwm_task_handle;

static void task_test_pwm(void *param)
{
    luat_rtos_task_sleep(2000);
    switch(luat_pwm_open(4,1,50,1))
    {
        case -1:
            LUAT_DEBUG_PRINT("pwm channel err");
        break;
        case -2:
            LUAT_DEBUG_PRINT("pwm period err");
        break;
        case -3:
            LUAT_DEBUG_PRINT("pwm pulse err");
        break;
        case -4:
            LUAT_DEBUG_PRINT("pwm hardware timer used");
        break;
        default :
            break;
    }

    while(1)
	{
        luat_rtos_task_sleep(1000);
		LUAT_DEBUG_PRINT("==================pwm run1==================");
	}
    
}

static void task_demo_pwm(void)
{
    luat_rtos_task_create(&pwm_task_handle, 1024, 20, "pwm", task_test_pwm, NULL, NULL);
}

INIT_TASK_EXPORT(task_demo_pwm,"1");



