#include "luat_network_adapter.h"
#include "common_api.h"
#include "luat_debug.h"
#include "luat_rtos.h"
#include "luat_mobile.h"
#include "networkmgr.h"

enum
{
	UPLOAD_TEST_CONNECT = 1,
	UPLOAD_TEST_TX_OK = 2,
	UPLOAD_TEST_ERROR,
};

static luat_rtos_task_handle g_s_task_handle;
static network_ctrl_t *g_s_network_ctrl;
static luat_rtos_task_handle g_s_server_task_handle;
static network_ctrl_t *g_s_server_network_ctrl;
static ip_addr_t g_s_server_ip;
static uint16_t server_port = 15000;
static luat_rtos_task_handle g_s_upload_test_task_handle;
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
	uint32_t all,now_free_block,min_free_block;
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG);
	g_s_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_network_ctrl, g_s_task_handle, luat_test_socket_callback, NULL);
	network_set_base_mode(g_s_network_ctrl, 1, 15000, 1, 300, 5, 9);
	g_s_network_ctrl->is_debug = 1;
	const char remote_ip[] = "112.125.89.8";
	const char hello[] = "hello, luatos!";
	uint8_t *tx_data = malloc(1024);
	uint8_t *rx_data = malloc(1024);
	uint32_t tx_len, rx_len, cnt;
	uint64_t uplink, downlink;
	int result;
	uint8_t is_break,is_timeout;
	cnt = 0;
	if (g_s_upload_test_task_handle) //上行测试时，需要延迟一段时间
	{
		luat_rtos_task_sleep(300000);
	}
	while(1)
	{
		luat_meminfo_sys(&all, &now_free_block, &min_free_block);
		LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
		result = network_wait_link_up(g_s_network_ctrl, 60000);
		if (result)
		{
			continue;
		}

		result = network_connect(g_s_network_ctrl, remote_ip, sizeof(remote_ip) - 1, NULL, 35228, 30000);
		if (!result)
		{
			result = network_tx(g_s_network_ctrl, hello, sizeof(hello) - 1, 0, NULL, 0, &tx_len, 15000);
			if (!result)
			{
				while(!result)
				{
					result = network_wait_rx(g_s_network_ctrl, 20000, &is_break, &is_timeout);
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
					luat_meminfo_sys(&all, &now_free_block, &min_free_block);
					LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
				}
			}
		}
		LUAT_DEBUG_PRINT("网络断开，15秒后重试");
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

static int32_t luat_async_test_socket_callback(void *pdata, void *param)
{
	OS_EVENT *event = (OS_EVENT *)pdata;
	LUAT_DEBUG_PRINT("%x", event->ID);
	return 0;
}

/*
 * 异步回调方式，同时做上传速度测试
 */
static void luat_async_test_task(void *param)
{
	luat_event_t event;
	network_ctrl_t *netc = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(netc, NULL, luat_server_test_socket_callback, g_s_upload_test_task_handle);
	network_set_base_mode(netc, 1, 15000, 1, 600, 5, 9);
	const char remote_ip[] = "112.125.89.8";
	uint8_t *upload_buff = malloc(16 * 1024);
	uint32_t tx_len;
	uint64_t start_ms, stop_ms;
	uint8_t cnt;
	int result = network_connect(netc, remote_ip, sizeof(remote_ip) - 1, NULL, 36746, 0);
	if (result)
	{
		LUAT_DEBUG_PRINT("test fail %d", result);
	}
	else
	{
		result = luat_rtos_event_recv(g_s_upload_test_task_handle, UPLOAD_TEST_CONNECT, &event, NULL, 180000);
		if (result)
		{
			LUAT_DEBUG_PRINT("connect fail %d", result);
		}
		else
		{
			network_tx(netc, upload_buff, 16 * 1024, 0, 0, 0, &tx_len, 0);
			cnt = 1;
			start_ms = luat_mcu_tick64_ms();
			while(cnt < 16)
			{
				result = luat_rtos_event_recv(g_s_upload_test_task_handle, UPLOAD_TEST_ERROR, &event, NULL, 10);
				if (!result)
				{
					LUAT_DEBUG_PRINT("test fail %d", result);
					break;
				}
				if ((netc->tx_size - netc->ack_size) < 8 * 1024)
				{
					network_tx(netc, upload_buff, 16 * 1024, 0, 0, 0, &tx_len, 0);
					cnt++;
				}
			}
			while(netc->tx_size > netc->ack_size)
			{
				result = luat_rtos_event_recv(g_s_upload_test_task_handle, UPLOAD_TEST_TX_OK, &event, NULL, 1000);
				if (!result)
				{
					if (netc->tx_size <= netc->ack_size)
					{
						 stop_ms = luat_mcu_tick64_ms();
						 LUAT_DEBUG_PRINT("tx %llubyte in %llums", netc->tx_size, stop_ms-start_ms);
					}
				}
				else
				{
					LUAT_DEBUG_PRINT("test fail %d", result);
					break;
				}
			}

		}
	}
	network_force_close_socket(netc);
	network_release_ctrl(netc);
	luat_rtos_task_delete(g_s_upload_test_task_handle);
}

static void luat_test_init(void)
{
	luat_mobile_event_register_handler(luatos_mobile_event_callback);
	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);
	luat_rtos_task_create(&g_s_task_handle, 2 * 1024, 10, "test", luat_test_task, NULL, 16);
//	luat_rtos_task_create(&g_s_server_task_handle, 4 * 1024, 10, "server", luat_server_test_task, NULL, 16);
//	上行测试
	luat_rtos_task_create(&g_s_upload_test_task_handle, 2 * 1024, 10, "test2", luat_async_test_task, NULL,0);
	luat_mobile_set_default_pdn_ipv6(1);
//	luat_mobile_set_rrc_auto_release_time(2);
}

INIT_TASK_EXPORT(luat_test_init, "1");
