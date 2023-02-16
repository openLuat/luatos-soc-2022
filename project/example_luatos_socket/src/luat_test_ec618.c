#include "luat_network_adapter.h"
#include "common_api.h"
#include "luat_debug.h"
#include "luat_rtos.h"
#include "luat_mobile.h"
#include "networkmgr.h"
static luat_rtos_task_handle g_s_task_handle;
static network_ctrl_t *g_s_network_ctrl;
static luat_rtos_task_handle g_s_server_task_handle;
static network_ctrl_t *g_s_server_network_ctrl;
static ip_addr_t g_s_server_ip;
static uint16_t server_port = 15000;
static int32_t luat_test_socket_callback(void *pdata, void *param)
{
	OS_EVENT *event = (OS_EVENT *)pdata;
	LUAT_DEBUG_PRINT("%x", event->ID);
	return 0;
}

static int32_t luat_server_test_socket_callback(void *pdata, void *param)
{
	OS_EVENT *event = (OS_EVENT *)pdata;
	LUAT_DEBUG_PRINT("%x", event->ID);
	return 0;
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

static void luat_test_task(void *param)
{
	g_s_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_network_ctrl, g_s_task_handle, luat_test_socket_callback, NULL);
	network_set_base_mode(g_s_network_ctrl, 1, 15000, 1, 300, 5, 9);
	g_s_network_ctrl->is_debug = 1;
	const char remote_ip[] = "2603:c023:1:5fcc:c028:8ed:49a7:6e08";
	const char hello[] = "hello, luatos!";
	uint8_t *tx_data = malloc(1024);
	uint8_t *rx_data = malloc(1024);
	uint32_t tx_len, rx_len, cnt;
	uint64_t uplink, downlink;
	int result;
	uint8_t is_break,is_timeout;
	cnt = 0;
	while(1)
	{
		result = network_wait_link_up(g_s_network_ctrl, 60000);
		if (result)
		{
			continue;
		}

		result = network_connect(g_s_network_ctrl, remote_ip, sizeof(remote_ip) - 1, NULL, 51288, 30000);
		if (!result)
		{
			result = network_tx(g_s_network_ctrl, hello, sizeof(hello) - 1, 0, NULL, 0, &tx_len, 15000);
			if (!result)
			{
				while(!result)
				{
					result = network_wait_rx(g_s_network_ctrl, 5000, &is_break, &is_timeout);
					if (!result)
					{
						if (!is_timeout && !is_break)
						{
							do
							{
								result = network_rx(g_s_network_ctrl, rx_data, 1024, 0, NULL, NULL, &rx_len);
								if (rx_len > 0)
								{
									LUAT_DEBUG_PRINT("%.*s", rx_len, rx_data);
								}
							}while(!result && rx_len > 0);
						}
						else if (is_timeout)
						{
							sprintf(tx_data, "test %u cnt", cnt);
							result = network_tx(g_s_network_ctrl, tx_data, strlen(tx_data), 0, NULL, 0, &tx_len, 15000);
							cnt++;
							if (!(cnt % 10))
							{
								luat_mobile_get_ip_data_traffic(&uplink, &downlink);
								LUAT_DEBUG_PRINT("%u,%u", (uint32_t)uplink, (uint32_t)downlink);
								luat_mobile_clear_ip_data_traffic(1, 1);
							}
						}
					}
				}
			}
		}
		LUAT_DEBUG_PRINT("网络断开，5秒后重试");
		network_close(g_s_network_ctrl, 5000);
		luat_rtos_task_sleep(15000);
	}
}

static void luat_server_test_task(void *param)
{
	g_s_server_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_server_network_ctrl, g_s_server_task_handle, luat_server_test_socket_callback, NULL);
	network_set_base_mode(g_s_server_network_ctrl, 1, 15000, 1, 600, 5, 9);
	network_set_local_port(g_s_server_network_ctrl, server_port);
	g_s_server_network_ctrl->is_debug = 1;
	uint8_t *rx_data = malloc(1024);
	uint32_t tx_len, rx_len;
	int result;
	uint8_t is_break,is_timeout;
	while(1)
	{
		result = network_listen(g_s_server_network_ctrl, 0xffffffff);
		if (!result)
		{
			network_socket_accept(g_s_server_network_ctrl, NULL);
			LUAT_DEBUG_PRINT("client %s, %u", ipaddr_ntoa(&g_s_server_network_ctrl->remote_ip), g_s_server_network_ctrl->remote_port);
			while(!result)
			{
				result = network_wait_rx(g_s_server_network_ctrl, 30000, &is_break, &is_timeout);
				if (!result)
				{
					if (!is_timeout && !is_break)
					{
						do
						{
							result = network_rx(g_s_server_network_ctrl, rx_data, 1024, 0, NULL, NULL, &rx_len);
							if (rx_len > 0)
							{
								network_tx(g_s_server_network_ctrl, rx_data, rx_len, 0, NULL, 0, &tx_len, 0);
							}
						}while(!result && rx_len > 0);

					}
				}
			}
		}
		LUAT_DEBUG_PRINT("网络断开，重试");
		network_close(g_s_server_network_ctrl, 5000);
	}
}


static void luat_test_init(void)
{
	luat_mobile_event_register_handler(luatos_mobile_event_callback);
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
	luat_rtos_task_create(&g_s_task_handle, 2 * 1024, 10, "test", luat_test_task, NULL, 16);
	luat_rtos_task_create(&g_s_server_task_handle, 4 * 1024, 10, "server", luat_server_test_task, NULL, 16);
	luat_mobile_set_default_pdn_ipv6(1);
}

INIT_TASK_EXPORT(luat_test_init, "1");
