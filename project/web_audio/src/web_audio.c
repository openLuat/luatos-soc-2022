#include "web_audio.h"
// 861551056133283
uint8_t link_UP = 0;
char mqtt_subTopic[40];
static char subTopic[] = "test20220929/";
QueueHandle_t audioQueueHandle = NULL;
static HttpClientContext AirM2mhttpClient;

#define HTTP_RECV_BUF_SIZE      (11520)
#define HTTP_HEAD_BUF_SIZE      (800)

/*-------------------------------------------------audio define-----------------------------------------------*/
static osEventFlagsId_t waitAudioPlayDone = NULL;
static HANDLE g_s_delay_timer;

/*-------------------------------------------------audio define-----------------------------------------------*/
/*------------------------------------------------audio-----------------------------------------------*/

void audio_data_cb(uint8_t *data, uint32_t len, uint8_t bits, uint8_t channels)
{
    // LUAT_DEBUG_PRINT("%x,%d,%d,%d", data, len, bits, channels);
}
void app_pa_on(uint32_t arg)
{
    luat_gpio_set(PA_PWR_PIN, 1);
}
void audio_event_cb(uint32_t event, void *param)
{
    //	PadConfig_t pad_config;
    //	GpioPinConfig_t gpio_config;

    LUAT_DEBUG_PRINT("%d", event);
    switch (event)
    {
    case LUAT_MULTIMEDIA_CB_AUDIO_DECODE_START:
        luat_gpio_set(CODEC_PWR_PIN, 1);
        luat_audio_play_write_blank_raw(0, 6, 1);
        break;
    case LUAT_MULTIMEDIA_CB_AUDIO_OUTPUT_START:
        luat_rtos_timer_start(g_s_delay_timer, 200, 0, app_pa_on, NULL);
        break;
    case LUAT_MULTIMEDIA_CB_TTS_INIT:
        audio_play_tts_set_param(0, ivTTS_PARAM_INPUT_CODEPAGE, ivTTS_CODEPAGE_GBK);
        break;
    case LUAT_MULTIMEDIA_CB_TTS_DONE:
        if (!luat_audio_play_get_last_error(0))
        {
            luat_audio_play_write_blank_raw(0, 1, 0);
        }
        break;
    case LUAT_MULTIMEDIA_CB_AUDIO_DONE:
        luat_rtos_timer_stop(g_s_delay_timer);
        LUAT_DEBUG_PRINT("audio play done, result=%d!", luat_audio_play_get_last_error(0));
        luat_gpio_set(PA_PWR_PIN, 0);
        luat_gpio_set(CODEC_PWR_PIN, 0);
        break;
    }
}
void audio_task(void *param)
{
    ivCStrA sdk_id = AISOUND_SDK_USERID_16K;
    luat_rtos_timer_create(&g_s_delay_timer);
    luat_audio_play_global_init(audio_event_cb, audio_data_cb, luat_audio_play_file_default_fun, luat_audio_play_tts_default_fun, NULL);
    luat_audio_play_tts_set_resource(ivtts_16k, sdk_id, NULL);
    //air780E开发板使用了ES7149，可以使用下面的配置，ES7148也可以
    luat_i2s_base_setup(0, I2S_MODE_I2S, I2S_FRAME_SIZE_16_16);
    audioQueueData audioQueueRecv = {0};
    uint32_t result = 0;
    while (1)
    {
        if (xQueueReceive(audioQueueHandle, &audioQueueRecv, portMAX_DELAY))
        {
            // audio_play_tts_text(0, audioQueueRecv.data, sizeof(audioQueueRecv.data));
            LUAT_DEBUG_PRINT("this is play priority %d", audioQueueRecv.priority);
            LUAT_DEBUG_PRINT("this is play playType %d", audioQueueRecv.playType);
            if (audioQueueRecv.priority == MONEY_PLAY)
            {

                if (audioQueueRecv.playType == TTS_PLAY)
                {
                    // DBG("TEST data address %d", sizeof(audioQueueRecv.message.data));
                    luat_audio_play_tts_text(0, audioQueueRecv.message.tts.data, audioQueueRecv.message.tts.len);
                }
                else if (audioQueueRecv.playType == FILE_PLAY)
                {
                    LUAT_DEBUG_PRINT("TEST address 2 %p", audioQueueRecv.message.file.info);
                    luat_audio_play_multi_files(0, audioQueueRecv.message.file.info, audioQueueRecv.message.file.count);
                }
            }
            else if (audioQueueRecv.priority == PAD_PLAY)
            {
            }
            result = osEventFlagsWait(waitAudioPlayDone, WAIT_PLAY_FLAG, osFlagsWaitAll, 20000);
            if (audioQueueRecv.playType == TTS_PLAY)
            {
                LUAT_DEBUG_PRINT("FREE MY AUDIO TTS");
                free(audioQueueRecv.message.tts.data);
            }
            else if (audioQueueRecv.playType == FILE_PLAY)
            {
                free(audioQueueRecv.message.file.info);
                LUAT_DEBUG_PRINT("FREE MY AUDIO FILE");
            }
        }
        //  luat_audio_play_tts_text(0, str, sizeof(str));
        // luat_rtos_task_sleep(1000);
    }
}

void audio_task_init(void)
{
    luat_gpio_cfg_t gpio_cfg;
    luat_gpio_set_default_cfg(&gpio_cfg);
    luat_rtos_task_handle audio_task_handle;

    gpio_cfg.pin = LED2_PIN;
    gpio_cfg.pull = LUAT_GPIO_DEFAULT;
    luat_gpio_open(&gpio_cfg);
    gpio_cfg.pin = LED3_PIN;
    luat_gpio_open(&gpio_cfg);
    gpio_cfg.pin = LED4_PIN;
    luat_gpio_open(&gpio_cfg);
    gpio_cfg.pin = CHARGE_EN_PIN;
    luat_gpio_open(&gpio_cfg);
    gpio_cfg.pin = PA_PWR_PIN;
    luat_gpio_open(&gpio_cfg);
    gpio_cfg.pin = CODEC_PWR_PIN;
    luat_gpio_open(&gpio_cfg);
    gpio_cfg.alt_fun = CODEC_PWR_PIN_ALT_FUN;
    luat_gpio_open(&gpio_cfg);
    audioQueueHandle = xQueueCreate(100, sizeof(audioQueueData));

    audioQueueData powerOn = {0};
    powerOn.playType = TTS_PLAY;
    powerOn.priority = MONEY_PLAY;
    char str[] = "欢迎使用,合宙云端 音频播放设备";
    powerOn.message.tts.data = malloc(sizeof(str));
    memcpy(powerOn.message.tts.data, str, sizeof(str));
    powerOn.message.tts.len = sizeof(str);
    if (pdTRUE != xQueueSend(audioQueueHandle, &powerOn, 0))
    {
        DBG("start send audio fail");
    }
    luat_rtos_task_create(&audio_task_handle,  5*1024, 60, "audio", audio_task, NULL, NULL);
}
/*------------------------------------------------audio-----------------------------------------------*/
/*------------------------------------------------http------------------------------------------------*/
static INT32 httpGetData(CHAR *getUrl, CHAR *buf, UINT32 len)
{
    HTTPResult result = HTTP_INTERNAL;
    HttpClientData    clientData = {0};
    UINT32 count = 0;
    uint16_t headerLen = 0;

    LUAT_DEBUG_ASSERT(buf != NULL,0,0,0);

    clientData.headerBuf = malloc(HTTP_HEAD_BUF_SIZE);
    clientData.headerBufLen = HTTP_HEAD_BUF_SIZE;
    clientData.respBuf = buf;
    clientData.respBufLen = len;
    result = httpSendRequest(&AirM2mhttpClient, getUrl, HTTP_GET, &clientData);
    LUAT_DEBUG_PRINT("send request result=%d", result);
    if (result != HTTP_OK)
        goto exit;
    do {
    	LUAT_DEBUG_PRINT("recvResponse loop.");
        memset(clientData.headerBuf, 0, clientData.headerBufLen);
        memset(clientData.respBuf, 0, clientData.respBufLen);
        result = httpRecvResponse(&AirM2mhttpClient, &clientData);
        if(result == HTTP_OK || result == HTTP_MOREDATA){
            headerLen = strlen(clientData.headerBuf);
            if(headerLen > 0)
            {
            	LUAT_DEBUG_PRINT("total content length=%d", clientData.recvContentLength);
            }

            if(clientData.blockContentLen > 0)
            {
            	LUAT_DEBUG_PRINT("response content:{%s}", (uint8_t*)clientData.respBuf);
            }
            count += clientData.blockContentLen;
            LUAT_DEBUG_PRINT("has recv=%d", count);
        }
    } while (result == HTTP_MOREDATA || result == HTTP_CONN);

    LUAT_DEBUG_PRINT("result=%d", result);
    if (AirM2mhttpClient.httpResponseCode < 200 || AirM2mhttpClient.httpResponseCode > 404)
    {
    	LUAT_DEBUG_PRINT("invalid http response code=%d",AirM2mhttpClient.httpResponseCode);
    } else if (count == 0 || count != clientData.recvContentLength) {
    	LUAT_DEBUG_PRINT("data not receive complete");
    } else {
    	LUAT_DEBUG_PRINT("receive success");
    }
exit:
    free(clientData.headerBuf);
    return result;
}

static void task_test_https(CHAR *getUrl, uint16_t *buf, UINT32 len)
{
    //HttpClientData    clientData = {0};
	//char *recvBuf = malloc(HTTP_RECV_BUF_SIZE);
	HTTPResult result = HTTP_INTERNAL;
    
    result = httpConnect(&AirM2mhttpClient,getUrl);
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
void messageArrived(MessageData *data)
{
    luat_audio_play_info_t info[1];
    memset(info, 0, sizeof(info));
    static uint32_t *tmpbuff[HTTP_RECV_BUF_SIZE]={0};
    uint32_t status;
    LUAT_DEBUG_PRINT("mqtt Message arrived on topic %d%s: %d%s", data->topicName->lenstring.len, data->topicName->lenstring.data, data->message->payloadlen, data->message->payload);
    char *p = (char *)data->message->payload;
    uint8_t payload_len = data->message->payloadlen;
    for (size_t i = 0; i < data->message->payloadlen; i++)
    {
        LUAT_DEBUG_PRINT("ceshi%c", p[i]);
    }
    if (p[0] == 0)
    {
        uint16_t TTS_Len = p[4];
        CHAR *TTS=malloc(TTS_Len*sizeof(char));
        if (TTS_Len == payload_len - 5)
        {
            memcpy(TTS, p + 5, TTS_Len);
            luat_audio_play_tts_text(0, TTS, TTS_Len);
        }
        else
        {
            int URL_Len = p[9 + TTS_Len];
            CHAR *URL=malloc(URL_Len*sizeof(char));
            memset(URL,'\0',URL_Len*sizeof(char));
            memcpy(TTS, p + 5, TTS_Len);
            memcpy(URL, p + 10 + TTS_Len, URL_Len);
            task_test_https(URL, tmpbuff, HTTP_RECV_BUF_SIZE);
            info[0].path = NULL;
            info[0].address = (uint32_t)tmpbuff;
            info[0].rom_data_len = sizeof(tmpbuff);
            luat_audio_play_tts_text(0, TTS, TTS_Len);
            luat_rtos_task_sleep(2000);
            luat_audio_play_multi_files(0, info, 1);
            free(URL);
        }
        luat_rtos_task_sleep(2000);
        free(TTS);
        
    }
    else if (p[0] == 1)
    {
        uint16_t URL_Len = p[4];
        CHAR *URL=malloc(URL_Len*sizeof(char));
        memset(URL,'\0',URL_Len*sizeof(char));
        if (URL_Len == payload_len - 5)
        {
            memcpy(URL, p + 5, URL_Len);
            task_test_https(URL, tmpbuff, HTTP_RECV_BUF_SIZE);
            FILE* fp1 = luat_fs_fopen("test1.mp3", "wb+");
            status = luat_fs_fwrite((uint8_t *)tmpbuff, sizeof(tmpbuff), 1, fp1);
            if (status == 0)
            {
                while (1);
            }
            luat_fs_fclose(fp1);
            info[0].path = "test1.mp3";
            luat_audio_play_multi_files(0, info, 1);
            luat_rtos_task_sleep(3000);
        }
        else
        {

            int TTS_Len = p[9 + URL_Len];
            CHAR *TTS=malloc(TTS_Len*sizeof(char));
            memcpy(URL, p + 5, URL_Len);
            memcpy(TTS, p + 10 + URL_Len, TTS_Len);
            task_test_https(URL, tmpbuff, HTTP_RECV_BUF_SIZE);
            info[0].path = NULL;
            info[0].address = (uint32_t)tmpbuff;
            info[0].rom_data_len = sizeof(tmpbuff);
            luat_audio_play_multi_files(0, info, 1);
            luat_rtos_task_sleep(2000);
            luat_audio_play_tts_text(0, TTS, TTS_Len);
            free(TTS);
        
        }
        free(URL);
    }
    luat_rtos_task_sleep(2000);
    
    memset(tmpbuff, 0, HTTP_RECV_BUF_SIZE);
}

static void mqtt_demo(void)
{
    CHAR IMEI[20] = {0};
    if (luat_mobile_get_imei(0, IMEI, sizeof(IMEI)))
    {
        sprintf(mqtt_subTopic, "%s%s", subTopic, IMEI);
        LUAT_DEBUG_PRINT("imei%s", mqtt_subTopic);
    }
    int rc = 0;
    MQTTClient mqttClient;
    static Network n = {0};
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
    connectData.MQTTVersion = 4;
    connectData.clientID.cstring = client_id;
    connectData.username.cstring = User;
    connectData.password.cstring = Password;
    connectData.keepAliveInterval = 120;
    while (1)
    {
        while (!link_UP)
        {
            luat_rtos_task_sleep(1000);
        }
        if ((rc = mqtt_connect(&mqttClient, &n, HOST, PORT, &connectData)) == 0)
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
            char str[] = "服务器连接失败";
            MQTT_link.message.tts.data = malloc(sizeof(str));
            memcpy(MQTT_link.message.tts.data, str, sizeof(str));
            MQTT_link.message.tts.len = sizeof(str);
            if (pdTRUE != xQueueSend(audioQueueHandle, &MQTT_link, 0))
            {
                LUAT_DEBUG_PRINT("start send audio fail");
            }
        }

        if ((rc = MQTTSubscribe(&mqttClient, mqtt_subTopic, 0, messageArrived)) != 0)
            LUAT_DEBUG_PRINT("mqtt Return code from MQTT subscribe is %d\n", rc);

        while (1)
        {
            luat_rtos_task_sleep(1000);
        }
    }
}
static void mqttclient_task_init(void)
{
    luat_rtos_task_handle mqttclient_task_handle;
    luat_rtos_task_create(&mqttclient_task_handle, 4*1024, 80, "mqttclient", mqtt_demo, NULL, NULL);
}
/*------------------------------------------------------MQTT event-----------------------------------------------------------------*/

/*-----------------------------------------------------------mobile event----------------------------------------------------------*/
static void mobile_event_callback_t(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status)
{
    switch (event)
    {
    case LUAT_MOBILE_EVENT_NETIF:
        switch (status)
        {
        case LUAT_MOBILE_NETIF_LINK_ON:
            LUAT_DEBUG_PRINT("网络连接成功");
            link_UP = 1;
            audioQueueData net_link = {0};
            net_link.playType = TTS_PLAY;
            net_link.priority = MONEY_PLAY;
            char str[] = "网络注册成功";
            net_link.message.tts.data = malloc(sizeof(str));
            memcpy(net_link.message.tts.data, str, sizeof(str));
            net_link.message.tts.len = sizeof(str);
            if (pdTRUE != xQueueSend(audioQueueHandle, &net_link, 0))
            {
                LUAT_DEBUG_PRINT("start send audio fail");
            }
            break;
        default:
            link_UP = 0;
            LUAT_DEBUG_PRINT("网络注册失败");
            break;
        }
    case LUAT_MOBILE_EVENT_SIM:
        switch (status)
        {
        case LUAT_MOBILE_SIM_READY:break;
        case LUAT_MOBILE_NO_SIM:
            LUAT_DEBUG_PRINT("请插入SIM卡");
            audioQueueData sim_state = {0};
            sim_state.playType = TTS_PLAY;
            sim_state.priority = MONEY_PLAY;
            char str[] = "请插入西木卡";
            sim_state.message.tts.data = malloc(sizeof(str));
            memcpy(sim_state.message.tts.data, str, sizeof(str));
            sim_state.message.tts.len = sizeof(str);
            if (pdTRUE != xQueueSend(audioQueueHandle, &sim_state, 0))
            {
                LUAT_DEBUG_PRINT("start send audio fail");
            }
        default:
            break;
        }
    default:
        break;
    }
}
static void Task_netinfo_call(void)
{
    luat_mobile_event_register_handler(mobile_event_callback_t);
}
/*-----------------------------------------------------------mobile event-------------------------------------------------------*/


/*-----------------------------------------------------------NET_LED begin-------------------------------------------------------*/
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


/*-----------------------------------------------------------NET_LED end-------------------------------------------------------*/




INIT_HW_EXPORT(Task_netinfo_call, "0");
INIT_TASK_EXPORT(mqttclient_task_init, "1");
INIT_TASK_EXPORT(audio_task_init, "2");
INIT_TASK_EXPORT(NET_LED_Task, "3");
