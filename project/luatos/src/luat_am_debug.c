#include "bsp_common.h"
#include "common_api.h"
#include "luat_rtos.h"
#include "luat_gpio.h"
#include "luat_fota.h"
#include "luat_malloc.h"
#include "cms_def.h"

#if 1

#define LUAT_LOG_TAG "sc"
#include "luat_log.h"

#define CMD_BUFF_SIZE (1024)

// 接管usb虚拟log口的输入数据, 实现免boot刷脚本
// 需要 LuaTools 2.1.94或以上

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

// 1. 初始化命令, 无后续数据
static const uint8_t cmd_fota_init[] = {0x7e, 0x00, 0x30, 0x7e};
// 2. 数据传输命令, 后续接1字节的无符号长度, 然后是实际数据, 通常为128字节
static const uint8_t cmd_fota_data[] = {0x7e, 0x00, 0x31, 0x7e}; 
// 3. 结束命令, 无后续数据
static const uint8_t cmd_fota_end[]  = {0x7e, 0x00, 0x32, 0x7e};
// 注意: data的最后一个包和init,会有额外处理, 耗时会长一些

// 命令的响应均为 >>>>$X FOTA $Y<<<<
// 其中: $X 是 OK 代表成功, 其他值代表失败
//       $Y 对应 INIT WRITE DONE

// 处理USB分包, 尤其是cmd_fota_data的分包处理
static uint8_t* tmpbuff;
static size_t buff_size; // 保存tmpbuff已写入的数据量

// 直接往soc log虚拟串口写入数据, 绕过DBG的处理逻辑
extern int32_t am_usb_direct_output(uint8_t atCid, uint8_t* atStr, uint16_t atStrLen);

// 方便输出响应
static void usb_output_str(const char* data) {
    am_usb_direct_output(CMS_CHAN_USB, data, strlen(data));
}
// static void usb_output_raw(const char* data, size_t len) {
//     am_usb_direct_output(CMS_CHAN_USB, data, len);
// }

// 复写csdk的同名函数, 原函数为weak属性, 可覆盖
void am_debug(uint8_t *data, uint32_t len) {
    int ret = 0;
    // LLOGD(">>>>>>>>>>> %d", len);
next:
    if (tmpbuff == NULL) { // 需要1k的缓冲区,实际用到200字节不到.
        tmpbuff = luat_heap_malloc(CMD_BUFF_SIZE);
    }
    if (tmpbuff == NULL) { // 内存炸了?
        LLOGW("sorry, out of memory");
        return;
    }
    if (buff_size + len > CMD_BUFF_SIZE) {
        LLOGW("too many data, drop old data");
        buff_size = 0;
    }
    memcpy(tmpbuff + buff_size, data, len);
    buff_size += len;
    if (buff_size < 4)
        return; // 等待更多数据
    len = 0;
    data = NULL; // 防止误用

    // 开始判断命令了

    // 1.初始化命令
    if (!memcmp(tmpbuff, cmd_fota_init, 4)) {
        luat_fota_init(0, 0, NULL, NULL, 0);
        buff_size -= 4;
        if (buff_size > 0) {
            memmove(tmpbuff, tmpbuff + 4, buff_size);
        }
        // FOTA INIT 总是成功. TODO 好像也不是吧??
        usb_output_str(">>>>OK FOTA INIT<<<<");
    }
    // 2.数据命令
    else if (!memcmp(tmpbuff, cmd_fota_data, 4)) {
        if (buff_size == 4) {
            // 数据还不够,等下一批
            return;
        }
        size_t rlen = tmpbuff[4]; // 每个包最大256字节
        if (rlen > buff_size - 5) {
            return; // 等待下一个包
        }
        //LLOGD("fota write %d", rlen);
        ret = luat_fota_write(tmpbuff + 5, rlen);
        if (ret < 0) {
            usb_output_str(">>>>ERR FOTA WRITE<<<<");
            buff_size = 0;
            return;
        }
        else {
            usb_output_str(">>>>OK FOTA WRITE<<<<");
        }
        buff_size -= rlen + 5;
        if (buff_size > 0) {
            memmove(tmpbuff, tmpbuff + 5, buff_size);
        }
    }
    // 3. 结束命令, 后续应该会有重启命令发过来
    else if (!memcmp(tmpbuff, cmd_fota_end, 4)) {
        ret = luat_fota_end(0);
        if (0 == ret) {
            usb_output_str(">>>>OK FOTA DONE<<<<");
        }
        else {
            usb_output_str(">>>>ERR FOTA DONE<<<<");
        }
        if (tmpbuff != NULL) {
            luat_heap_free(tmpbuff);
            tmpbuff = NULL;
            buff_size = 0;
        }
    }
    else {
        LLOGD(">>>>ERR FOTA<<<<");
        return;
    }
    if (buff_size >= 4) {
        goto next;
    }
}

#endif
