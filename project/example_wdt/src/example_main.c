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
#include "luat_wdt.h"

static luat_rtos_task_handle uart_task_handle;

static void task_test_wdt(void *param)
{
    luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_RESET);                  //配置看门狗超时时为重启，默认死机
    while (1)
    {
        luat_wdt_feed();                                                //看门狗默认打开，超时时间20s，并且有自动喂狗逻辑，用户不需要去再次启动看门狗，只需根据自身逻辑按时去喂狗即可
        luat_rtos_task_sleep(1000);                            
    }
    luat_rtos_task_delete(uart_task_handle);
    
}

static void task_demo_wdt(void)
{
    luat_rtos_task_create(&uart_task_handle, 2048, 20, "wdt", task_test_wdt, NULL, NULL);
}

INIT_TASK_EXPORT(task_demo_wdt,"1");



