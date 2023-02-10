#include "web_audio.h"
#include "audio_Task.h"
#include "net_led_Task.h"

#include "luat_network_adapter.h"
#include "networkmgr.h"

#include "libemqtt.h"
#include "luat_mqtt.h"
#include "luat_rtos.h"

luat_rtos_queue_t MQTT_payload_queue;

extern QueueHandle_t audioQueueHandle;
extern uint8_t link_UP;
char mqtt_subTopic[40];
static char subTopic[] = "test20220929/";

static HttpClientContext AirM2mhttpClient;

#define HTTP_RECV_BUF_SIZE (11520)
#define HTTP_HEAD_BUF_SIZE (800)

/************************云端音频MQTT 负载消息类型***************************************/
// 0---4字节大端文字长度--文字的gbk编码--1--4字节大端音频url长度--url的gbk编码
// 0表示后面是普通文字。1表示后面是音频。两种任意组合。长度是后面编码的长度
/************************云端音频MQTT 负载消息类型***************************************/
/************************HTTP         音频下载处理**************************************/
static INT32 httpGetData(CHAR *getUrl, CHAR *buf, UINT32 len)
{
    HTTPResult result = HTTP_INTERNAL;
    HttpClientData clientData = {0};
    UINT32 count = 0;
    uint16_t headerLen = 0;

    LUAT_DEBUG_ASSERT(buf != NULL, 0, 0, 0);

    clientData.headerBuf = malloc(HTTP_HEAD_BUF_SIZE);
    clientData.headerBufLen = HTTP_HEAD_BUF_SIZE;
    clientData.respBuf = buf;
    clientData.respBufLen = len;
    result = httpSendRequest(&AirM2mhttpClient, getUrl, HTTP_GET, &clientData);
    LUAT_DEBUG_PRINT("send request result=%d", result);
    if (result != HTTP_OK)
        goto exit;
    do
    {
        LUAT_DEBUG_PRINT("recvResponse loop.");
        memset(clientData.headerBuf, 0, clientData.headerBufLen);
        memset(clientData.respBuf, 0, clientData.respBufLen);
        result = httpRecvResponse(&AirM2mhttpClient, &clientData);
        if (result == HTTP_OK || result == HTTP_MOREDATA)
        {
            headerLen = strlen(clientData.headerBuf);
            if (headerLen > 0)
            {
                LUAT_DEBUG_PRINT("total content length=%d", clientData.recvContentLength);
            }

            if (clientData.blockContentLen > 0)
            {
                LUAT_DEBUG_PRINT("response content:{%s}", (uint8_t *)clientData.respBuf);
            }
            count += clientData.blockContentLen;
            LUAT_DEBUG_PRINT("has recv=%d", count);
        }
    } while (result == HTTP_MOREDATA || result == HTTP_CONN);

    LUAT_DEBUG_PRINT("result=%d", result);
    if (AirM2mhttpClient.httpResponseCode < 200 || AirM2mhttpClient.httpResponseCode > 404)
    {
        LUAT_DEBUG_PRINT("invalid http response code=%d", AirM2mhttpClient.httpResponseCode);
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

static void task_test_https(CHAR *getUrl, uint32_t *buf, UINT32 len)
{
    HTTPResult result = HTTP_INTERNAL;
    result = httpConnect(&AirM2mhttpClient, getUrl);
    if (result == HTTP_OK)
    {
        httpGetData(getUrl, buf, len);
        httpClose(&AirM2mhttpClient);
    }
    else
    {
        LUAT_DEBUG_PRINT("http client connect error");
    }
}

void messageArrived(uint8_t payloadlen, uint8_t *data) // 对MQTT的负载消息进行处理，解析数据
{
    int payload_array[60];
    int payload_array_len = 0;
    luat_audio_play_info_t info[1];
    memset(info, 0, sizeof(info));
    static uint32_t *tmpbuff[HTTP_RECV_BUF_SIZE] = {0};
    char *p = (char *)data;
    uint8_t payload_len = payloadlen;
    for (uint8_t i = 0; i < payload_len; i++)
    {
        if ((p[i] == 0) || (p[i] == 1))
        {
            if ((p[i + 1] == 0) && (p[i + 2] == 0) && (p[i + 3] == 0))
            {
                payload_array[payload_array_len] = p[i];
                payload_array_len = payload_array_len + 1;
                payload_array[payload_array_len] = p[i + 4];
                payload_array_len = payload_array_len + 1;
                payload_array[payload_array_len] = &p[i + 4];
                payload_array_len = payload_array_len + 1;
            }
        }
        LUAT_DEBUG_PRINT("ceshi%d", p[i]);
    }
    LUAT_DEBUG_PRINT("payload_array_len%d", payload_array_len);
    for (int i = 0; i < payload_array_len; i = i + 3)
    {
        if (payload_array[i] == 0)
        {
            CHAR *TTS = malloc(payload_array[i + 1] * sizeof(char));
            memcpy(TTS, payload_array[i + 2] + 1, payload_array[i + 1]);
            int ret = luat_audio_play_tts_text(0, TTS, payload_array[i + 1]);
            if (0 == ret)
            {
                free(TTS);
            }
        }
        else if (payload_array[i]==1)
        {
            CHAR *URL = malloc(payload_array[i + 1] * sizeof(int));
            memset(URL, '\0', payload_array[i + 1] * sizeof(int));
            memcpy(URL, payload_array[i + 2] + 1, payload_array[i + 1]);
            task_test_https(URL, tmpbuff, HTTP_RECV_BUF_SIZE);
            LUAT_DEBUG_PRINT("name%c",URL[payload_array[i + 1]-1]);
            if (URL[payload_array[i + 1]-1]=='3')
            {
                FILE *fp1=luat_fs_fopen("test1.mp3","wb+");
                uint32_t status=luat_fs_fwrite((uint32_t*)tmpbuff,sizeof(tmpbuff),1,fp1);
                if (0==status)
                {
                   while (1);
                }
                luat_fs_fclose(fp1);
                info[0].path="test1.mp3";
                luat_audio_play_multi_files(0,info,1);
            }
            else if (URL[payload_array[i + 1]-1]=='r')
            {
                FILE *fp2=luat_fs_fopen("test1.amr","wb+");
                uint32_t status=luat_fs_fwrite((uint32_t*)tmpbuff,sizeof(tmpbuff),1,fp2);
                if (0==status)
                {
                   while (1);
                }
                luat_fs_fclose(fp2);
                info[0].path="test1.amr";
                luat_audio_play_multi_files(0,info,1);
            }
            else if (URL[payload_array[i + 1]-1]=='v')
            {
                FILE *fp3=luat_fs_fopen("test1.wav","wb+");
                uint32_t status=luat_fs_fwrite((uint32_t*)tmpbuff,sizeof(tmpbuff),1,fp3);
                if (0==status)
                {
                   while (1);
                }
                luat_fs_fclose(fp3);
                info[0].path="test1.wav";
                luat_audio_play_multi_files(0,info,1);
            }   
        }
        luat_rtos_task_sleep(1500);
        memset(tmpbuff, 0, HTTP_RECV_BUF_SIZE);
        LUAT_DEBUG_PRINT("payload_array%d", payload_array[i]);
    }
   
   
}

void mqtt_payload_task(void *param)
{
    payloaddata mqtt_queue_recv = {0};
    while (1)
    {
        if (0 == luat_rtos_queue_recv(MQTT_payload_queue, &mqtt_queue_recv, NULL, portMAX_DELAY))
        {
            LUAT_DEBUG_PRINT("pub_msg: %d", mqtt_queue_recv.payload_len);
            messageArrived(mqtt_queue_recv.payload_len, mqtt_queue_recv.payload_data);
        }
    }
}

void Mqtt_payload_task_Init(void) // 单独搞个任务负责下载，数据通过消息队列传输
{
    int ret = -1;
    luat_rtos_task_handle Mqtt_payload_handle;
    ret = luat_rtos_queue_create(&MQTT_payload_queue, 100, sizeof(payloaddata));
    if (0 == ret)
    {
        LUAT_DEBUG_PRINT("mqtt_payload_queue create sucessed\r\n");
    }
    luat_rtos_task_create(&Mqtt_payload_handle, 6 * 1024, 50, "Mqtt_payload_task", mqtt_payload_task, NULL, NULL);
}
/************************HTTP       音频下载处理**************************************/
static void luat_mqtt_cb(luat_mqtt_ctrl_t *luat_mqtt_ctrl, uint16_t event)
{
    switch (event)
    {
    case MQTT_MSG_CONNACK:
    {
        LUAT_DEBUG_PRINT("mqtt_subscribe");
        mqtt_subscribe(&(luat_mqtt_ctrl->broker), mqtt_subTopic, NULL, 0);
        break;
    }
    case MQTT_MSG_PUBLISH:
    {
        const uint8_t *ptr;
        int ret = -1;
        uint8_t payloadlen = mqtt_parse_pub_msg_ptr(luat_mqtt_ctrl->mqtt_packet_buffer, &ptr);
        LUAT_DEBUG_PRINT("pub_msg: %d%s", payloadlen, ptr);
        payloaddata payload_send = {0};
        payload_send.payload_len = payloadlen;
        payload_send.payload_data = ptr;
        ret = luat_rtos_queue_send(MQTT_payload_queue, &payload_send, NULL, 0);
        LUAT_DEBUG_PRINT("laut_rtos_send_sucessed%d", ret);
        if (0 == ret)
        {
            LUAT_DEBUG_PRINT("laut_rtos_send_sucessed");
        }

        break;
    }
    case MQTT_MSG_PUBACK:
    case MQTT_MSG_PUBCOMP:
    {
        LUAT_DEBUG_PRINT("msg_id: %d", mqtt_parse_msg_id(luat_mqtt_ctrl->mqtt_packet_buffer));
        break;
    }
    default:
        break;
    }
    return;
}

static void mqtt_demo(void)
{
    CHAR IMEI[20] = {0};
    if (luat_mobile_get_imei(0, IMEI, sizeof(IMEI)))
    {
        sprintf(mqtt_subTopic, "%s%s", subTopic, IMEI);
        LUAT_DEBUG_PRINT("imei%s", mqtt_subTopic);
    }
    int rc = -1;
    luat_mqtt_ctrl_t *luat_mqtt_ctrl = (luat_mqtt_ctrl_t *)luat_heap_malloc(sizeof(luat_mqtt_ctrl_t));
    rc = luat_mqtt_init(luat_mqtt_ctrl, NW_ADAPTER_INDEX_LWIP_GPRS);
    if (rc)
    {
        LUAT_DEBUG_PRINT("mqtt init FAID ret %d", rc);
        return 0;
    }
    luat_mqtt_ctrl->ip_addr.type = 0xff;
    luat_mqtt_connopts_t opts = {0};
    opts.host = HOST;
    opts.port = PORT;
    opts.is_tls = 0;
    luat_mqtt_set_connopts(luat_mqtt_ctrl, &opts);

    mqtt_init(&(luat_mqtt_ctrl->broker), IMEI);

    mqtt_init_auth(&(luat_mqtt_ctrl->broker), User, Password);

    // luat_mqtt_ctrl->netc->is_debug = 1;// debug
    luat_mqtt_ctrl->broker.clean_session = 1;
    luat_mqtt_ctrl->keepalive = 240;

    luat_mqtt_ctrl->reconnect = 1;
    luat_mqtt_ctrl->reconnect_time = 3000;

    luat_mqtt_set_cb(luat_mqtt_ctrl, luat_mqtt_cb);
    while (1)
    {
        while (!link_UP)
        {
            luat_rtos_task_sleep(1000);
        }
        rc = luat_mqtt_connect(luat_mqtt_ctrl);
        if (0 == rc)
        {
            audioQueueData MQTT_link = {0};
            MQTT_link.playType = TTS_PLAY;
            MQTT_link.priority = MONEY_PLAY;
            char str[] = "服务器连接成功";
            MQTT_link.message.tts.data = malloc(sizeof(str));
            memcpy(MQTT_link.message.tts.data, str, sizeof(str));
            MQTT_link.message.tts.len = sizeof(str);
            if (pdTRUE != xQueueSend(audioQueueHandle, &MQTT_link, 0))
            {
                LUAT_DEBUG_PRINT("start send audio fail");
            }
        }
        else
        {
            audioQueueData MQTT_link = {0};
            MQTT_link.playType = TTS_PLAY;
            MQTT_link.priority = MONEY_PLAY;
            char str[] = "服务器连接成功失败";
            MQTT_link.message.tts.data = malloc(sizeof(str));
            memcpy(MQTT_link.message.tts.data, str, sizeof(str));
            MQTT_link.message.tts.len = sizeof(str);
            if (pdTRUE != xQueueSend(audioQueueHandle, &MQTT_link, 0))
            {
                LUAT_DEBUG_PRINT("start send audio fail");
            }
        }
        while (1)
        {
            luat_rtos_task_sleep(1000);
        }
    }
}
static void mqttclient_task_init(void)
{
    luat_rtos_task_handle mqttclient_task_handle;
    luat_rtos_task_create(&mqttclient_task_handle, 4 * 1024, 80, "mqttclient", mqtt_demo, NULL, NULL);
}
/*------------------------------------------------------MQTT event-----------------------------------------------------------------*/

INIT_HW_EXPORT(Task_netinfo_call, "0");
INIT_TASK_EXPORT(mqttclient_task_init, "1");
INIT_TASK_EXPORT(Mqtt_payload_task_Init, "1");
INIT_TASK_EXPORT(audio_task_init, "2");
INIT_TASK_EXPORT(NET_LED_Task, "3");
