#include "luat_debug.h"
#include "luat_rtos.h"
#include "mqtt_publish.h"
#include "MQTTClient.h"
luat_rtos_task_handle mqtt_publish_task_handle = NULL;

void mqtt_publish_task(void *args)
{
    uint32_t id;    //暂时没什么用，因为只有publish的时候才会触发这条消息
    int rc;
    MQTTClient *mqttClient;
    mqttClient = (MQTTClient *)(args);
    mqtt_publish_para_t *publish_para = NULL;
    while (1)
    { 
        if(luat_rtos_message_recv(mqtt_publish_task_handle, &id, (void **)&publish_para, LUAT_WAIT_FOREVER) == 0)
        {
            rc = MQTTPublish(mqttClient, publish_para->publish->topic, &publish_para->publish->message);
            if (rc != 0){
				LUAT_DEBUG_PRINT("MQTTPublish error %d", rc);
			}
            else
            {
                LUAT_DEBUG_PRINT("MQTTPublish success %d", rc);
            }
            
            if (publish_para->callback != NULL)
                publish_para->callback(rc);

            if(publish_para->publish->topic != NULL)
            {
                free(publish_para->publish->topic);
            }

            if(publish_para->publish->message.payload != NULL)
            {
                free(publish_para->publish->message.payload);
            }

            if(publish_para->publish != NULL)
            {
                free(publish_para->publish);
            }
            if(publish_para != NULL)
            {
                free(publish_para);
            }
        }
    }
}