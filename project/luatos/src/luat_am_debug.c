#include "bsp_common.h"
#include "common_api.h"
#include "luat_rtos.h"
#include "luat_gpio.h"
#include "luat_fota.h"

#if 1

#define LUAT_LOG_TAG "sc"
#include "luat_log.h"

// 接管usb虚拟log口的输入数据, 尝试实现免boot刷脚本
/*
在csdk中, 已内置如下命令
uint8_t fast_ack_cmd[4] = {0x7e, 0x00, 0x00, 0x7e};
uint8_t fast_reboot_cmd[4] = {0x7e, 0x00, 0x01, 0x7e};

按协议文档, 这里先临时定义3个刷机相关的命令
1. 初始化数据传输 {0x7e, 0x00, 0x30, 0x7e}
2. 块数据传输     {0x7e, 0x00, 0x31, 0x7e}, 后面接4k的fota数据
3. 结束数据传输   {0x7e, 0x00, 0x32, 0x7e}

传输结束后, 需要发送重启命令
*/
static const uint8_t cmd_fota_init[] = {0x7e, 0x00, 0x30, 0x7e};
static const uint8_t cmd_fota_data[] = {0x7e, 0x00, 0x31, 0x7e}; // TODO 0x00应该是字节数?
static const uint8_t cmd_fota_end[]  = {0x7e, 0x00, 0x32, 0x7e};

void am_debug(uint8_t *data, uint32_t len) {
    int ret = 0;
    // LLOGD(">>>>>>>>>>> %d", len);
    if (len >= 4) {
        // 初始化命令
        if (!memcmp(data, cmd_fota_init, 4)) {
            luat_fota_init(0, 0, NULL, NULL, 0);
            LLOGD(">>>> FOTA INIT <<<<");
        }
        // 数据命令,貌似cmd_fota_data[1]应该是长度数据?
        else if (!memcmp(data, cmd_fota_data, 4)) {
            if (len == 256 + 4) { // soc log单次最多收512字节
                ret = luat_fota_write(data + 4, 256);
                if (ret < 0) {
                    LLOGD(">>>> FOTA WRITE ERR <<<<");
                }
                else {
                    LLOGD(">>>> FOTA WRITE OK <<<<");
                }
            }
        }
        // 结束命令, 后续应该会有重启命令发过来
        else if (!memcmp(data, cmd_fota_end, 4)) {
            if (0 == luat_fota_done()) {
                LLOGD(">>>> FOTA DONE OK <<<<");
            }
            else {
                LLOGD(">>>> FOTA DONE ERR <<<<");
            }
        }
    }
}

#endif
