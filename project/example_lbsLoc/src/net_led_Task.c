#include "net_led_Task.h"
uint8_t link_UP = 0;
static void mobile_event_callback_t(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status)
{
    switch (event)
    {
    case LUAT_MOBILE_EVENT_NETIF:
        switch (status)
        {
        case LUAT_MOBILE_NETIF_LINK_ON:
            link_UP = 1;
            LUAT_DEBUG_PRINT("网络注册成功");
            break;
        default:
            LUAT_DEBUG_PRINT("网络未注册成功");
            link_UP = 0;
            break;
        }
    case LUAT_MOBILE_EVENT_SIM:
        switch (status)
        {
        case LUAT_MOBILE_SIM_READY:
            LUAT_DEBUG_PRINT("SIM卡已插入");
            break;
        case LUAT_MOBILE_NO_SIM:
        default:
            break;
        }
    default:
        break;
    }
}
void Task_netinfo_call(void)
{
    luat_mobile_event_register_handler(mobile_event_callback_t);
}



static void NET_LED_FUN(void *param)
{
    luat_gpio_cfg_t net_led_cfg;
    luat_gpio_set_default_cfg(&net_led_cfg);

    net_led_cfg.pin=NET_LED_PIN;
    luat_gpio_open(&net_led_cfg);
    while (1)
    {
        if (link_UP)
        {
            luat_gpio_set(NET_LED_PIN,1);
            luat_rtos_task_sleep(500);
            luat_gpio_set(NET_LED_PIN,0);
            luat_rtos_task_sleep(500);
        }
        else
        {
            luat_gpio_set(NET_LED_PIN,0);
            luat_rtos_task_sleep(500);
        }
        
    }
    
}

void NET_LED_Task(void)
{
    luat_rtos_task_handle NET_LED_Task_HANDLE;
    luat_rtos_task_create(&NET_LED_Task_HANDLE,1*1024,20,"NET_LED_TASK",NET_LED_FUN,NULL,NULL);
}