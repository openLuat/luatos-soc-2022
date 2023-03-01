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
#include "luat_mobile.h"
#include "luat_rtos.h"
#include "luat_debug.h"
#include "luat_fota.h"
#include "reset.h"
#include "HTTPClient.h"

/*
FOTA应用开发可以参考：https://doc.openluat.com/wiki/37?wiki_page_id=4727
*/

#define PROJECT_VERSION  "1.0.0"                  //使用合宙iot升级的话此字段必须存在，并且强制固定格式为x.x.x, x可以为任意的数字
#define PROJECT_KEY "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  //修改为自己iot上面的PRODUCT_KEY，这里是一个错误的，使用合宙iot升级的话此字段必须存在
#define PROJECT_NAME "TEST_FOTA"                  //使用合宙iot升级的话此字段必须存在，可以任意修改，但和升级包的必须一致


/*
    PROJECT_VERSION:用于区分不同软件版本，同时也用于区分固件差分基础版本
    假设：
        现在有两批模块在运行不同的基础版本，一个版本号为1.0.0，另一个版本号为2.0.0
        现在这两个版本都需要升级到3.0.0，则需要做两个差分包，一个是从1.0.0升级到3.0.0，另一个是从2.0.0升级到3.0.0
        但是因为合宙IOT是通过firmware来区分不同版本固件，只要请求时firmware相同，版本号比设备运行的要高，就会下发升级文件
        所以需要对firmware做一点小小的操作，加一个字段来区分基础版本不同的差分包
        从1.0.0升级到3.0.0生成的firmware字段为1.0.0_TEST_FOTA_CSDK_EC618
        从2.0.0升级到3.0.0生成的firmware字段为2.0.0_TEST_FOTA_CSDK_EC618

        这样操作后，当两个升级包放上去，1.0.0就算被放进了2.0.0_TEST_FOTA_CSDK_EC618的升级列表里，也会因为自身上报的字段和所在升级列表的固件名称不一致而被服务器拒绝升级

    重要说明！！！！！！！！！！！！！！
    设备如果升级的话，设备运行的版本必须和差分文件时所选的旧版固件一致
    举例，用1.0.0和3.0.0差分得软件，必须也只能给运行1.0.0软件的设备用
*/

char g_test_server_name[200] = {0};
#define TEST_HOST "http://iot.openluat.com"
#define HTTP_RECV_BUF_SIZE      (1501)
#define HTTP_HEAD_BUF_SIZE      (800)



static HttpClientContext        gHttpClient = {0};
luat_fota_img_proc_ctx_ptr test_luat_fota_handle = NULL;


const char *soc_get_sdk_type(void) //用户可以重新实现这个函数，自定义版本名称
{
	return "CSDK";
}

/**
  \fn      INT32 httpGetData(CHAR *getUrl, CHAR *buf, UINT32 len)
  \brief
  \return
*/
static INT32 httpGetData(CHAR *getUrl, CHAR *buf, UINT32 len)
{
    HTTPResult result = HTTP_INTERNAL;
    HttpClientData    clientData = {0};
    UINT32 count = 0;
    uint16_t headerLen = 0;
    int result1 = 0;

    LUAT_DEBUG_ASSERT(buf != NULL,0,0,0);

    clientData.headerBuf = malloc(HTTP_HEAD_BUF_SIZE);
    clientData.headerBufLen = HTTP_HEAD_BUF_SIZE;
    clientData.respBuf = buf;
    clientData.respBufLen = len;
    
    result = httpSendRequest(&gHttpClient, getUrl, HTTP_GET, &clientData);
    LUAT_DEBUG_PRINT("send request result=%d", result);
    if (result != HTTP_OK)
        goto exit;
    do {
    	LUAT_DEBUG_PRINT("recvResponse loop.");
        memset(clientData.headerBuf, 0, clientData.headerBufLen);
        memset(clientData.respBuf, 0, clientData.respBufLen);
        result = httpRecvResponse(&gHttpClient, &clientData);
        if(result == HTTP_OK || result == HTTP_MOREDATA){
            headerLen = strlen(clientData.headerBuf);
            if(headerLen > 0)
            {
            	LUAT_DEBUG_PRINT("total content length=%d", clientData.recvContentLength);
                test_luat_fota_handle = luat_fota_init();
            }

            if(clientData.blockContentLen > 0)
            {
            	LUAT_DEBUG_PRINT("response content:{%s}", (uint8_t*)clientData.respBuf);
                result1 = luat_fota_write(test_luat_fota_handle,clientData.respBuf,clientData.blockContentLen);
                if (result1==0)
                {
                    LUAT_DEBUG_PRINT("fota update success");
                }
                else{
                    LUAT_DEBUG_PRINT("fota update error");
                }
            }
            count += clientData.blockContentLen;

            
            LUAT_DEBUG_PRINT("has recv=%d", count);
        }
    } while (result == HTTP_MOREDATA || result == HTTP_CONN);

    LUAT_DEBUG_PRINT("result=%d", result);
    if (gHttpClient.httpResponseCode < 200 || gHttpClient.httpResponseCode > 404)
    {
    	LUAT_DEBUG_PRINT("invalid http response code=%d",gHttpClient.httpResponseCode);
    } else if (count == 0 || count != clientData.recvContentLength) {
    	LUAT_DEBUG_PRINT("data not receive complete");
    } else {
    	LUAT_DEBUG_PRINT("receive success");
    }
exit:
    free(clientData.headerBuf);
    return result;
}

luat_rtos_semaphore_t net_semaphore_handle;
luat_rtos_task_handle https_task_handle;

void mobile_event_callback(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status){
    if (event == LUAT_MOBILE_EVENT_NETIF && status == LUAT_MOBILE_NETIF_LINK_ON){
        LUAT_DEBUG_PRINT("network ready");
        luat_rtos_semaphore_release(net_semaphore_handle);
    }
}

static void task_test_fota(void *param)
{
    luat_rtos_semaphore_create(&net_semaphore_handle, 1);

	char *recvBuf = malloc(HTTP_RECV_BUF_SIZE);
	HTTPResult result = HTTP_INTERNAL;

    luat_mobile_event_register_handler(mobile_event_callback);

    luat_rtos_task_sleep(3000);
    LUAT_DEBUG_PRINT("version = %s", PROJECT_VERSION);

    gHttpClient.timeout_s = 2;
    gHttpClient.timeout_r = 20;
    gHttpClient.seclevel = 1;
    gHttpClient.ciphersuite[0] = 0xFFFF;
    gHttpClient.ignore = 1;
    char imei[16] = {0};
    luat_mobile_get_imei(0, imei, 15);
    snprintf(g_test_server_name, 200, "%s/api/site/firmware_upgrade?project_key=%s&imei=%s&device_key=&firmware_name=%s_%s_%s_%s&version=%s", TEST_HOST, PROJECT_KEY, imei, PROJECT_VERSION, PROJECT_NAME, soc_get_sdk_type(), "EC618", PROJECT_VERSION);
    LUAT_DEBUG_PRINT("test print url %s", g_test_server_name);
    luat_rtos_semaphore_take(net_semaphore_handle, LUAT_WAIT_FOREVER);

    result = httpConnect(&gHttpClient, TEST_HOST);
    if (result == HTTP_OK)
    {
        httpGetData(g_test_server_name, recvBuf, HTTP_RECV_BUF_SIZE);
        httpClose(&gHttpClient);
        LUAT_DEBUG_PRINT("verify start");
        int verify = luat_fota_done(test_luat_fota_handle);
        if(verify != 0)
        {
            LUAT_DEBUG_PRINT("image_verify error");
            goto exit;
        }
	    LUAT_DEBUG_PRINT("image_verify ok");
        ResetStartPorReset(RESET_REASON_FOTA);
    }
    else
    {
        LUAT_DEBUG_PRINT("http client connect error");
    }
    exit:
    free(recvBuf);
    luat_rtos_task_delete(https_task_handle);
}

static void task_demo_fota(void)
{
	luat_rtos_task_create(&https_task_handle, 32*1024, 20, "https", task_test_fota, NULL, NULL);
}
//启动task_demoF_init，启动位置任务2级
INIT_TASK_EXPORT(task_demo_fota, "1");

