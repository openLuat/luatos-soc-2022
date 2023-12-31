#ifndef _LUAT_WIFISCAN_H_
#define _LUAT_WIFISCAN_H_

#define Luat_MAX_CHANNEL_NUM     14

#include "luat_base.h"
/**
 * @defgroup luat_wifiscan wifiscan扫描接口
 * @{
 */

/// @brief wifiscan 扫描的优先级
typedef enum luat_wifiscan_set_priority
{
    LUAT_WIFISCAN_DATA_PERFERRD=0,/**< 数据优先*/
    LUAT_WIFISCAN_WIFI_PERFERRD
}luat_wifiscan_set_priority_t;

/// @brief wifiscan 控制参数结构体
typedef struct luat_wifiscan_set_info
{
    int   maxTimeOut;         //ms, 最大执行时间 取值范围4000~255000
    uint8_t   round;              //wifiscan total round 取值范围1~3
    uint8_t   maxBssidNum;        //wifiscan max report num 取值范围4~40
    uint8_t   scanTimeOut;        //s, max time of each round executed by RRC 取值范围1~255
    uint8_t   wifiPriority;       //CmiWifiScanPriority
    uint8_t   channelCount;       //channel count; if count is 1 and all channelId are 0, UE will scan all frequecny channel
    uint8_t   rsvd[3];
    uint16_t  channelRecLen;      //ms, max scantime of each channel
    uint8_t   channelId[Luat_MAX_CHANNEL_NUM];          //channel id 1-14: scan a specific channel
}luat_wifiscan_set_info_t;


#define LUAT_MAX_WIFI_BSSID_NUM      40 ///< bssid 的最大数量
#define LUAT_MAX_SSID_HEX_LENGTH     32 ///< SSID 的最大长度

/// @brief wifiscan 扫描结果
typedef struct luat_wifisacn_get_info
{
    uint8_t   bssidNum;                                   /**<wifi 个数*/
    uint8_t   rsvd;
    uint8_t   ssidHexLen[LUAT_MAX_WIFI_BSSID_NUM];        /**<SSID name 的长度*/
    uint8_t   ssidHex[LUAT_MAX_WIFI_BSSID_NUM][LUAT_MAX_SSID_HEX_LENGTH]; /**<SSID name*/
    int8_t    rssi[LUAT_MAX_WIFI_BSSID_NUM];           /**<rssi*/
    uint8_t   channel[LUAT_MAX_WIFI_BSSID_NUM];        /**<record channel index of bssid, 2412MHz ~ 2472MHz correspoding to 1 ~ 13*/ 
    uint8_t   bssid[LUAT_MAX_WIFI_BSSID_NUM][6];       /**<mac address is fixed to 6 digits*/ 
}luat_wifisacn_get_info_t;

/**
 * @brief 获取wifiscan 的信息
 * @param set_info[in] 设置控制wifiscan的参数
 * @param get_info[out] wifiscan 扫描结果 
 * @return int =0成功，其他失败
 */
int32_t luat_get_wifiscan_cell_info(luat_wifiscan_set_info_t * set_info,luat_wifisacn_get_info_t* get_info);

/**
 * @brief 获取wifiscan 的信息
 * @param set_info[in] 设置控制wifiscan的参数 
 * @return int =0成功，其他失败
 */
int luat_wlan_scan_nonblock(luat_wifiscan_set_info_t * set_info);
/** @}*/

#endif
