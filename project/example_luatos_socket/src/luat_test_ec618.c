#include "luat_network_adapter.h"
#include "common_api.h"
#include "luat_debug.h"
#include "luat_rtos.h"
#include "luat_mobile.h"
#include "net_lwip.h"

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
	luat_mobile_cell_info_t cell_info;
	luat_mobile_signal_strength_info_t signal_info;
	uint8_t csq, i;
	char imsi[20];
	char iccid[24] = {0};
	char apn[32] = {0};
	ip_addr_t ipv4;
	ip_addr_t ipv6;
	switch(event)
	{
	case LUAT_MOBILE_EVENT_CFUN:
		LUAT_DEBUG_PRINT("CFUN消息，status %d", status);
		break;
	case LUAT_MOBILE_EVENT_SIM:
		if (status != LUAT_MOBILE_SIM_NUMBER)
		{
			LUAT_DEBUG_PRINT("SIM卡消息，卡槽%d", index);
		}
		switch(status)
		{
		case LUAT_MOBILE_SIM_READY:
			LUAT_DEBUG_PRINT("SIM卡正常工作");
			luat_mobile_get_iccid(index, iccid, sizeof(iccid));
			LUAT_DEBUG_PRINT("ICCID %s", iccid);
			luat_mobile_get_imsi(index, imsi, sizeof(imsi));
			LUAT_DEBUG_PRINT("IMSI %s", imsi);
			break;
		case LUAT_MOBILE_NO_SIM:
			LUAT_DEBUG_PRINT("SIM卡不存在");
			break;
		case LUAT_MOBILE_SIM_NEED_PIN:
			LUAT_DEBUG_PRINT("SIM卡需要输入PIN码");
			break;
		}
		break;
	case LUAT_MOBILE_EVENT_REGISTER_STATUS:
		LUAT_DEBUG_PRINT("移动网络服务状态变更，当前为%d", status);
		break;
	case LUAT_MOBILE_EVENT_CELL_INFO:
		switch(status)
		{
		case LUAT_MOBILE_CELL_INFO_UPDATE:
			LUAT_DEBUG_PRINT("周期性搜索小区信息完成一次");
			luat_mobile_get_last_notify_cell_info(&cell_info);
			if (cell_info.lte_service_info.cid)
			{
				LUAT_DEBUG_PRINT("服务小区信息 mcc %x mnc %x cellid %u band %d tac %u pci %u earfcn %u is_tdd %d rsrp %d rsrq %d snr %d rssi %d",
						cell_info.lte_service_info.mcc, cell_info.lte_service_info.mnc, cell_info.lte_service_info.cid,
						cell_info.lte_service_info.band, cell_info.lte_service_info.tac, cell_info.lte_service_info.pci, cell_info.lte_service_info.earfcn,
						cell_info.lte_service_info.is_tdd, cell_info.lte_service_info.rsrp, cell_info.lte_service_info.rsrq,
						cell_info.lte_service_info.snr, cell_info.lte_service_info.rssi);
			}
			for (i = 0; i < cell_info.lte_neighbor_info_num; i++)
			{
				if (cell_info.lte_info[i].cid)
				{
					LUAT_DEBUG_PRINT("邻小区 %d mcc %x mnc %x cellid %u tac %u pci %u", i + 1, cell_info.lte_info[i].mcc,
							cell_info.lte_info[i].mnc, cell_info.lte_info[i].cid, cell_info.lte_info[i].tac, cell_info.lte_info[i].pci);
					LUAT_DEBUG_PRINT("邻小区 %d earfcn %u rsrp %d rsrq %d snr %d", i + 1, cell_info.lte_info[i].earfcn, cell_info.lte_info[i].rsrp,
							cell_info.lte_info[i].rsrq, cell_info.lte_info[i].snr);
				}
			}
			break;
		case LUAT_MOBILE_SIGNAL_UPDATE:
			LUAT_DEBUG_PRINT("服务小区信号状态变更");
			luat_mobile_get_last_notify_signal_strength_info(&signal_info);
			luat_mobile_get_last_notify_signal_strength(&csq);
			if (signal_info.luat_mobile_lte_signal_strength_vaild)
			{
				LUAT_DEBUG_PRINT("rsrp %d, rsrq %d, snr %d, rssi %d, csq %d %d", signal_info.lte_signal_strength.rsrp,
						signal_info.lte_signal_strength.rsrq, signal_info.lte_signal_strength.snr,
						signal_info.lte_signal_strength.rssi, csq, luat_mobile_rssi_to_csq(signal_info.lte_signal_strength.rssi));
			}

			break;
		}
		break;
	case LUAT_MOBILE_EVENT_PDP:
		LUAT_DEBUG_PRINT("CID %d PDP激活状态变更为 %d", index, status);
		break;
	case LUAT_MOBILE_EVENT_NETIF:
		LUAT_DEBUG_PRINT("internet工作状态变更为 %d,cause %d", status,index);
		switch (status)
		{
		case LUAT_MOBILE_NETIF_LINK_ON:
			LUAT_DEBUG_PRINT("可以上网");
			luat_socket_check_ready(index, &ipv6);
			if (luat_mobile_get_apn(0, 0, apn, sizeof(apn)))
			{
				LUAT_DEBUG_PRINT("默认apn %s", apn);
			}
			luat_mobile_get_local_ip(0, 1, &ipv4, &ipv6);
			if (ipv4.type != 0xff)
			{
				LUAT_DEBUG_PRINT("IPV4 %s", ip4addr_ntoa(&ipv4.u_addr.ip4));
			}
			if (ipv6.type != 0xff)
			{
				LUAT_DEBUG_PRINT("IPV6 %s", ip6addr_ntoa(&ipv4.u_addr.ip6));
			}
			break;
		default:
			LUAT_DEBUG_PRINT("不能上网");
			break;
		}
		break;
	case LUAT_MOBILE_EVENT_TIME_SYNC:
		if (!status)
		{
			LUAT_DEBUG_PRINT("通过移动网络同步了UTC时间");
		}
		else
		{
			LUAT_DEBUG_PRINT("移动网络同步UTC时间出错");
		}
		break;
	case LUAT_MOBILE_EVENT_CSCON:
		LUAT_DEBUG_PRINT("RRC状态 %d", status);
		break;
	case LUAT_MOBILE_EVENT_NAS_ERROR:
		LUAT_DEBUG_PRINT("NAS异常类型 %d，拒绝原因 %d", index, status);
		break;
	case LUAT_MOBILE_EVENT_FATAL_ERROR:
		LUAT_DEBUG_PRINT("网络需要严重故障，建议在5秒后重启协议栈");
		break;
	default:
		break;
	}
}

static void luat_test_task(void *param)
{
	uint32_t all,now_free_block,min_free_block;
	/* 
		出现异常后默认为死机重启
		demo这里设置为LUAT_DEBUG_FAULT_HANG_RESET出现异常后尝试上传死机信息给PC工具，上传成功或者超时后重启
		如果为了方便调试，可以设置为LUAT_DEBUG_FAULT_HANG，出现异常后死机不重启
		但量产出货一定要设置为出现异常重启！！！！！！！！！1
	*/
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG_RESET); 
	net_lwip_set_tcp_rx_cache(NW_ADAPTER_INDEX_LWIP_GPRS, 32); //为了下行测试才需要打开，对于不需要高速流量的应用不要打开
	g_s_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_network_ctrl, g_s_task_handle, luat_test_socket_callback, NULL);
	network_set_base_mode(g_s_network_ctrl, 1, 15000, 1, 300, 5, 9);
	g_s_network_ctrl->is_debug = 1;	//下行测速时关闭debug，如果只是普通测试，打开debug
	luat_mobile_set_rrc_auto_release_time(3);
	// 请访问 https://netlab.luatos.com 获取新的端口号
	const char remote_ip[] = "112.125.89.8";
	int port = 42025;
	const char hello[] = "hello, luatos!";
	uint8_t *tx_data = malloc(1024);
	uint8_t *rx_data = malloc(1024 * 8);
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

		result = network_connect(g_s_network_ctrl, remote_ip, sizeof(remote_ip) - 1, NULL, port, 30000);
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
								result = network_rx(g_s_network_ctrl, rx_data, 1024 * 8, 0, NULL, NULL, &rx_len);
								if (rx_len > 0)
								{
									LUAT_DEBUG_PRINT("rx %d", rx_len);
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
							luat_meminfo_sys(&all, &now_free_block, &min_free_block);
							LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
						}
					}

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


static void luat_udp_server_test_task(void *param)
{
	g_s_server_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_server_network_ctrl, g_s_server_task_handle, luat_server_test_socket_callback, NULL);
	network_set_base_mode(g_s_server_network_ctrl, 0, 15000, 1, 600, 5, 9);
	network_set_local_port(g_s_server_network_ctrl, server_port);
	g_s_server_network_ctrl->is_debug = 1;
	uint8_t *rx_data = malloc(1024);
	uint32_t tx_len, rx_len;
	ip_addr_t remote_ip;
	uint16_t remote_port;
	int result;
	uint8_t is_break,is_timeout;

	while(1)
	{
		remote_ip = ip6_addr_any;
		result = network_connect(g_s_server_network_ctrl, NULL, 0, &remote_ip, 0, 30000);
		if (!result)
		{
			while(!result)
			{
				result = network_wait_rx(g_s_server_network_ctrl, 30000, &is_break, &is_timeout);
				if (!result)
				{
					if (!is_timeout && !is_break)
					{
						do
						{
							result = network_rx(g_s_server_network_ctrl, rx_data, 1024, 0, &remote_ip, &remote_port, &rx_len);
							if (rx_len > 0)
							{
								network_tx(g_s_server_network_ctrl, rx_data, rx_len, 0, &remote_ip, remote_port, &tx_len, 0);
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
	LUAT_DEBUG_PRINT("%x,%d", event->ID, event->Param1);
	if (event->Param1)
	{
		luat_rtos_event_send(param, UPLOAD_TEST_ERROR, 0, 0, 0, 0);
		return 0;
	}
	switch(event->ID)
	{
	case EV_NW_RESULT_CONNECT:
		luat_rtos_event_send(param, UPLOAD_TEST_CONNECT, 0, 0, 0, 0);
		break;
	case EV_NW_RESULT_TX:
		luat_rtos_event_send(param, UPLOAD_TEST_TX_OK, 0, 0, 0, 0);
		break;
	}
	return 0;
}

/*
 * 异步回调方式，同时做上传速度测试
 */
static void luat_async_test_task(void *param)
{
	uint32_t all,now_free_block,min_free_block;

	luat_event_t event;
	network_ctrl_t *netc = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(netc, NULL, luat_async_test_socket_callback, g_s_upload_test_task_handle);
	network_set_base_mode(netc, 1, 15000, 1, 600, 5, 9);
	const char remote_ip[] = "112.125.89.8";
	uint8_t *upload_buff = malloc(32 * 1024);
	uint32_t tx_len;
	uint64_t start_ms, stop_ms;
	uint8_t cnt;
	int result = network_connect(netc, remote_ip, sizeof(remote_ip) - 1, NULL, 36746, 0);
	if (result < 0)
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
			luat_meminfo_sys(&all, &now_free_block, &min_free_block);
			LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
			network_tx(netc, upload_buff, 32 * 1024, 0, 0, 0, &tx_len, 0);
			cnt = 1;
			start_ms = luat_mcu_tick64_ms();
			while(cnt < 8)
			{
				result = luat_rtos_event_recv(g_s_upload_test_task_handle, UPLOAD_TEST_ERROR, &event, NULL, 10);
				if (!result)
				{
					LUAT_DEBUG_PRINT("test fail %d", result);
					break;
				}
				if ((netc->tx_size - netc->ack_size) < 16 * 1024)
				{
					network_tx(netc, upload_buff, 32 * 1024, 0, 0, 0, &tx_len, 0);
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
						 break;
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
	luat_meminfo_sys(&all, &now_free_block, &min_free_block);
	LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);
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
	//	下行测试，不要和上行测试同步进行
	luat_rtos_task_create(&g_s_task_handle, 2 * 1024, 90, "test", luat_test_task, NULL, 16);
//  服务器功能测试必须打开IPV6，客户端不是必须的
//	luat_mobile_set_default_pdn_ipv6(1);
//	luat_rtos_task_create(&g_s_server_task_handle, 4 * 1024, 10, "server", luat_server_test_task, NULL, 16);
//	luat_rtos_task_create(&g_s_server_task_handle, 4 * 1024, 10, "server", luat_udp_server_test_task, NULL, 16);
//	上行测试
//	luat_rtos_task_create(&g_s_upload_test_task_handle, 2 * 1024, 10, "test2", luat_async_test_task, NULL,0);
//	luat_mobile_set_rrc_auto_release_time(2);
}

INIT_TASK_EXPORT(luat_test_init, "1");
