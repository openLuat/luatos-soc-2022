#ifndef __MQTT_PUBLISH_H__
#define __MQTT_PUBLISH_H__
#include "MQTTClient.h"
typedef struct mqtt_publish_para
{
    mqttSendMsg* publish;
    void (*callback)(int result);
}mqtt_publish_para_t;
#endif