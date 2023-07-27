#ifndef __ANDLINK_H__
#define __ANDLINK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define PROTOCOL_VERSION    "V3.2" 

#define MAX_SSID_LEN        256
#define MAX_PASSWD_LEN      256
#define MAX_ENCRYPT_LEN     256

#define MAX_FUNCSTR_LEN     256
#define MAX_DEV_ID_LEN      256

// 配网模式
typedef enum{
    NETWOKR_MODE_WIRED = 1,     // 表示有线配网设备
    NETWOKR_MODE_WIFI = 2,      // 表示WIFI配网设备
    NETWOKR_MODE_4G = 3,        // 表示4G配网设备
    NETWOKR_MODE_BT = 4,        // 表示蓝牙配网设备
    NETWOKR_MODE_OTHERS = 5,    // 表示其他配网设备
    NETWOKR_MODE_MAX
}CFG_NET_MODE_e;

// WIFI控制选项(用不到)
typedef enum{
    WIFI_OPT_STA_START = 1,     // 表示打开STA模式
    WIFI_OPT_STA_STOP = 2,      // 表示关闭STA模式
    WIFI_OPT_AP_START = 3,      // 表示打开AP模式
    WIFI_OPT_AP_STOP = 4        // 表示关闭AP模式
}WIFI_CTRL_OPT_e;

// 设备管控操作的响应类型
typedef enum{
    NORSP_MODE = 0,             // 无需响应
    ASYNC_MODE = 1,             // 异步响应,采用devDataReport进行响应
    SYNC_MODE = 2,              // 同步响应,下行管控函数的输出参数进行响应,一般用户设备接入本地网关,不常用
}RESP_MODE_e;

// 设备andlink状态
typedef enum{
    ADL_BOOTSTRAP = 0,          // 0 设备开始注册状态
    ADL_BOOTSTRAP_SUC,          // 1 设备注册成功状态
    ADL_BOOTSTRAP_FAIL,         // 2 设备注册失败状态
    ADL_BOOT,                   // 3 设备开始上线状态
    ADL_BOOT_SUC,               // 4 设备上线成功状态
    ADL_BOOT_FAIL,              // 5 设备上线失败状态
    ADL_ONLINE,                 // 6 设备在线状态
    ADL_RESET,                  // 7 设备复位状态
    ADL_OFFLINE,                // 8 设备离线状态
    ADL_STATE_MAX
} ADL_DEV_STATE_e;

// 设备对外开放查询的属性信息类型
typedef enum{
    ADL_AUTH_CODE,              // 获取一机一密生成的设备工作秘钥;
    ADL_AUTH_DEVICE_ID,         // 获取一机一密生成的设备唯一ID;
    ADL_USER_KEY                // 获取userkey,默认值为16个0
}EXPORT_DEVICE_ATTRS_e;

// WiFi控制信息，用于开启STA模式；特殊说明：当ssid为"CMCC-QLINK",表示切换到一个隐藏的，开放的qlink热点 (用不到)
typedef struct{
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASSWD_LEN];
    char encrypt[MAX_ENCRYPT_LEN];
    int channel;
} wifi_cfg_info_t;

// 设备的属性
typedef struct{
    CFG_NET_MODE_e cfgNetMode;
    char *deviceType;           // 设备或子设备在开发者门户注册的产品类型码 (实际上就是产品ID,对,前面的话可以不看,误导人)
    char *deviceMac;            // 设备MAC，与deviceId一一对应，格式AABBCCDDEEFF，字母需全部大写
    char *andlinkToken;         // 子设备在开发者门户注册的产品类型对应的平台Token
    char *productToken;         // 设备在开发者门户注册的产品类型Token
    char *firmWareVersion;      // 设备固件版本号
    char *softWareVersion;      // 设备软件版本号
    char *cfgPath;              // 供sdk存储配置文件和日志的文件系统路径，此路径需可读可写
    char *extInfo;              // 设备扩展信息(DM信息)，必选，json格式如下
    /*
    {
        "cmei": "", // 设备唯一标识,必选
        "authMode": 0, // 0表示类型认证，1表示设备认证，设备认证时，需使用authId和authKey
        "authId": "", // 用于生成工作秘钥，设备认证必选
        "authKey": "", // 用于生成工作秘钥，设备认证必选
        "sn": "", // 设备SN，必选
        "reserve": "", // 标记字段，可选
        "manuDate": "yyyy-mm", // 设备生产日期，格式为年-月
        "OS": "", // 操作系统
        "chips": [ // 芯片信息,可包含多组
            {
                "type": "", // 芯片类型，如Main/WiFi/Zigbee/BLE等
                "factory": "", // 芯片厂商
                "model": "" // 芯片型号
            }
        ]
    }
    */

}adl_dev_attr_t;

// 下行控制帧结构
typedef struct{
    char function[MAX_FUNCSTR_LEN];     // 下行控制命令类型
    char deviceId[MAX_DEV_ID_LEN];      // 设备ID
    char childDeviceId[MAX_DEV_ID_LEN]; // 子设备ID
    int dataLen;                        // 管控数据长度
    char *data;                         // 管控数据内容
} dn_dev_ctrl_frame_t;

typedef struct{
    // 控制STA (用不到)
    int (*ctrl_wifi_callback)(WIFI_CTRL_OPT_e opt, wifi_cfg_info_t *wificfg, char *outMsg, int msgBufSize);
    // 通知设备状态 (重要)
    int (*set_led_callback)(ADL_DEV_STATE_e state);
    // 下行管控 (重要)
    int (*dn_send_cmd_callback)(RESP_MODE_e mode, dn_dev_ctrl_frame_t *ctrlFrame, char *eventType, char *respData, int respBufSize);
    // 下载并升级版本 (用不到)
    int (*download_upgrade_version_callback)(char *downloadrul, char *filetype, int chkfilesize);
    // 获取设备IP (重要,按照demo的实现即可)
    int (*get_device_ipaddr)(char *ip, char *broadAddr);
    // 复位设备IP (用不到)
    int (*reset_device_Ipaddr)(void);
    // 使能或禁止监听802.11广播帧 (用不到)
    int (*set_listen80211_callback)(int enable);
}adl_dev_callback_t;

// andlink 初始化
int andlink_init(adl_dev_attr_t *devAttr, adl_dev_callback_t *devCbs);

// 设备复位(恢复出厂设置时调用，用于清除配网信息，使设备重新进入配网状态) (用不到)
int devReset(void);

// 消息事件上报(主动消息上报或收到下行控制指令，进行异步响应) 
int devDataReport(char *childDevId, char *eventType, char *data, int dataLen);// eventType: Inform、ParamChange、File、Unbind

// 程序消亡(假设需要卸载sdk时调用，用于释放SDK的相应资源)
int andlink_destroy(void);

// // 写日志功能
// int printLog(int fid, int logLevel, const char * fmt, ...);

// 查询设备特定的信息(获取设备特定信息时调用) (一般用不到)
char *getDeviceInfoStr(EXPORT_DEVICE_ATTRS_e attr);

#ifdef __cplusplus
}
#endif

#endif
