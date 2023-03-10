#include "luat_base.h"
#include "luat_wlan.h"
#include "luat_rtos.h"

#include "common_api.h"
#include "ps_dev_if.h"
#include "cmidev.h"

#define WLAN_SCAN_DONE 1

static CmiDevSetWifiSacnReq wifiscanreq = {
    .maxTimeOut = 10000,
    .round = 1,
    .maxBssidNum = 40,
    .scanTimeOut = 20,
    .wifiPriority = 0,
    .channelRecLen = 280,
    .channelCount = 1
};

static CmiDevSetWifiScanCnf *pWifiScanInfo = NULL;

static int l_wlan_handler(lua_State *L, void* ptr) {
    rtos_msg_t* msg = (rtos_msg_t*)lua_topointer(L, -1);
    int32_t event_id = msg->arg1;
    lua_getglobal(L, "sys_pub");
    switch (event_id)
    {
    case WLAN_SCAN_DONE:
        DBG("wifi scan done");
        lua_pushstring(L, "WLAN_SCAN_DONE");
        lua_call(L, 1, 0);
        break;
    default:
        DBG("unkown event %d", event_id);
        break;
    }
    return 0;
}

int luat_wlan_init(luat_wlan_config_t *conf){
    DBG("wifi support scan only");
    return 0;
}

void luat_wlan_scan_ec618(UINT16 paramSize, void *pParam)
{
	devSetWIFISCAN(PS_DIAL_REQ_HANDLER, pParam);
}

int luat_wlan_scan(void){
	cmsNonBlockApiCall(luat_wlan_scan_ec618, sizeof(wifiscanreq), &wifiscanreq);
    return 0;
}

void luat_wlan_done_callback_ec618(void *param)
{
    rtos_msg_t msg = {0};
    msg.handler = l_wlan_handler;
    msg.arg1 = WLAN_SCAN_DONE;
    if (pWifiScanInfo == NULL)
        pWifiScanInfo = (CmiDevSetWifiScanCnf *)malloc(sizeof(CmiDevSetWifiScanCnf));
    if (pWifiScanInfo) {
        memset(pWifiScanInfo, 0, sizeof(CmiDevSetWifiScanCnf));
        memcpy(pWifiScanInfo, param, sizeof(CmiDevSetWifiScanCnf));
        luat_msgbus_put(&msg, 0);
    }
}

int luat_wlan_scan_get_result(luat_wlan_scan_result_t *results, int ap_limit){
    if (pWifiScanInfo == NULL) {
        return 0;
    }
    if (ap_limit > pWifiScanInfo->bssidNum){
        ap_limit = pWifiScanInfo->bssidNum;
    }
    for (size_t i = 0; i < ap_limit; i++){
        memcpy(results[i].ssid, pWifiScanInfo->ssidHex[i], pWifiScanInfo->ssidHexLen[i]);
        memcpy(results[i].bssid, pWifiScanInfo->bssid[i], 6);
        results[i].rssi = pWifiScanInfo->rssi[i];
    }
    return ap_limit;
}
