#include "common_api.h"
#include "sockets.h"
#include "dns.h"
#include "lwip/ip4_addr.h"
#include "netdb.h"
#include "luat_debug.h"
#include "luat_rtos.h"
#include "luat_mobile.h"
#include "string.h"
#include "example_lbsLoc.h"
#include "net_led_Task.h"
#define DEMO_SERVER_UDP_IP "bs.openluat.com" //基站定位网址
#define DEMO_SERVER_UDP_PORT 12411 //端口
extern uint8_t link_UP; //网络状态指示

/// @brief 合宙IOT 项目productkey ，必须加上，否则定位失败
static uint8_t *productKey="vQfJesoQdXDK8X1sXkYg0KTU42UBhdjh" ;


/// @brief 把string 转换为BCD 编码
/// @param arr 字符串输入
/// @param len 长度
/// @param outPut 输出
/// @return 
static uint8_t imeiToBcd(uint8_t *arr, uint8_t len, uint8_t *outPut)
{
    if (len % 2 != 0)
    {
        arr[len] = 0x0f;
    }

    uint8_t tmp = 0;

    for (uint8_t j = 0; j < len; j = j + 2)
    {
        outPut[tmp] = (arr[j] & 0x0f) << 4 | (arr[j + 1] & 0x0f);
        tmp++;
    }
    for (uint8_t i = 0; i < 8; i++)
    {
        outPut[i] = (outPut[i] % 0x10) * 0x10 + (outPut[i] - (outPut[i] % 0x10)) / 0x10;
    }
    return 0;
}

/// @brief BCD ->> str
/// @param pOutBuffer 
/// @param pInBuffer 
/// @param nInLen 长度
/// @return 
static uint32_t location_service_bcd_to_str(uint8_t *pOutBuffer, uint8_t *pInBuffer, uint32_t nInLen)
{
    uint32_t len = 0;
    uint8_t ch;
    uint8_t *p = pOutBuffer;
    uint32_t i = 0;

    if (pOutBuffer == NULL || pInBuffer == NULL || nInLen == 0)
    {
        return 0;
    }

    for (i = 0; i < nInLen; i++)
    {
        ch = pInBuffer[i] & 0x0F;
        if (ch == 0x0F)
        {
            break;
        }
        *pOutBuffer++ = ch + '0';

        ch = (pInBuffer[i] >> 4) & 0x0F;
        if (ch == 0x0F)
        {
            break;
        }
        *pOutBuffer++ = ch + '0';
    }

    len = pOutBuffer - p;

    return len;
}


static bool location_service_parse_response(struct am_location_service_rsp_data_t *response, uint8_t *latitude, uint8_t *longitude,
                                            uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    uint8_t loc[20] = {0};
    uint32_t len = 0;

    if (response == NULL || latitude == NULL || longitude == NULL || year == NULL || month == NULL || day == NULL || hour == NULL || minute == NULL || second == NULL)
    {
        LUAT_DEBUG_PRINT("location_service_parse_response: invalid parameter");
        return FALSE;
    }

    if (!(response->result == 0 || response->result == 0xFF))
    {
        LUAT_DEBUG_PRINT("location_service_parse_response: result fail %d", response->result);
        return FALSE;
    }

    // latitude
    len = location_service_bcd_to_str(loc, response->latitude, AM_LOCATION_SERVICE_LOCATION_BCD_LEN);
    if (len <= 0)
    {
        LUAT_DEBUG_PRINT("location_service_parse_response: latitude fail");
        return FALSE;
    }
    strncat((char *)latitude, (char *)loc, 3);
    strncat((char *)latitude, ".", 2);
    strncat((char *)latitude, (char *)(loc + 3), len - 3);
    len = location_service_bcd_to_str(loc, response->longitude, AM_LOCATION_SERVICE_LOCATION_BCD_LEN);
    if (len <= 0)
    {
        LUAT_DEBUG_PRINT("location_service_parse_response: longitude fail");
        return FALSE;
    }
    strncat((char *)longitude, (char *)loc, 3);
    strncat((char *)longitude, (char *)".", 2);
    strncat((char *)longitude, (char *)(loc + 3), len - 3);
    *year = response->year + 2000;
    *month = response->month;
    *day = response->day;
    *hour = response->hour;
    *minute = response->minute;
    *second = response->second;

    return TRUE;
}

static void demo_udp_task(void *arg)
{
	ip_addr_t remote_ip;
	char *txbuf;
    struct sockaddr_in name;
    socklen_t sockaddr_t_size = sizeof(name);
    int ret;
    struct timeval to;
    int socket_id = -1;
    struct hostent dns_result;
    struct hostent *p_result;
	int h_errnop, read_len;
    struct am_location_service_rsp_data_t locationServiceResponse;

	uint8_t latitude[20] = {0};//经度
    uint8_t longitude[20] = {0};//维度
    uint16_t year = 0;//年
    uint8_t month = 0;//月
    uint8_t day = 0;//日
    uint8_t hour = 0;//小时
    uint8_t minute = 0;//分钟
    uint8_t second = 0;//秒
    while (!link_UP)
	{
		luat_rtos_task_sleep(1000);
		LUAT_DEBUG_PRINT("等待网络注册");
	}
	uint8_t lbsLocReqBuf[176] = {0};
    memset(lbsLocReqBuf, 0, 176);
	uint8_t sendLen=0;
	lbsLocReqBuf[sendLen++]=strlen(productKey);
	memcpy(&lbsLocReqBuf[sendLen], (UINT8 *)productKey, strlen(productKey));
	sendLen = sendLen + strlen(productKey);
	// lbsLocReqBuf[sendLen++] = 0x01;
    // lbsLocReqBuf[sendLen++] = '0';
	lbsLocReqBuf[sendLen++]=0x28;
	CHAR imeiBuf[16];
    memset(imeiBuf, 0, sizeof(imeiBuf));
	luat_mobile_get_imei(0,imeiBuf,16);
	uint8_t imeiBcdBuf[8]={0};
	imeiToBcd((uint8_t *)imeiBuf, 15, imeiBcdBuf);
	memcpy(&lbsLocReqBuf[sendLen], imeiBcdBuf, 8);
	sendLen = sendLen + 8;
	CHAR muidBuf[64];
    memset(muidBuf, 0, sizeof(muidBuf));
    luat_mobile_get_muid(muidBuf,sizeof(muidBuf));
    lbsLocReqBuf[sendLen++] = strlen(muidBuf);
    memcpy(&lbsLocReqBuf[sendLen], (UINT8 *)muidBuf, strlen(muidBuf));
    sendLen = sendLen + strlen(muidBuf);
    luat_mobile_cell_info_t  cell_info;
	memset(&cell_info,0,sizeof(cell_info));
	luat_mobile_get_cell_info(&cell_info);
	lbsLocReqBuf[sendLen++] = 0x01;
	lbsLocReqBuf[sendLen++] = (cell_info.lte_service_info.tac>> 8) & 0xFF;
    lbsLocReqBuf[sendLen++] = cell_info.lte_service_info.tac & 0xFF;
    lbsLocReqBuf[sendLen++] = (cell_info.lte_service_info.mcc >> 8) & 0xFF;
    lbsLocReqBuf[sendLen++] = cell_info.lte_service_info.mcc & 0XFF;
	uint8_t mnc=cell_info.lte_service_info.mnc;
    if(mnc > 10)
    {
        CHAR buf[3] = {0};
        snprintf(buf, 3, "%02x", mnc);
        int ret1 = atoi(buf);
        lbsLocReqBuf[sendLen++] = ret1;
    }
    else
    {
        lbsLocReqBuf[sendLen++] = mnc;
    }
	uint8_t sRssi,retRssi;
	sRssi=cell_info.lte_service_info.rssi;
	if (sRssi <= -113)
	{
		retRssi = 0;
	}
	else if (sRssi < -52)
	{
		retRssi = (sRssi + 113) >> 1;
	}
	else
	{
		retRssi = 31;
	}
	lbsLocReqBuf[sendLen++] = retRssi;
    lbsLocReqBuf[sendLen++] = (cell_info.lte_service_info.cid >> 24) & 0xFF;
    lbsLocReqBuf[sendLen++] = (cell_info.lte_service_info.cid >> 16) & 0xFF;
    lbsLocReqBuf[sendLen++] = (cell_info.lte_service_info.cid >> 8) & 0xFF;
    lbsLocReqBuf[sendLen++] = cell_info.lte_service_info.cid & 0xFF;

    txbuf = malloc(128);
	ret = lwip_gethostbyname_r(DEMO_SERVER_UDP_IP, &dns_result, txbuf, 128, &p_result, &h_errnop);
	if (!ret)
	{
		remote_ip = *((ip_addr_t *)dns_result.h_addr_list[0]);
		free(txbuf);
	}
	else
	{
		free(txbuf);
		luat_rtos_task_sleep(1000);
		LUAT_DEBUG_PRINT("dns fail");
		
	}

	socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	fcntl(socket_id, F_SETFL, O_NONBLOCK);
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = IPADDR_ANY;
	name.sin_port = 0;
	bind(socket_id, (const struct sockaddr *)&name, sockaddr_t_size);
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = remote_ip.u_addr.ip4.addr;
	name.sin_port = htons(DEMO_SERVER_UDP_PORT);
	while(1)
    {
		ret = sendto(socket_id, lbsLocReqBuf, sendLen, 0, (const struct sockaddr *)&name, sockaddr_t_size);
		luat_rtos_task_sleep(5000);
		if (ret == sendLen)
		{
			LUAT_DEBUG_PRINT("lbsLocSendRequest send lbsLoc request success");
			ret = recv(socket_id, &locationServiceResponse, sizeof(struct am_location_service_rsp_data_t), 0);
			if (sizeof(struct am_location_service_rsp_data_t) == ret)
			{
				if (location_service_parse_response(&locationServiceResponse, latitude, longitude,&year, &month, &day, &hour, &minute, &second) == TRUE)
				{
					LUAT_DEBUG_PRINT("latitude:%s,longitude:%s,year:%d,month:%d,day:%d,hour:%d,minute:%d,second:%d\r\n",latitude,longitude,year,month,day,hour,minute,second);
				}
				else
				{
					
					LUAT_DEBUG_PRINT("location_service_task: rcv response, but process fail");
				}
			}
		}
		else
		{
			LUAT_DEBUG_PRINT("lbsLocSendRequest send lbsLoc request fail");
		}
		memset(latitude,0,20);
		memset(longitude,0,20);
		year = 0;
		month = 0;
		day = 0;
		hour = 0;
		minute = 0;
		second = 0;
	}
	LUAT_DEBUG_PRINT("socket quit");
	close(socket_id);
	socket_id = -1;
	luat_rtos_task_sleep(5000);
}




static void demo_udp_init(void)
{
	luat_rtos_task_handle tcp_task_handle;
	luat_rtos_task_create(&tcp_task_handle, 4*2048, 80, "udp", demo_udp_task, NULL, NULL);
}

INIT_HW_EXPORT(Task_netinfo_call, "0");
INIT_TASK_EXPORT(demo_udp_init, "1");
INIT_TASK_EXPORT(NET_LED_Task, "2");

