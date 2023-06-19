/*
本文件实现的业务逻辑如下：
1. 开机进入IPV6
2. 按照"水熊虫"服务文档水熊虫服务(低功耗唤醒服务) 上报,imei,iccid,ipv6地址，并且在网络切换的时候再次上报
3. 默认进入"响应优先"休眠模式，客户可以选择"均衡模式"
4. 收到服务器下发，模块数据后打印在luatools (客户通过网页可以下发唤醒)
5. 收到服务器下发数据后，链接IPV4的TCP服务器，并且转发到ipv4服务器，转发后关闭TCP(使用https://netlab.luatos.com/模拟客户的ipv4 服务端)
6. 可以通过IO 唤醒模块，并打印在luatools
7. 收到IO 唤醒后，链接IPV4的TCP服务器，并且转发到ipv4服务器，转发后关闭TCP

*/

#include "luat_network_adapter.h"
#include "common_api.h"
#include "luat_debug.h"
#include "luat_rtos.h"
#include "luat_mobile.h"
#include "networkmgr.h"
#include "cmidev.h"
#include "luat_pm.h"

// #define  LUAT_TRANS_IPV6ADDR_TO_BYTE(ipv6, ipv6_bytes)   \
//   do{                                                     \
//         UINT16  ipv6U16 = 0;                              \
//         ipv6U16 = IP6_ADDR_BLOCK1((ipv6));   \
//         ipv6_bytes[0] = (UINT8)((ipv6U16 >> 8)&0xFF);   \
//         ipv6_bytes[1] = (UINT8)(ipv6U16 & 0xFF);        \
//         ipv6U16 = IP6_ADDR_BLOCK2((ipv6));      \
//         ipv6_bytes[2] = (UINT8)((ipv6U16 >> 8)&0xFF);   \
//         ipv6_bytes[3] = (UINT8)(ipv6U16 & 0xFF);        \
//         ipv6U16 = IP6_ADDR_BLOCK3((ipv6));      \
//         ipv6_bytes[4] = (UINT8)((ipv6U16 >> 8)&0xFF);   \
//         ipv6_bytes[5] = (UINT8)(ipv6U16 & 0xFF);         \
//         ipv6U16 = IP6_ADDR_BLOCK4((ipv6));      \
//         ipv6_bytes[6] = (UINT8)((ipv6U16 >> 8)&0xFF);  \
//         ipv6_bytes[7] = (UINT8)(ipv6U16 & 0xFF);   \
//         ipv6U16 = IP6_ADDR_BLOCK5((ipv6));   \
//         ipv6_bytes[8] = (UINT8)((ipv6U16 >> 8)&0xFF);  \
//         ipv6_bytes[9] = (UINT8)(ipv6U16 & 0xFF);   \
//         ipv6U16 = IP6_ADDR_BLOCK6((ipv6));  \
//         ipv6_bytes[10] = (UINT8)((ipv6U16 >> 8)&0xFF);   \
//         ipv6_bytes[11] = (UINT8)(ipv6U16 & 0xFF);    \
//         ipv6U16 = IP6_ADDR_BLOCK7((ipv6));   \
//         ipv6_bytes[12] = (UINT8)((ipv6U16 >> 8)&0xFF);  \
//         ipv6_bytes[13] = (UINT8)(ipv6U16 & 0xFF);    \
//         ipv6U16 = IP6_ADDR_BLOCK8((ipv6));   \
//         ipv6_bytes[14] = (UINT8)((ipv6U16 >> 8)&0xFF);  \
//         ipv6_bytes[15] = (UINT8)(ipv6U16 & 0xFF);  \
//     }while(0)



typedef struct app_client_send_data
{
    char *data;
    uint32_t len;
}app_client_send_data_t;


static int test = 0;

static luat_rtos_task_handle g_s_client_task_handle;
static char* g_s_remote_server_addr = "114.55.242.59";
static int g_s_remote_server_port = 45000;
// static char* g_s_remote_server_addr = "112.125.89.8";
// static int g_s_remote_server_port = 36794;
static int g_s_is_ipv6_ready = 0;
static luat_rtos_semaphore_t g_s_ipv6_ready_semaphore = NULL;
static luat_rtos_semaphore_t g_s_client_wakeup_semaphore = NULL;
static network_ctrl_t *g_s_network_ctrl;


static luat_rtos_task_handle g_s_server_task_handle;
static network_ctrl_t *g_s_server_network_ctrl;
static ip_addr_t g_s_server_ip;
static uint16_t server_port = 45000;
#define SERVER_RX_BUF_SIZE (1024)
static uint8_t *g_s_server_rx_data = NULL;

// 自定义的业务客户端和服务器参数
static luat_rtos_task_handle g_s_app_client_task_handle;
static char* g_s_app_remote_server_addr = "112.125.89.8";
static int g_s_app_remote_server_port = 36078;
static network_ctrl_t *g_s_app_network_ctrl;

// 按键唤醒标记
static uint8_t g_s_pad_wakeup_flag = 0;
static luat_rtos_task_handle g_s_key_wakeup_task_handle;
static luat_rtos_semaphore_t g_s_key_wakeup_semaphore = NULL;

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
			LUAT_DEBUG_PRINT("type %x", type);

			g_s_is_ipv6_ready = 0;

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

						// 如果ipv6地址发生了改变，通知client task将新的ip地址上报到业务服务器
						if (memcmp(&(g_s_server_ip.u_addr.ip6), &(pNetifInfo->ipv6Info.ipv6Addr), sizeof(pNetifInfo->ipv6Info.ipv6Addr)) != 0)
						{
							luat_rtos_semaphore_release(g_s_client_wakeup_semaphore);
						}
						
						g_s_server_ip.u_addr.ip6 = pNetifInfo->ipv6Info.ipv6Addr;
						g_s_server_ip.type = IPADDR_TYPE_V6;
						LUAT_DEBUG_PRINT("%s", ipaddr_ntoa(&g_s_server_ip));

						g_s_is_ipv6_ready = 1;
						luat_rtos_semaphore_release(g_s_ipv6_ready_semaphore);
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
		else if(LUAT_MOBILE_NETIF_LINK_OFF == status)
		{
			g_s_is_ipv6_ready = 0;
		}
	}
}

int app_client_send_data(const char *data, uint32_t len)
{
	if(data==NULL || len==0)
	{
		return -1;
	}
	app_client_send_data_t *data_item = (app_client_send_data_t *)malloc(sizeof(app_client_send_data_t));
	LUAT_DEBUG_ASSERT(data_item != NULL,"malloc data_item fail");;

	data_item->data = malloc(len);
	LUAT_DEBUG_ASSERT(data_item->data != NULL, "malloc data_item.data fail");
	memcpy(data_item->data, data, len);
	data_item->len = len;

	int ret = luat_rtos_message_send(g_s_app_client_task_handle, 0, data_item);

	if(ret != 0)
	{
		free(data_item->data);
		data_item->data = NULL;
		free(data_item);
		data_item = NULL;
	}

	return ret;
}


static void luat_tcp_server_task(void *param)
{
	g_s_server_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_server_network_ctrl, g_s_server_task_handle, luat_server_test_socket_callback, NULL);
	network_set_base_mode(g_s_server_network_ctrl, 1, 15000, 1, 600, 5, 9);
	network_set_local_port(g_s_server_network_ctrl, server_port);
	g_s_server_network_ctrl->is_debug = 1;

	if (!g_s_server_rx_data)
	{
		g_s_server_rx_data = malloc(SERVER_RX_BUF_SIZE);
	}
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
							result = network_rx(g_s_server_network_ctrl, g_s_server_rx_data, SERVER_RX_BUF_SIZE, 0, NULL, NULL, &rx_len);
							if (rx_len > 0)
							{
								uint8_t *rx_hex_data = malloc(rx_len*2+1);
								memset(rx_hex_data, 0, rx_len*2+1);
								for (size_t i = 0; i < rx_len; i++)
								{
									sprintf(rx_hex_data+i*2, "%02X", g_s_server_rx_data[i]);
								}
								LUAT_DEBUG_PRINT("rx %d: %s", rx_len, rx_hex_data);
								free(rx_hex_data);


								// 前缀+结束包
								if ((rx_len >= 12) && (memcmp(g_s_server_rx_data, "\x4C\x50\x00\x01", 4)==0) && (memcmp(g_s_server_rx_data+rx_len-4, "\x00\x00\x00\x00", 4)==0))
								{
									// 正在解析的参数len字段值
									uint16_t param_len = 0;
									// 正在解析的数据在整个报文中的索引位置，调过4字节前缀，从第5字节开始
									uint16_t rx_index = 4;

									while (1)
									{
										// 初步根据长度来判断剩余的数据是否可以完整解析一个参数（一个参数最小需要占用4个字节，id+len字段）
										if (rx_index+8 > rx_len)
										{
											break;
										}

										param_len = *(g_s_server_rx_data+rx_index+2)*256 + *(g_s_server_rx_data+rx_index+3);
										if (rx_index+8+param_len > rx_len)
										{
											break;
										}


										if (memcmp(g_s_server_rx_data+rx_index, "\x00\x83", 2)==0)
										{
											LUIAT_DEBUG_PRINT("server wakeup");
											luat_rtos_semaphore_release(g_s_client_wakeup_semaphore);
										}
										else if (memcmp(g_s_server_rx_data+rx_index, "\x02\x01", 2)==0)
										{
											
										}
										else
										{
											LUAT_DEBUG_PRINT("unsupport cmd");										
										}

										rx_index += 4+param_len;
									}
									

								}
								else
								{
									LUAT_DEBUG_PRINT("invalid head or tail");
								}

								
								// 接收到正确的唤醒报文
								// if ((rx_len >= sizeof(wakeup_cmd)) && (memcmp(g_s_server_rx_data, "\x4C\x50\x00\x01", 4)==0) && (memcmp(g_s_server_rx_data+rx_len-4, "\x00\x00\x00\x00", 4)==0))
								// {
								// 	luat_rtos_semaphore_release(g_s_client_wakeup_semaphore);
								// }
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

static void luat_udp_server_task(void *param)
{
	g_s_server_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_server_network_ctrl, g_s_server_task_handle, luat_server_test_socket_callback, NULL);
	network_set_base_mode(g_s_server_network_ctrl, 0, 15000, 1, 600, 5, 9);
	network_set_local_port(g_s_server_network_ctrl, server_port);
	g_s_server_network_ctrl->is_debug = 1;

	if (!g_s_server_rx_data)
	{
		g_s_server_rx_data = malloc(SERVER_RX_BUF_SIZE);
	}
	
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
							result = network_rx(g_s_server_network_ctrl, g_s_server_rx_data, SERVER_RX_BUF_SIZE, 0, &remote_ip, &remote_port, &rx_len);
							if (rx_len > 0)
							{
								
								uint8_t *rx_hex_data = malloc(rx_len*2+1);
								memset(rx_hex_data, 0, rx_len*2+1);
								for (size_t i = 0; i < rx_len; i++)
								{
									sprintf(rx_hex_data+i*2, "%02X", g_s_server_rx_data[i]);
								}
								LUAT_DEBUG_PRINT("rx %d: %s", rx_len, rx_hex_data);
								free(rx_hex_data);


								// 前缀+结束包
								if ((rx_len >= 12) && (memcmp(g_s_server_rx_data, "\x4C\x50\x00\x01", 4)==0) && (memcmp(g_s_server_rx_data+rx_len-4, "\x00\x00\x00\x00", 4)==0))
								{
									// 正在解析的参数len字段值
									uint16_t param_len = 0;
									// 正在解析的数据在整个报文中的索引位置，调过4字节前缀，从第5字节开始
									uint16_t rx_index = 4;

									while (1)
									{
										// 初步根据长度来判断剩余的数据是否可以完整解析一个参数（一个参数最小需要占用4个字节，id+len字段）
										if (rx_index+8 > rx_len)
										{
											break;
										}

										param_len = *(g_s_server_rx_data+rx_index+2)*256 + *(g_s_server_rx_data+rx_index+3);
										if (rx_index+8+param_len > rx_len)
										{
											break;
										}


										// 唤醒包
										if (memcmp(g_s_server_rx_data+rx_index, "\x00\x83", 2)==0)
										{
											luat_rtos_semaphore_release(g_s_client_wakeup_semaphore);
											// app_client_send_data(g_s_server_rx_data+rx_index+4, 4);
										}
										// 自定义数据包
										else if (memcmp(g_s_server_rx_data+rx_index, "\x02\x01", 2)==0)
										{
											app_client_send_data(g_s_server_rx_data+rx_index+4, param_len);
										}
										else
										{
											LUAT_DEBUG_PRINT("unsupport cmd");
										}

										rx_index += 4+param_len;
									}
									

								}
								else
								{
									LUAT_DEBUG_PRINT("invalid head or tail");
								}

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


static void luat_app_client_task(void *param)
{
	// 申请并且配置一个网络适配器
	g_s_app_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_app_network_ctrl, g_s_app_client_task_handle, NULL, NULL);
	network_set_base_mode(g_s_app_network_ctrl, 1, 15000, 1, 600, 5, 9);
	g_s_app_network_ctrl->is_debug = 1;


	uint8_t rx_data[50] = {0};
	uint32_t  tx_len, rx_len, cnt;
	int result;
	uint8_t is_break,is_timeout;
	cnt = 0;

	while (1)
	{
		uint32_t message_id;
		app_client_send_data_t *data_item;

		if(luat_rtos_message_recv(g_s_app_client_task_handle, &message_id, (void **)&data_item, LUAT_WAIT_FOREVER) == 0)
		{
			// 阻塞+超时60秒等待网络环境准备就绪，0表示成功
			while (network_wait_link_up(g_s_app_network_ctrl, 60000) != 0)
			{
				LUAT_DEBUG_PRINT("wait network link up");
			}
			
			while (1)
			{
				if (0 != network_connect(g_s_app_network_ctrl, g_s_app_remote_server_addr, strlen(g_s_app_remote_server_addr), NULL, g_s_app_remote_server_port, 30000))
				{
					LUAT_DEBUG_PRINT("connect fail, retry after 5 seconds");
					network_close(g_s_app_network_ctrl, 5000);
					luat_rtos_task_sleep(5000);
					continue;
				}

				if (0 != network_tx(g_s_app_network_ctrl, data_item->data, data_item->len, 0, NULL, 0, &tx_len, 15000))
				{
					LUAT_DEBUG_PRINT("send fail, retry after 5 seconds");
					network_close(g_s_network_ctrl, 5000);
					luat_rtos_task_sleep(5000);
					continue;
				}				

				network_close(g_s_app_network_ctrl, 5000);
				break;
			}
			

			free(data_item->data);
			data_item->data = NULL;
			free(data_item);
			data_item = NULL;
		}
	}
	

	luat_rtos_task_delete(g_s_app_client_task_handle);
}

static void luat_key_wakeup_task(void *param)
{
	while (1)
	{
		if (g_s_pad_wakeup_flag)
		{
			g_s_pad_wakeup_flag = 0;
			app_client_send_data("wakeup from wakeup0", strlen("wakeup from wakeup0"));
		}
		else
		{
			luat_rtos_semaphore_take(g_s_key_wakeup_semaphore, LUAT_WAIT_FOREVER);
		}
	}
	
}

//wakepad回调函数
static luat_pm_wakeup_pad_isr_callback_t pad_wakeup_callback(LUAT_PM_WAKEUP_PAD_E num)
{
    LUAT_DEBUG_PRINT("pm demo wakeup pad num %d", num);
	if (LUAT_PM_WAKEUP_PAD_0 == num)
	{
		g_s_pad_wakeup_flag = 1;
		luat_rtos_semaphore_release(g_s_key_wakeup_semaphore);
	}	
}



static void luat_client_task(void *param)
{
	/* 
		出现异常后默认为死机重启
		demo这里设置为LUAT_DEBUG_FAULT_HANG_RESET出现异常后尝试上传死机信息给PC工具，上传成功或者超时后重启
		如果为了方便调试，可以设置为LUAT_DEBUG_FAULT_HANG，出现异常后死机不重启
		但量产出货一定要设置为出现异常重启！！！！！！！！！
	*/
	luat_debug_set_fault_mode(LUAT_DEBUG_FAULT_HANG_RESET); 


	if (NULL == g_s_ipv6_ready_semaphore)
	{
		luat_rtos_semaphore_create(&g_s_ipv6_ready_semaphore, 1);
	}
	if (NULL == g_s_client_wakeup_semaphore)
	{
		luat_rtos_semaphore_create(&g_s_client_wakeup_semaphore, 1);
	}
	

	// 申请并且配置一个网络适配器
	g_s_network_ctrl = network_alloc_ctrl(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_init_ctrl(g_s_network_ctrl, g_s_client_task_handle, luat_test_socket_callback, NULL);
	network_set_base_mode(g_s_network_ctrl, 0, 15000, 0, 0, 0, 0);
	g_s_network_ctrl->is_debug = 1;


	uint8_t rx_data[50] = {0};
	uint32_t  tx_len, rx_len, cnt;
	int result;
	uint8_t is_break,is_timeout;
	cnt = 0;

	while(1)
	{
		while(1)
		{
			// 打印ram信息
			uint32_t all,now_free_block,min_free_block;
			luat_meminfo_sys(&all, &now_free_block, &min_free_block);
			LUAT_DEBUG_PRINT("meminfo %d,%d,%d",all,now_free_block,min_free_block);

			// 阻塞+超时60秒等待网络环境准备就绪，0表示成功
			result = network_wait_link_up(g_s_network_ctrl, 60000);
			if (result)
			{
				continue;
			}

			// 本次开机运行过程中是否已经设置过休眠模式
			// 网络准备就绪后，才能调用接口设置休眠模式；
			// 有些休眠参数需要在飞行模式下进行设置;
			// 使用此标志变量来保证仅设置一次；
			static uint8_t has_set_power_mode = 0;
			if (!has_set_power_mode)
			{
				has_set_power_mode = 1;
				
				// 设置为响应优先休眠模式
				luat_pm_set_power_mode(LUAT_PM_POWER_MODE_HIGH_PERFORMANCE, 0);
			}
			

			// 等待ipv6准备就绪
			if (!g_s_is_ipv6_ready)
			{
				LUAT_DEBUG_PRINT("wait ipv6 ready semaphore");
				luat_rtos_semaphore_take(g_s_ipv6_ready_semaphore, LUAT_WAIT_FOREVER);
				continue;
			}

			// 创建ipv6 server task，启动一个ipv6服务器，用来接受ipv6客户端的连接以及发送过来的唤醒报文或者其他自定义的报文

			// 需要注意的是：必须使用开通ipv6服务的sim卡，才能使用ipv6服务；
			// 如果使用大于等于2.2.2版本的Luatools，勾选 清除kv分区 和 清除文件系统分区，直接烧录运行本程序，可以打印出来类似于以下的日志信息：
			// [2023-06-06 18:32:22.745][000000005.052] luatos_mobile_event_callback 63:240E:46C:8920:1225:1766:B4E:9D8D:DDBD
			// 最后8组以:分割的16进制字符串为ipv6地址，则表示测试使用的sim卡支持ipv6服务；

			// 如果没有以下日志信息，按照以下步骤排查：	
			// 1、手机卡一般都支持ipv6，可以在手机浏览器中访问http://www.test-ipv6.com/，如果可以显示ipv6地址，则表示支持，否则打运营商客服电话开通;
			//    确认手机卡支持后，可以用手机卡来测试；
			// 2、比较老的物联网卡可能不支持ipv6，因为物联网卡一般都有机卡绑定功能，所以无法把物联网卡插到手机中按照第1种情况去验证是否支持；
			//    可以直接咨询卡商
			//    或者
			//    设备通过usb插入电脑，电脑通过设备的rndis上网，电脑浏览器访问http://www.test-ipv6.com/，如果可以显示ipv6地址，则表示支持，否则咨询卡商开通；
			//    确认物联网卡支持后，可以用物联网卡来测试；
			if (!g_s_server_task_handle)
			{
				// luat_rtos_task_create(&g_s_server_task_handle, 4 * 1024, 10, "server", luat_tcp_server_task, NULL, 16); // ipv6 tcp server
				luat_rtos_task_create(&g_s_server_task_handle, 4 * 1024, 10, "server", luat_udp_server_task, NULL, 16); // ipv6 udp server
			}

			// 创建自定义业务的ipv4客户端，可以连接自定义的ipv4业务服务器，发送登录报文、唤醒报文、ipv6客户端下发的自定义数据等报文到ipv4业务服务器
			// 在具体项目中，本客户端需要根据客户的业务逻辑自行修改调整
			if (!g_s_app_client_task_handle)
			{
				luat_rtos_task_create(&g_s_app_client_task_handle, 2 * 1024, 90, "app_client", luat_app_client_task, NULL, 16);	
			}

			

			if (NULL == g_s_key_wakeup_semaphore)
			{
				luat_rtos_semaphore_create(&g_s_key_wakeup_semaphore, 1);
			}
			// 创建按键唤醒处理task
			if (!g_s_key_wakeup_task_handle)
			{
				luat_rtos_task_create(&g_s_key_wakeup_task_handle, 2 * 1024, 90, "key_wakeup", luat_key_wakeup_task, NULL, NULL);	
			}
			// 设置wakeup0中断回调函数
			// 配置为下降沿触发中断
			// 使用低功耗开发板测试时，将wakeup0管脚gnd短接即可触发一次中断
			luat_pm_wakeup_pad_set_callback(pad_wakeup_callback);
			luat_pm_wakeup_pad_cfg_t cfg = {0};
			cfg.neg_edge_enable = 1;
			cfg.pos_edge_enable = 0;
			cfg.pull_down_enable = 0;
			cfg.pull_up_enable = 1;
			luat_pm_wakeup_pad_set(1, LUAT_PM_WAKEUP_PAD_0, &cfg);
				

			

			// 连接服务器
			result = network_connect(g_s_network_ctrl, g_s_remote_server_addr, strlen(g_s_remote_server_addr), NULL, g_s_remote_server_port, 30000);
			if (!result)
			{
				uint8_t register_packet_data[120] = {0};
				uint32_t register_packet_len = 0;

				
				// 固定前缀+注册包类型
				sprintf(register_packet_data, "LP%c%c%c%c%c%c", 0x00, 0x01, 0x00, 0x01, 0x00, 0x00);
				register_packet_len += 8;

				//IMEI
				char imei[20] = {0};
				int imei_len = luat_mobile_get_imei(0, imei, sizeof(imei));
				if (imei_len > 0)
				{
					imei_len = strlen(imei);
				}
				
				snprintf(register_packet_data+register_packet_len, sizeof(register_packet_data)-register_packet_len, "%c%c%c%c%s", 0x01, 0x01, 0x00, (char)imei_len, imei);
				register_packet_len = register_packet_len+4+imei_len;

				//ICCID
				int sim_id = 0;
				luat_mobile_get_sim_id(&sim_id);
				char iccid[24] = {0};
				int iccid_len = luat_mobile_get_iccid(sim_id, iccid, sizeof(iccid));	
				if (iccid_len > 0)
				{
					iccid_len = strlen(iccid);
				}	
				snprintf(register_packet_data+register_packet_len, sizeof(register_packet_data)-register_packet_len, "%c%c%c%c%s", 0x01, 0x02, 0x00, (char)iccid_len, iccid);
				register_packet_len = register_packet_len+4+iccid_len;

				//IPV6
				// uint8_t ipv6_bytes[16] = {0};
				// int ipv6_len = 0;

				// NmAtiNetifInfo *pNetifInfo = malloc(sizeof(NmAtiNetifInfo));
				// NetMgrGetNetInfo(0xff, pNetifInfo);
				// if (pNetifInfo->ipv6Cid != 0xff)
				// {
				// 	net_lwip_set_local_ip6(&pNetifInfo->ipv6Info.ipv6Addr);
				// 	LUAT_TRANS_IPV6ADDR_TO_BYTE(&(pNetifInfo->ipv6Info.ipv6Addr), ipv6_bytes);
				// 	ipv6_len = 16;
				// }
				// free(pNetifInfo);

				ip_addr_t server_ip;
				char server_ip_str[45] = {0};
				NmAtiNetifInfo *pNetifInfo = malloc(sizeof(NmAtiNetifInfo));
				NetMgrGetNetInfo(0xff, pNetifInfo);
				if (pNetifInfo->ipv6Cid != 0xff)
				{
					server_ip.u_addr.ip6 = pNetifInfo->ipv6Info.ipv6Addr;
					server_ip.type = IPADDR_TYPE_V6;
					snprintf(server_ip_str, sizeof(server_ip_str)-1, "%s", ipaddr_ntoa(&server_ip));
				}
				free(pNetifInfo);



		
				snprintf(register_packet_data+register_packet_len, sizeof(register_packet_data)-register_packet_len, "%c%c%c%c%s", 
					0x01, 0x03, 0x00, (char)strlen(server_ip_str), server_ip_str);
				register_packet_len = register_packet_len+4+strlen(server_ip_str);
				// if (ipv6_len > 0)
				// {
				// 	snprintf(register_packet_data+register_packet_len, sizeof(register_packet_data)-register_packet_len, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 
				// 		ipv6_bytes[0],ipv6_bytes[1],ipv6_bytes[2],ipv6_bytes[3],
				// 		ipv6_bytes[4],ipv6_bytes[5],ipv6_bytes[6],ipv6_bytes[7],
				// 		ipv6_bytes[8],ipv6_bytes[9],ipv6_bytes[10],ipv6_bytes[11],
				// 		ipv6_bytes[12],ipv6_bytes[13],ipv6_bytes[14],ipv6_bytes[15]);
				// 	register_packet_len += ipv6_len;
				// }

				// 发送注册报文到服务器
				result = network_tx(g_s_network_ctrl, register_packet_data, register_packet_len, 0, NULL, 0, &tx_len, 15000);
				if (!result)
				{
					while(!result)
					{
						// 等待服务器应答注册报文
						result = network_wait_rx(g_s_network_ctrl, 20000, &is_break, &is_timeout);
						if (!result)
						{
							if (!is_timeout && !is_break)
							{
								cnt = 0;

								do
								{
									uint8_t register_ack[] = {'L', 'P', 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
									// 读取服务器的应答数据
									result = network_rx(g_s_network_ctrl, rx_data, sizeof(rx_data), 0, NULL, NULL, &rx_len);
									if (rx_len > 0)
									{
										LUAT_DEBUG_PRINT("rx %d", rx_len);
										// 接收到正确的应答报文，退出本task
										if ((rx_len == sizeof(register_ack)) && (memcmp(register_ack, rx_data, rx_len)==0))
										{
											network_close(g_s_network_ctrl, 5000);
											luat_rtos_semaphore_take(g_s_client_wakeup_semaphore, 100);
											goto CLIENT_TASK_WAIT_NEXT_WAKEUP;
										}	
									}
								}while(!result && rx_len > 0);
							}
							else if (is_timeout)
							{
								result = network_tx(g_s_network_ctrl, register_packet_data, register_packet_len, 0, NULL, 0, &tx_len, 15000);
								cnt++;
								if (cnt >= 10)
								{
									// 重试10次，仍然接收失败，如何处理，直接重启？
									// luat_pm_reboot();
								}
							}
						}

					}
				}
			}
			LUAT_DEBUG_PRINT("网络断开，15秒后重试");
			network_close(g_s_network_ctrl, 5000);
			luat_rtos_task_sleep(15000);
		}

CLIENT_TASK_WAIT_NEXT_WAKEUP:
		LUAT_DEBUG_PRINT("wait next wakeup");
		luat_rtos_semaphore_take(g_s_client_wakeup_semaphore, LUAT_WAIT_FOREVER);
	}

	luat_rtos_task_delete(g_s_client_task_handle);
}


static void flymode_task(void *arg)
{
	luat_rtos_task_sleep(5000);
	LUAT_DEBUG_PRINT("entry");
	while (1)
	{
		luat_rtos_task_sleep(60000);
		LUAT_DEBUG_PRINT("enter flymode");
		luat_mobile_set_flymode(0,1);
		luat_rtos_task_sleep(5000);
		luat_mobile_set_flymode(0,0);
		LUAT_DEBUG_PRINT("exit flymode");
	}
}


static void ipv6_sleep_wakeup_init(void)
{
	// 注册网络状态事件处理函数
	luat_mobile_event_register_handler(luatos_mobile_event_callback);

	net_lwip_init();
	net_lwip_register_adapter(NW_ADAPTER_INDEX_LWIP_GPRS);
	network_register_set_default(NW_ADAPTER_INDEX_LWIP_GPRS);

    // 软件上开启ipv6功能；此接口需要在网络环境准备就绪前调用
	luat_mobile_set_default_pdn_ipv6(1);

    // 创建ipv4 client task，用来上报imei、iccid、ipv6 server地址给服务器；
	// 同时在此task主函数中还会创建ipv6 server task和业务client task；
	luat_rtos_task_create(&g_s_client_task_handle, 2 * 1024, 90, "client", luat_client_task, NULL, 16);
	

	// 此task通过不断的进入和退出飞行模式，来模拟断网场景，每次断网重新联网后，分配的ipv6地址会发生改变；
	// 仅用来模拟测试"ipv6地址发生改变时，client task主动再去上报一次imei、iccid、ipv6 server地址给服务器"的功能；
	// 有需要可以自行打开
	// luat_rtos_task_handle flymode_task_handle;
	// luat_rtos_task_create(&flymode_task_handle, 2048, 20, "flymode", flymode_task, NULL, NULL);
	
}

INIT_TASK_EXPORT(ipv6_sleep_wakeup_init, "1");
