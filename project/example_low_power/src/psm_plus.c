#include "luat_network_adapter.h"
#include "common_api.h"
#include "luat_debug.h"
#include "luat_rtos.h"
#include "luat_mobile.h"
#include "networkmgr.h"
#include "platform_def.h"
#include "luat_pm.h"
#include "luat_gpio.h"


#define PM_TEST_PERIOD	1*60 // 60s唤醒一次 如果有需要可以修改

static ip_addr_t g_s_server_ip;
static network_ctrl_t *g_s_network_ctrl;
static luat_rtos_task_handle g_s_task_handle;

static uint8_t pm_wakeup_by_timer_flag;

// 请访问 https://netlab.luatos.com 获取新的端口号
const char remote_ip[] = "112.125.89.8";
int port = 44462;

static void pm_deep_sleep_timer_callback(uint8_t id)
{
	if (LUAT_PM_DEEPSLEEP_TIMER_ID0 == id)
	{
		LUAT_DEBUG_PRINT("deep sleep timer %d", id);
		pm_wakeup_by_timer_flag = 1;
	}
}

static void luatos_mobile_event_callback(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status)
{
	if (LUAT_MOBILE_EVENT_NETIF == event)
	{
		if (LUAT_MOBILE_NETIF_LINK_ON == status)
		{
			ip_addr_t dns_ip[2];
			uint8_t type, dns_num;
			dns_num = 2;
			soc_mobile_get_default_pdp_part_info(&type, NULL, NULL, &dns_num, dns_ip);

			if (type & 0x80)
			{
				if (index != 4)
				{
					return;
				}
				else
				{
					NmAtiNetifInfo *pNetifInfo = malloc(sizeof(NmAtiNetifInfo));
					NetMgrGetNetInfo(0xff, pNetifInfo);
					if (pNetifInfo->ipv6Cid != 0xff)
					{
						net_lwip_set_local_ip6(&pNetifInfo->ipv6Info.ipv6Addr);
						g_s_server_ip.u_addr.ip6 = pNetifInfo->ipv6Info.ipv6Addr;
						g_s_server_ip.type = IPADDR_TYPE_V6;
						LUAT_DEBUG_PRINT("%s", ipaddr_ntoa(&g_s_server_ip));
					}
					free(pNetifInfo);
				}
			}
			if (dns_num > 0)
			{
				network_set_dns_server(NW_ADAPTER_INDEX_LWIP_GPRS, 2, &dns_ip[0]);
				if (dns_num > 1)
				{
					network_set_dns_server(NW_ADAPTER_INDEX_LWIP_GPRS, 3, &dns_ip[1]);
				}
			}
			net_lwip_set_link_state(NW_ADAPTER_INDEX_LWIP_GPRS, 1);
		}
	}
}

static int32_t luat_test_socket_callback(void *pdata, void *param)
{
	OS_EVENT *event = (OS_EVENT *)pdata;
	LUAT_DEBUG_PRINT("%x", event->ID);
	return 0;
}

static luat_pm_wakeup_pad_isr_callback_t wakeup_pad_callback(int num)
{
	if (LUAT_PM_WAKEUP_PAD_0 == num)
	{

	}
}


static void luat_test_task(void)
{
	luat_pm_wakeup_pad_set_callback(wakeup_pad_callback);
    int result;
    int retry = 0;
	// 唤醒后先关闭wakeup0
	luat_pm_wakeup_pad_cfg_t cfg = {0};
	cfg.pull_up_enable = 1;
	cfg.neg_edge_enable = 1;
	cfg.pos_edge_enable = 0;
	cfg.pull_down_enable = 0;
	luat_pm_wakeup_pad_set(false, LUAT_PM_WAKEUP_PAD_0, &cfg);
    luat_gpio_cfg_t gpio_cfg = {0};
    uint32_t tx_len;
    uint32_t data_len;
    uint8_t * tx_buff = malloc(100);
    int wakeup_reason = luat_pm_get_wakeup_reason();


    if (wakeup_reason > LUAT_PM_WAKEUP_FROM_POR)
	{
        if ((wakeup_reason == LUAT_PM_WAKEUP_FROM_RTC) && pm_wakeup_by_timer_flag)
        {
            data_len = snprintf(tx_buff, 100, "%s", "timer wakeup");
        }
        else if(wakeup_reason == LUAT_PM_WAKEUP_FROM_PAD)
        {
            data_len = snprintf(tx_buff, 100, "%s", "pad wakeup");
        }
        else
        {
            data_len = snprintf(tx_buff, 100, "%s", "other wakeup");
        }
    }
    else
    {
        data_len = snprintf(tx_buff, 100, "%s", "normal wakeup");
    }

    if (luat_pm_deep_sleep_mode_timer_is_running(LUAT_PM_DEEPSLEEP_TIMER_ID0))
    {
        luat_pm_deep_sleep_mode_timer_stop(LUAT_PM_DEEPSLEEP_TIMER_ID0);
    }

	// 设置死机模式
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG);
	// 申请socket资源
	g_s_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_network_ctrl, g_s_task_handle, luat_test_socket_callback, NULL);
	network_set_base_mode(g_s_network_ctrl, 1, 15000, 0, 300, 5, 9);
	g_s_network_ctrl->is_debug = 0;

	// 连接服务器
    while (retry < 3)
    {
       result = network_connect(g_s_network_ctrl, remote_ip, sizeof(remote_ip) - 1, NULL, port, 1000);
       if(!result)
       {
            result = network_tx(g_s_network_ctrl, tx_buff, data_len, 0, NULL, 0, &tx_len, 1000);
            if(!result)
            {
                network_close(g_s_network_ctrl, 1000);
                goto exit;
            }
       }
       network_close(g_s_network_ctrl, 1000);
       retry++;
    }
    
exit:
	gpio_cfg.pin = HAL_GPIO_23;
	gpio_cfg.mode = LUAT_GPIO_INPUT;
	luat_gpio_open(&gpio_cfg);
    pwrKeyHwDeinit(0);
	// 初始化wakeup0，默认上拉，下降沿中断
	cfg.pull_up_enable = 1;
	cfg.neg_edge_enable = 1;
	cfg.pos_edge_enable = 0;
	cfg.pull_down_enable = 0;
	luat_pm_wakeup_pad_set(true, LUAT_PM_WAKEUP_PAD_0, &cfg);
    luat_pm_deep_sleep_mode_timer_start(LUAT_PM_DEEPSLEEP_TIMER_ID0, PM_TEST_PERIOD * 1000);
    luat_pm_set_power_mode(LUAT_PM_POWER_MODE_POWER_SAVER, 0);
    luat_rtos_task_sleep(10000);
    LUAT_DEBUG_PRINT("未进入PSM+，测试失败, 此处用重启来处理进入PSM+失败的逻辑");
    luat_pm_reboot();
    luat_rtos_task_sleep(5000);
	luat_rtos_task_delete(g_s_task_handle);
}

static void luat_test_init(void)
{
	luat_mobile_event_register_handler(luatos_mobile_event_callback);
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
    luat_pm_deep_sleep_mode_register_timer_cb(LUAT_PM_DEEPSLEEP_TIMER_ID0, pm_deep_sleep_timer_callback);
	// 创建task
	luat_rtos_task_create(&g_s_task_handle, 2 * 1024, 50, "test", luat_test_task, NULL, 16);
}

INIT_TASK_EXPORT(luat_test_init, "1");
