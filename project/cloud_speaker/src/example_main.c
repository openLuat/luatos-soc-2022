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

#include "bsp.h"
#include "bsp_custom.h"
#include "common_api.h"
#include "cJSON.h"
#include "MQTTClient.h"
#include "audio_task.h"
#include "audio_file.h"
#include "math.h"
#include "luat_rtos.h"
#include "luat_mobile.h"
#include "luat_debug.h"

static luat_rtos_semaphore_t net_semaphore_handle;
static luat_rtos_task_handle mqtt_task_handle;
extern luat_rtos_queue_t audio_queue_handle;

#define MQTT_HOST    	"lbsmqtt.airm2m.com"   				// MQTT服务器的地址和端口号
#define MQTT_PORT		 1884


const static char mqtt_sub_topic_head[] = "/sub/topic/money/";    //订阅的主题头，待与设备imei进行拼接
const static char mqtt_pub_topic[] = "/pub/topic/message";       //发布的主题
static char mqtt_send_payload[] = "hello mqtt_test!!!";
static char mqtt_sub_topic[40];                            //订阅的主题，待存放订阅的主题头和设备imei，共计17+15,32个字符
static bool netStatus = false;
static bool serverStatus = false;

bool getNetStatus()
{
    return netStatus;
}
bool getServerStatus()
{
    return serverStatus;
}

int fomatMoney(int num, audioQueueData *data, int *index, BOOL flag)
{
     uint32_t  audioArray[10][2] = 
     {
         {audio0,   sizeof(audio0)},
         {audio1,   sizeof(audio1)},
         {audio2,   sizeof(audio2)},
         {audio3,   sizeof(audio3)},
         {audio4,   sizeof(audio4)},
         {audio5,   sizeof(audio5)},
         {audio6,   sizeof(audio6)},
         {audio7,   sizeof(audio7)},
         {audio8,   sizeof(audio8)},
         {audio9,   sizeof(audio9)}
    };
    int thousand = (num - num % 1000) / 1000;
    int hundred = ((num % 1000) - ((num % 1000) % 100)) / 100;
    int ten = ((num % 100) - ((num % 100) % 10)) / 10;
    int unit = num % 10;
    if (thousand == 0)
    {
        thousand = -1;
        if (hundred == 0)
        {
            hundred = -1;
            if (ten == 0)
            {
                ten = -1;
                if (unit == 0)
                {
                    unit = -1;
                }
            }
        }
    }
    if (unit == 0)
    {
        unit = -1;
        if (ten == 0)
        {
            ten = -1;
            if (hundred == 0)
            {
                hundred = -1;
                if (thousand == 0)
                {
                    thousand = -1;
                }
            }
        }
    }
    if (ten == 0 && hundred == 0)
    {
        ten = -1;
    }
    if (thousand != -1)
    {
        if (flag)
        {
            data->message.file.info[*index].path = NULL; 
            if(2 == thousand)
            {
                data->message.file.info[*index].address = audioliang;
                data->message.file.info[*index].rom_data_len = audioliangSize;
            }
            else
            {
                data->message.file.info[*index].address = audioArray[thousand][0];
                data->message.file.info[*index].rom_data_len = audioArray[thousand][1];
            }
        }
        *index += 1;
        if (flag)
        {
            data->message.file.info[*index].path = NULL; 
            data->message.file.info[*index].address = audio1000;
            data->message.file.info[*index].rom_data_len = sizeof(audio1000);
        }
        *index += 1;
    }
    if (hundred != -1)
    {
        if(flag)
        {
            data->message.file.info[*index].path = NULL; 
            if(2 == hundred)
            {
                data->message.file.info[*index].address = audioliang;
                data->message.file.info[*index].rom_data_len = audioliangSize;
            }
            else
            {
                data->message.file.info[*index].address = audioArray[hundred][0];
                data->message.file.info[*index].rom_data_len = audioArray[hundred][1];
        }
        }
        *index += 1;
        if(flag)
        {
            data->message.file.info[*index].path = NULL; 
            data->message.file.info[*index].address = audio100;
            data->message.file.info[*index].rom_data_len = sizeof(audio100);
        }
        *index += 1;
    }
    if (ten != -1)
    {
        if (!(ten == 1 && hundred == -1 && thousand == -1))
        {
            if(flag)
            {
                data->message.file.info[*index].path = NULL; 
                data->message.file.info[*index].address = audioArray[ten][0];
                data->message.file.info[*index].rom_data_len = audioArray[ten][1];
            }
            *index += 1;
        }
        if (ten != 0)
        {
            if(flag)
            {
                data->message.file.info[*index].path = NULL; 
                data->message.file.info[*index].address = audio10;
                data->message.file.info[*index].rom_data_len = sizeof(audio10);
            }
            *index += 1;
        }
    }
    if (unit != -1)
    {
        if(flag)
        {
            data->message.file.info[*index].path = NULL; 
            data->message.file.info[*index].address = audioArray[unit][0];
            data->message.file.info[*index].rom_data_len = audioArray[unit][1];
        }
        *index += 1;
    }
    return 0;
}
static int strToFile(char *money, audioQueueData *data, int *index, bool flag)
{
    if (flag)
    {
        data->message.file.info[*index].address  = audiozhifubao;
        data->message.file.info[*index].rom_data_len  = sizeof(audiozhifubao);
    }
    *index += 1;
     uint32_t  audioArray[10][2] = 
     {
         {audio0, sizeof(audio0)},
         {audio1, sizeof(audio1)},
         {audio2, sizeof(audio2)},
         {audio3, sizeof(audio3)},
         {audio4, sizeof(audio4)},
         {audio5, sizeof(audio5)},
         {audio6, sizeof(audio6)},
         {audio7, sizeof(audio7)},
         {audio8, sizeof(audio8)},
         {audio9, sizeof(audio9)}
    };
    int count = 0;
    int integer = 0;
    char *str = NULL;
    char intStr[8] = {0};
    char decStr[3] = {0};
    str = strstr(money, ".");
    if (str != NULL)
    {
        memcpy(intStr, money, str - money);
        str = str + 1;
        memcpy(decStr, str, 2);
        integer = atoi(intStr);
    }
    else
    {
        integer = atoi(money);
    }
    if (integer >= 10000)
    {
        int filecount = fomatMoney(integer / 10000, data, index, flag);
        if (flag)
        {
            if((2 == *index) && (data->message.file.info[1].address == audio2))
            {
                data->message.file.info[1].address = audioliang;
                data->message.file.info[1].rom_data_len = sizeof(audioliang);
            }
            data->message.file.info[*index].path = NULL; 
            data->message.file.info[*index].address = audio10000;
            data->message.file.info[*index].rom_data_len = sizeof(audio10000);
        }
        *index += 1;
        if (((integer % 10000) < 1000) && ((integer % 10000) != 0))
        {
            if (flag)
            {
                data->message.file.info[*index].path = NULL; 
                data->message.file.info[*index].address = audioArray[0][0];
                data->message.file.info[*index].rom_data_len = audioArray[0][1];
            }
            *index += 1;
        }
    }
    if ((integer % 10000) > 0)
    {
        int filecount = fomatMoney(integer % 10000, data, index, flag);
    }
    if (*index == 1)
    {
        if (flag)
        {
            data->message.file.info[*index].path = NULL; 
            data->message.file.info[*index].address = audioArray[0][0];
            data->message.file.info[*index].rom_data_len = audioArray[0][1];
        }
        *index += 1;
    }
    int decial = atoi(decStr);
    if (decial > 0)
    {
        if (flag)
        {
            data->message.file.info[*index].path = NULL; 
            data->message.file.info[*index].address = audiodot;
            data->message.file.info[*index].rom_data_len = sizeof(audiodot);
        }
        *index += 1;
        if (decial > 10)
        {
            int ten = decial / 10;
            int unit = decial % 10;
            if (ten != 0 && unit != 0)
            {
                if (flag)
                {
                    data->message.file.info[*index].path = NULL; 
                    data->message.file.info[*index].address = audioArray[ten][0];
                    data->message.file.info[*index].rom_data_len = audioArray[0][1];
                }
                *index += 1;
                if(flag)
                {
                    data->message.file.info[*index].path = NULL; 
                    data->message.file.info[*index].address = audioArray[unit][0];
                    data->message.file.info[*index].rom_data_len = audioArray[0][1];
                }
                *index += 1;
            }
            else if(ten == 0 && unit!=0)
            {
                if (flag)
                {
                    data->message.file.info[*index].path = NULL; 
                    data->message.file.info[*index].address = audioArray[0][0];
                    data->message.file.info[*index].rom_data_len = audioArray[0][1];
                }
                *index += 1;
                if(flag)
                {
                    data->message.file.info[*index].path = NULL; 
                    data->message.file.info[*index].address = audioArray[unit][0];
                    data->message.file.info[*index].rom_data_len = audioArray[0][1];
                }
                *index += 1;
            }
            else if(ten !=0 && unit == 0)
            {
                if (flag)
                {
                    data->message.file.info[*index].path = NULL; 
                    data->message.file.info[*index].address = audioArray[0][0];
                    data->message.file.info[*index].rom_data_len = audioArray[0][1];
                }
                *index += 1;
            }
        }
        else
        {
            if (flag)
            {
                data->message.file.info[*index].path = NULL; 
                data->message.file.info[*index].address = audioArray[decial][0];
                data->message.file.info[*index].rom_data_len = audioArray[decial][1];
            }
            *index += 1;
        }
    }
    if (flag)
    {
        data->message.file.info[*index].path = NULL; 
        data->message.file.info[*index].address = audioyuan;
        data->message.file.info[*index].rom_data_len = sizeof(audioyuan);
    }
    *index += 1;
    return count;
}

void messageArrived(MessageData* data)
{
    if (memcmp(mqtt_sub_topic, data->topicName->lenstring.data, strlen(mqtt_sub_topic)) == 0)
    {
        cJSON *boss = NULL;
        LUAT_DEBUG_PRINT("cloud_speaker_mqtt mqtt Message arrived on topic %.*s: %.*s\n", data->topicName->lenstring.len, data->topicName->lenstring.data, data->message->payloadlen, data->message->payload);
        boss = cJSON_Parse((const char *)data->message->payload);
        if (boss == NULL){
            LUAT_DEBUG_PRINT("cloud_speaker_mqtt cjson parse fail");
        }
        else
        {
            LUAT_DEBUG_PRINT("cloud_speaker_mqtt cjson parse success");
            cJSON *money = cJSON_GetObjectItem(boss, "money");
            if(money == NULL)
            {
                LUAT_DEBUG_PRINT("cloud_speaker_mqtt Missing amount field %d", money);
                return 0;
            }
            if (cJSON_IsString(money))
            {
                audioQueueData moneyPlay = {0};
                moneyPlay.priority = MONEY_PLAY;
                moneyPlay.playType = FILE_PLAY;
                char* str = NULL;
                str = strstr(money->valuestring, ".");
                //判断金额长度是否大于8个，也就是千万级别的金额，如果是，则播报收款成功，如果不是，则播报对应金额，这里并未对金额字段做合法性判断
                if (str != NULL)
                {
                    if((str - money->valuestring) > 8)
                    {
                        moneyPlay.message.file.info = (audio_play_info_t *)calloc(1, sizeof(audio_play_info_t));
                        moneyPlay.message.file.info->address = audioshoukuanchenggong;
                        moneyPlay.message.file.info->rom_data_len = audioshoukuanchenggongSize;
                        moneyPlay.message.file.count = 1;
                    }
                    else
                    {
                        str++;
                        //如果小数点位数大于两位，则说明数字金额不合法，播报收款成功
                        if(strlen(str) > 2)
                        {
                            moneyPlay.message.file.info = (audio_play_info_t *)calloc(1, sizeof(audio_play_info_t));
                            moneyPlay.message.file.info->address = audioshoukuanchenggong;
                            moneyPlay.message.file.info->rom_data_len = audioshoukuanchenggongSize;
                            moneyPlay.message.file.count = 1;
                        }
                        else
                        {
                            //调用strToFile来将金额格式化为对应的文件播报数据，需要调用两次，第一次获取需要malloc的空间，第二次将文件数据放进空间里
                            int index = 0;
                            strToFile(money->valuestring, &moneyPlay, &index, false);
                            moneyPlay.message.file.info = (audio_play_info_t *)calloc(index, sizeof(audio_play_info_t));
                            index = 0;
                            strToFile(money->valuestring, &moneyPlay, &index, true);
                            moneyPlay.message.file.count = index;
                        }
                    }
                }
                else
                {
                    if(strlen(money->valuestring) > 8)
                    {
                        moneyPlay.message.file.info = (audio_play_info_t *)calloc(1, sizeof(audio_play_info_t));
                        moneyPlay.message.file.info->address = audioshoukuanchenggong;
                        moneyPlay.message.file.info->rom_data_len = audioshoukuanchenggongSize;
                        moneyPlay.message.file.count = 1;
                    }
                    else
                    {
                        //调用strToFile来将金额格式化为对应的文件播报数据，需要调用两次，第一次获取需要malloc的空间，第二次将文件数据放进空间里
                        int index = 0;
                        strToFile(money->valuestring, &moneyPlay, &index, false);
                        moneyPlay.message.file.info = (audio_play_info_t *)calloc(index, sizeof(audio_play_info_t));
                        index = 0;
                        strToFile(money->valuestring, &moneyPlay, &index, true);
                        moneyPlay.message.file.count = index;
                    }
                }
                if (-1 == luat_rtos_queue_send(audio_queue_handle, &moneyPlay, NULL, 0)){
                    LUAT_DEBUG_PRINT("cloud_speaker_mqtt sub queue send error");
                }
            }
            else
            {
                LUAT_DEBUG_PRINT("cloud_speaker_mqtt money data is invalid %d", cJSON_IsString(money));
            }
        }
        cJSON_Delete(boss);
    }
}

void mobile_event_callback(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status){
    if (event == LUAT_MOBILE_EVENT_NETIF && status == LUAT_MOBILE_NETIF_LINK_ON){
        LUAT_DEBUG_PRINT("network ready");
        luat_rtos_semaphore_release(net_semaphore_handle);
    }
}

static void mqtt_demo(void){
    luat_rtos_semaphore_create(&net_semaphore_handle, 1);
    luat_mobile_event_register_handler(mobile_event_callback);
    luat_rtos_semaphore_take(net_semaphore_handle, LUAT_WAIT_FOREVER);
	int rc = 0;
    MQTTClient mqttClient = {0};
    Network mqttNetwork = {0};

    MQTTMessage message = {0};
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
    connectData.MQTTVersion = 4;
    int ret = 0;
    char str[32] = {0};
    char clientId[17] = {0};
    char username[17] = {0};
    char password[17] = {0};
    ret = luat_kv_get("clientId", str, 17);             //从数据库中读取clientId
    if(ret > 0 )
    {
        memcpy(clientId, str, 16);                      //留一位确保字符串结尾能有0x00
        connectData.clientID.cstring = clientId;
    }
    else                                                //数据库中没有写入过clientId，获取设备imei作为clientId
    {
        int result = 0;
        result = luat_mobile_get_imei(0, clientId, 15); //imei是15位，留一个位置放0x00
        if(result <= 0)
        {
            LUAT_DEBUG_PRINT("cloud_speaker_mqtt clientid get fail");
        }
    }
    memset(str, 0, 32);
    ret = luat_kv_get("username", str, 17);             //从数据库中读取username，如果没读到，则用默认的
    if(ret > 0 )
    {
        memcpy(username, str, 16);                      //留一位确保字符串结尾能有0x00
        connectData.username.cstring = username;
    }
    else
    {
        connectData.username.cstring = NULL;
    }
    memset(str, 0, 32);
    ret = luat_kv_get("password", str, 17);             //从数据库中读取password，如果没读到，则用默认的
    if(ret > 0 )
    {
        memcpy(password, str, 16);                      //留一位确保字符串结尾能有0x00
        connectData.password.cstring = password;
    }
    else
    {
        connectData.password.cstring = NULL;
    }
    memset(str, 0, 32);
    memset(mqtt_sub_topic, 0x00, sizeof(mqtt_sub_topic));
    snprintf(mqtt_sub_topic, 40, "%s%s", mqtt_sub_topic_head, clientId);
    LUAT_DEBUG_PRINT("cloud_speaker_mqtt subscribe_topic %s %s %s %s", mqtt_sub_topic, clientId, username, password);
    connectData.keepAliveInterval = 120;

    mqtt_connect(&mqttClient, &mqttNetwork, MQTT_HOST, MQTT_PORT, &connectData);

    if ((rc = MQTTSubscribe(&mqttClient, mqtt_sub_topic, 0, messageArrived)) != 0)
        {
        LUAT_DEBUG_PRINT("cloud_speaker_mqtt Return code from MQTT subscribe is %d\n", rc);
            serverStatus = false;
        }
        else
        {
            serverStatus = true;
            audioQueueData welcome = {0};
            welcome.playType = TTS_PLAY;
            welcome.priority = MONEY_PLAY;
            char str[] = "服务器连接成功"; 
            welcome.message.tts.data = malloc(sizeof(str));
            memcpy(welcome.message.tts.data, str, sizeof(str));
            welcome.message.tts.len = sizeof(str);
            if (-1 == luat_rtos_queue_send(audio_queue_handle, &welcome, NULL, 0)){
                LUAT_DEBUG_PRINT("cloud_speaker_mqtt sub queue send error");
            }
        }
    
    while(1){
        int len = strlen(mqtt_send_payload);
        message.qos = 1;
		message.retained = 0;
        message.payload = mqtt_send_payload;
        message.payloadlen = len;
        LUAT_DEBUG_PRINT("cloud_speaker_mqtt publish data");
        MQTTPublish(&mqttClient, mqtt_pub_topic, &message);
#if !defined(MQTT_TASK)
		if ((rc = MQTTYield(&mqttClient, 1000)) != 0)
			LUAT_DEBUG_PRINT("cloud_speaker_mqtt Return code from yield is %d\n", rc);
#endif
        luat_rtos_task_sleep(60000);
    }
    luat_rtos_task_delete(mqtt_task_handle);
}




static void mqttclient_task_init(void){
    int result = luat_rtos_task_create(&mqtt_task_handle, 4096, 20, "mqtt", mqtt_demo, NULL, NULL);
    LUAT_DEBUG_PRINT("cloud_speaker_mqtt create task result %d", result);
}

extern void led_task_init(void);
extern void key_task_init(void);
extern void charge_task_init(void);
extern void usb_uart_init(void);
extern void fdb_init(void);

INIT_DRV_EXPORT(fdb_init, "2");
INIT_TASK_EXPORT(mqttclient_task_init, "2");
INIT_TASK_EXPORT(audio_task_init, "2");
INIT_TASK_EXPORT(led_task_init, "2");
INIT_TASK_EXPORT(key_task_init, "2");
INIT_TASK_EXPORT(charge_task_init, "2");
INIT_TASK_EXPORT(usb_uart_init, "2");

