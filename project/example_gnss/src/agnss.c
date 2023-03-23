#include "luat_mobile.h"
#include "HTTPClient.h"

#include "common_api.h"
#include "sockets.h"
#include "dns.h"
#include "lwip/ip4_addr.h"
#include "netdb.h"
#include "luat_debug.h"
#include "luat_rtos.h"
#include "lbsLoc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "luat_fs.h"
#include "agnss.h"

#define LBSLOC_SERVER_UDP_IP "bs.openluat.com" // 基站定位网址
#define LBSLOC_SERVER_UDP_PORT 12411           // 端口
#define UART_ID 2
/// @brief 合宙IOT 项目productkey ，必须加上，否则定位失败
static char *productKey = "vQfJesoQdXDK8X1sXkYg0KTU42UBhdjh";
static luat_rtos_task_handle lbsloc_task_handle = NULL;

static uint8_t lbsloc_task_tatus = 0;

static bool ddddtoddmm(char *location, char *test)
{
    char *integer = NULL;
    char *fraction = NULL;
    char tmpstr[15] = {0};
    float tmp = 0;
    integer = strtok(location, ".");
    if (integer)
    {
        fraction = strtok(NULL, ".");
        sprintf(tmpstr, "0.%d", atoi(fraction));
        tmp = atof(tmpstr) * 60;
        tmp = atoi(integer) * 100 + tmp;
        memset(tmpstr, 0x00, 15);
        sprintf(tmpstr, "%f", tmp);
        memcpy(test, tmpstr, strlen(tmpstr) + 1);
        return true;
    }
    return false;
}

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

static void lbsloc_task(void *arg)
{
    lbsloc_task_tatus = 1;
    ip_addr_t remote_ip;
    struct sockaddr_in name;
    socklen_t sockaddr_t_size = sizeof(name);
    int ret;
    struct timeval to;
    int socket_id = -1;
    struct hostent dns_result;
    struct hostent *p_result;
    int h_errnop, read_len;
    struct am_location_service_rsp_data_t locationServiceResponse;

    uint8_t latitude[20] = {0};  // 经度
    uint8_t longitude[20] = {0}; // 维度
    uint16_t year = 0;           // 年
    uint8_t month = 0;           // 月
    uint8_t day = 0;             // 日
    uint8_t hour = 0;            // 小时
    uint8_t minute = 0;          // 分钟
    uint8_t second = 0;          // 秒
    uint8_t lbsLocReqBuf[176] = {0};
    memset(lbsLocReqBuf, 0, 176);
    uint8_t sendLen = 0;
    lbsLocReqBuf[sendLen++] = strlen(productKey);
    memcpy(&lbsLocReqBuf[sendLen], (UINT8 *)productKey, strlen(productKey));
    sendLen = sendLen + strlen(productKey);
    lbsLocReqBuf[sendLen++] = 0x28;
    CHAR imeiBuf[16];
    memset(imeiBuf, 0, sizeof(imeiBuf));
    luat_mobile_get_imei(0, imeiBuf, 16);
    uint8_t imeiBcdBuf[8] = {0};
    imeiToBcd((uint8_t *)imeiBuf, 15, imeiBcdBuf);
    memcpy(&lbsLocReqBuf[sendLen], imeiBcdBuf, 8);
    sendLen = sendLen + 8;
    CHAR muidBuf[64];
    memset(muidBuf, 0, sizeof(muidBuf));
    luat_mobile_get_muid(muidBuf, sizeof(muidBuf));
    lbsLocReqBuf[sendLen++] = strlen(muidBuf);
    memcpy(&lbsLocReqBuf[sendLen], (UINT8 *)muidBuf, strlen(muidBuf));
    sendLen = sendLen + strlen(muidBuf);
    luat_mobile_cell_info_t cell_info;
    memset(&cell_info, 0, sizeof(cell_info));
    luat_mobile_get_cell_info(&cell_info);
    lbsLocReqBuf[sendLen++] = 0x01;
    lbsLocReqBuf[sendLen++] = (cell_info.lte_service_info.tac >> 8) & 0xFF;
    lbsLocReqBuf[sendLen++] = cell_info.lte_service_info.tac & 0xFF;
    lbsLocReqBuf[sendLen++] = (cell_info.lte_service_info.mcc >> 8) & 0xFF;
    lbsLocReqBuf[sendLen++] = cell_info.lte_service_info.mcc & 0XFF;
    uint8_t mnc = cell_info.lte_service_info.mnc;
    if (mnc > 10)
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
    int16_t sRssi;
    uint8_t retRssi;
    sRssi = cell_info.lte_service_info.rssi;
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

    char hostname[128] = {0};
    ret = lwip_gethostbyname_r(LBSLOC_SERVER_UDP_IP, &dns_result, hostname, 128, &p_result, &h_errnop);
    if (!ret)
    {
        remote_ip = *((ip_addr_t *)dns_result.h_addr_list[0]);
    }
    else
    {
        luat_rtos_task_sleep(1000);
        LUAT_DEBUG_PRINT("dns fail");
        lbsloc_task_tatus = 0;
        luat_rtos_task_delete(lbsloc_task_handle);
        return;
    }

    socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct timeval timeout;
    timeout.tv_sec = AM_LOCATION_SERVICE_RCV_TIMEOUT;
    timeout.tv_usec = 0;
    setsockopt(socket_id, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = remote_ip.u_addr.ip4.addr;
    name.sin_port = htons(LBSLOC_SERVER_UDP_PORT);
    ret = sendto(socket_id, lbsLocReqBuf, sendLen, 0, (const struct sockaddr *)&name, sockaddr_t_size);
    if (ret == sendLen)
    {
        LUAT_DEBUG_PRINT("location_service_task send lbsLoc request success");
        ret = recv(socket_id, &locationServiceResponse, sizeof(struct am_location_service_rsp_data_t), 0);
        if (ret > 0)
        {
            LUAT_DEBUG_PRINT("location_service_task recv lbsLoc success %d", ret);
            if (sizeof(struct am_location_service_rsp_data_t) == ret)
            {
                if (location_service_parse_response(&locationServiceResponse, latitude, longitude, &year, &month, &day, &hour, &minute, &second) == TRUE)
                {
                    LUAT_DEBUG_PRINT("location_service_task: rcv response, process success");
                    LUAT_DEBUG_PRINT("latitude:%s,longitude:%s,year:%d,month:%d,day:%d,hour:%d,minute:%d,second:%d\r\n", latitude, longitude, year, month, day, hour, minute, second);
                    char data_buf[60] = {0};
                    snprintf(data_buf, 60, "$AIDTIME,%d,%d,%d,%d,%d,%d,000\r\n", year, month, day, hour, minute, second);
                    LUAT_DEBUG_PRINT("location_service_task: AIDTIME %s", data_buf);
                    luat_uart_write(UART_ID, data_buf, strlen(data_buf));
                    luat_rtos_task_sleep(200);
                    memset(data_buf, 0x00, 60);
                    char lat[20] = {0};
                    char lng[20] = {0};
                    ddddtoddmm(latitude, lat);
                    ddddtoddmm(longitude, lng);
                    snprintf(data_buf, 60, "$AIDPOS,%s,N,%s,E,1.0\r\n", lat, lng);
                    LUAT_DEBUG_PRINT("location_service_task: AIDPOS %s", data_buf);
                    luat_uart_write(UART_ID, data_buf, strlen(data_buf));
                }
                else
                {
                    LUAT_DEBUG_PRINT("location_service_task: rcv response, but process fail");
                }
            }
        }
        else
        {
            LUAT_DEBUG_PRINT("location_service_task recv lbsLoc fail %d", ret);
        }
    }
    else
    {
        LUAT_DEBUG_PRINT("location_service_task send lbsLoc request fail");
    }
    memset(latitude, 0, 20);
    memset(longitude, 0, 20);
    year = 0;
    month = 0;
    day = 0;
    hour = 0;
    minute = 0;
    second = 0;
    LUAT_DEBUG_PRINT("socket quit");
    close(socket_id);
    socket_id = -1;
    lbsloc_task_tatus = 0;
    luat_rtos_task_delete(lbsloc_task_handle);
}

void lbsloc_request(void)
{
    if (lbsloc_task_handle == NULL || lbsloc_task_tatus == 0)
        luat_rtos_task_create(&lbsloc_task_handle, 4 * 2048, 80, "lbsloc", lbsloc_task, NULL, NULL);
    else
        LUAT_DEBUG_PRINT("lbsloc task create fail");
}

#define HTTP_RECV_BUF_SIZE (1500)
#define HTTP_HEAD_BUF_SIZE (800)
#define EPH_HOST "http://download.openluat.com/9501-xingli/HXXT_GPS_BDS_AGNSS_DATA.dat"

static g_s_network_status = 0;
static HttpClientContext gHttpClient = {0};
static luat_rtos_task_handle http_task_handle;

void mobile_event_callback(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status)
{
    if (event == LUAT_MOBILE_EVENT_NETIF && status == LUAT_MOBILE_NETIF_LINK_ON)
    {
        LUAT_DEBUG_PRINT("network ready");
        g_s_network_status = 1;
    }
    else if (event == LUAT_MOBILE_EVENT_NETIF && status == LUAT_MOBILE_NETIF_LINK_OFF)
    {
        LUAT_DEBUG_PRINT("network no ready");
        g_s_network_status = 0;
    }
}

static INT32 httpGetData(CHAR *getUrl, CHAR *buf, UINT32 len)
{
    HTTPResult result = HTTP_INTERNAL;
    HttpClientData clientData = {0};
    UINT32 count = 0;
    FILE *fp1 = NULL;
    uint16_t headerLen = 0;

    LUAT_DEBUG_ASSERT(buf != NULL, 0, 0, 0);

    clientData.headerBuf = malloc(HTTP_HEAD_BUF_SIZE);
    clientData.headerBufLen = HTTP_HEAD_BUF_SIZE;
    clientData.respBuf = buf;
    clientData.respBufLen = len;

    result = httpSendRequest(&gHttpClient, getUrl, HTTP_GET, &clientData);
    LUAT_DEBUG_PRINT("send request result=%d", result);
    if (result != HTTP_OK)
        goto exit;
    do
    {
        LUAT_DEBUG_PRINT("recvResponse loop.");
        memset(clientData.headerBuf, 0, clientData.headerBufLen);
        memset(clientData.respBuf, 0, clientData.respBufLen);
        result = httpRecvResponse(&gHttpClient, &clientData);
        if (result == HTTP_OK || result == HTTP_MOREDATA)
        {
            headerLen = strlen(clientData.headerBuf);
            if (headerLen > 0)
            {
                fp1 = luat_fs_fopen(EPH_FILE_PATH, "w+");
                LUAT_DEBUG_PRINT("total content length=%d", clientData.recvContentLength);
            }

            if (clientData.blockContentLen > 0)
            {
                LUAT_DEBUG_PRINT("response content:{%s}", (uint8_t *)clientData.respBuf);
                luat_fs_fwrite((uint8_t *)clientData.respBuf, clientData.blockContentLen, 1, fp1);
            }
            count += clientData.blockContentLen;
            LUAT_DEBUG_PRINT("has recv=%d", count);
        }
    } while (result == HTTP_MOREDATA || result == HTTP_CONN);
    luat_fs_fclose(fp1);
    LUAT_DEBUG_PRINT("result=%d", result);
    if (gHttpClient.httpResponseCode < 200 || gHttpClient.httpResponseCode > 404)
    {
        LUAT_DEBUG_PRINT("invalid http response code=%d", gHttpClient.httpResponseCode);
    }
    else if (count == 0 || count != clientData.recvContentLength)
    {
        LUAT_DEBUG_PRINT("data not receive complete");
    }
    else
    {
        LUAT_DEBUG_PRINT("receive success");
    }
exit:
    free(clientData.headerBuf);
    return result;
}

static bool update_eph()
{
    char *recvBuf = malloc(HTTP_RECV_BUF_SIZE);
    HTTPResult result = HTTP_INTERNAL;
    gHttpClient.timeout_s = 2;
    gHttpClient.timeout_r = 20;
    FILE *fp = NULL;
    result = httpConnect(&gHttpClient, EPH_HOST);
    if (result == HTTP_OK)
    {
        result = httpGetData(EPH_HOST, recvBuf, HTTP_RECV_BUF_SIZE);
        httpClose(&gHttpClient);
        if (result == HTTP_OK)
        {
            time_t nowtime;
            time(&nowtime);
            LUAT_DEBUG_PRINT("http client get data success %d", nowtime);
        }
        else
        {
            luat_fs_remove(EPH_FILE_PATH);
        }
    }
    else
    {
        LUAT_DEBUG_PRINT("http client connect error");
    }
    memset(recvBuf, 0x00, HTTP_RECV_BUF_SIZE);
    free(recvBuf);

    if (result != HTTP_OK)
    {
        return false;
    }
    return true;
}


static bool check_eph_time()
{
    if(luat_fs_fexist(EPH_TIME_FILE) == 0)
        return false;
    FILE *fp = luat_fs_fopen(EPH_TIME_FILE, "r");
    time_t history_time;
    luat_fs_fread((void *)&history_time, sizeof(time_t), 1, fp);
    luat_fs_fclose(fp);

    time_t now_time;
    time(&now_time);
    LUAT_DEBUG_PRINT("this is interval time %d", now_time - history_time);
    if((now_time - history_time) > 4 * 60 * 60)
    {
        return false;
    }
    return true;
}

static void ephemeris_get_task(void *param)
{
    while (1)
    {
        while (g_s_network_status != 1)
        {
            LUAT_DEBUG_PRINT("http wait network ready");
            luat_rtos_task_sleep(1000);
        }
        if (luat_fs_fexist(EPH_FILE_PATH) != 0 && check_eph_time() == true)
        {
            size_t size = luat_fs_fsize(EPH_FILE_PATH);
            uint8_t *data = NULL;
            data = (uint8_t *)luat_heap_malloc(size);
            FILE *fp1 = luat_fs_fopen(EPH_FILE_PATH, "r");
            luat_fs_fread(data, size, 1, fp1);
            luat_fs_fclose(fp1);
            for (size_t i = 0; i < size; i = i + 512)
            {
                if (i + 512 < size)
                    luat_uart_write(UART_ID, (uint8_t *)&data[i], 512);
                else
                    luat_uart_write(UART_ID, (uint8_t *)&data[i], size - i);
                luat_rtos_task_sleep(100);
            }
            lbsloc_request();
            luat_rtos_task_sleep(4 * 60 * 60 * 1000);
        }
        else
        {
            bool result = update_eph();
            if (result)
            {
                FILE *fp = luat_fs_fopen(EPH_TIME_FILE, "w+");
                time_t now_time;
                time(&now_time);
                luat_fs_fwrite((void *)&now_time, sizeof(time_t), 1, fp);
                luat_fs_fclose(fp);

                size_t size = luat_fs_fsize(EPH_FILE_PATH);
                uint8_t *data = NULL;
                data = (uint8_t *)luat_heap_malloc(size);
                FILE *fp1 = luat_fs_fopen(EPH_FILE_PATH, "r");
                luat_fs_fread(data, size, 1, fp1);
                luat_fs_fclose(fp1);
                for (size_t i = 0; i < size; i = i + 512)
                {
                    if (i + 512 < size)
                        luat_uart_write(UART_ID, (uint8_t *)&data[i], 512);
                    else
                        luat_uart_write(UART_ID, (uint8_t *)&data[i], size - i);
                    luat_rtos_task_sleep(100);
                }
                lbsloc_request();
                luat_rtos_task_sleep(4 * 60 * 60 * 1000);
            }
            else
            {
                luat_rtos_task_sleep(30000);
            }
        }
        luat_rtos_task_delete(http_task_handle);
    }
}
void task_ephemeris(void)
{
    luat_rtos_task_create(&http_task_handle, 10 * 1024, 20, "http", ephemeris_get_task, NULL, NULL);
}

void network_init(void)
{
    luat_mobile_event_register_handler(mobile_event_callback);
}