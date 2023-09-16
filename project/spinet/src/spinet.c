

#include <string.h>
#include <sys/queue.h>
#include "common_api.h"
#include "luat_rtos.h"
#include "luat_debug.h"
#include "luat_gpio.h"
#include "luat_mcu.h"
#include "luat_pm.h"
#include "platform_define.h"

#include "bsp_spi.h"
#include "slpman.h"
#include "ostask.h"
#include "plat_config.h"
#include "osadlfcmem.h"
#include "pad.h"
#include "cmips.h"
#include "tcpip.h"
#include "psifapi.h"
#include "ps_lib_api.h"
#include "ps_event_callback.h"
#include "networkmgr.h"
#include "lwip/netif.h"

#include "eth.h"

#define MAX_TRANSFER_SIZE                (4 * 1024)

#define MAX_TRSNSFER_TIMEOUT             5

#define ARM_SPI_FRAME_FROMAT             ARM_SPI_CPOL1_CPHA1

#define DIRE_GPIO_PIN          HAL_GPIO_1
#define HRDY_GPIO_PIN          HAL_GPIO_27
#define DRDY_GPIO_PIN          HAL_GPIO_22
#define NOT_GPIO_PIN           HAL_GPIO_24

#define MSG_FLAG                 (0xF9)

/** \brief DEVICE
 */
#define MSG_TYPE_ACK             (0x00)
#define MSG_TYPE_CMD_RSP         (0x01)
#define MSG_TYPE_DEVICE_DATA     (0x02)

/** \brief HOST
 */
#define MSG_TYPE_GET_DATA    (0x08)
#define MSG_TYPE_GET_LEN     (0x09)
#define MSG_TYPE_HOST_DATA   (0x0A)
#define MSG_TYPE_AT_CMD      (0x0B)

enum
{
    MSG_ERROR_NONE,
    MSG_ERROR_HEADER,
    MSG_ERROR_SEQ,
    MSG_ERROR_TYPE,
    MSG_ERROR_LEN,
    MSG_ERROR_CRC,
    MSG_ERROR_OTHER = 0xFF,
};

typedef struct
{
    uint8_t flag;
    uint8_t type : 4;
    uint8_t seq : 4;
    uint8_t len_low;
    uint8_t len_heigh;
} msgHdr_t;

enum
{
    PRIV_MSG_HRDY,
    PRIV_MSG_NEW_DATA,
};

typedef struct privData privData_t;
typedef STAILQ_ENTRY(privData) privDataIter_t;
typedef STAILQ_HEAD(, privData) privDataHead_t;
struct privData
{
    uint8_t type;
    uint32_t len;
    void *data;
    privDataIter_t iter;
};

/** \brief driver instance declare */
extern ARM_DRIVER_SPI Driver_SPI0;

extern uint16_t crc16ccitt(uint16_t crc, uint8_t *bytes, int start, int len);

extern void netif_dump_dl_packet(u8_t *data, u16_t len, u8_t type);

static ARM_DRIVER_SPI *spiSlaveDrv = &CREATE_SYMBOL(Driver_SPI, 0);

static luat_rtos_task_handle main_msgq;
static luat_rtos_semaphore_t done_sema;
static luat_rtos_semaphore_t hrdy_high_sema;

static privDataHead_t dq;

static uint8_t hrdy_debounce_edge;

static int seq = 0;

static bool spi_error = true;

static int default_cid = 1;

const uint8_t host_mac[ETH_HWADDR_LEN] = {0xf0, 0x4b, 0xb3, 0xb9, 0xeb, 0xe5};
const uint8_t dev_mac[ETH_HWADDR_LEN] = {0xfa, 0x32, 0x47, 0x15, 0xe1, 0x88};

void RetCheck(int32_t cond, char * v1)
{
    if (cond == ARM_DRIVER_OK) {
        // LUAT_DEBUG_PRINT("SPI %s OK", (v1));
    } else {
        LUAT_DEBUG_PRINT("SPI %s Failed !!, %d", (v1), cond);;
    }
}

/** \brief transaction buffer */
static __ALIGNED(4) uint8_t slave_buffer_out[MAX_TRANSFER_SIZE];
static __ALIGNED(4) uint8_t slave_buffer_in[MAX_TRANSFER_SIZE];

static inline uint8_t nextSeq()
{
    if (!(seq < 0x0F))
    {
        seq = 0;
    }
    return (seq++) % 0x0F;
}

static inline const char *msgCode2Str(uint8_t code)
{
    const char *codeStr[] = {
        "MSG_ERROR_NONE",
        "MSG_ERROR_HEADER",
        "MSG_ERROR_SEQ",
        "MSG_ERROR_TYPE",
        "MSG_ERROR_LEN",
        "MSG_ERROR_CRC",
    };

    return code == MSG_ERROR_OTHER ? "MSG_ERROR_OTHER" : codeStr[code];
}

static inline const char *msgType2Str(uint8_t code)
{
    const char *typeStr[] = {
        "MSG_TYPE_ACK",
        "MSG_TYPE_CMD_RSP",
        "MSG_TYPE_DEVICE_DATA",
        "HOLE",
        "HOLE",
        "HOLE",
        "HOLE",
        "HOLE",
        "MSG_TYPE_GET_DATA",
        "MSG_TYPE_GET_LEN",
        "MSG_TYPE_HOST_DATA",
        "MSG_TYPE_AT_CMD",
    };

    return typeStr[code];
}

static inline void initMsgHdr(msgHdr_t *hdr, uint8_t type, uint16_t len)
{
    hdr->flag      = MSG_FLAG;
    hdr->type      = type & 0x0F;
    hdr->seq       = nextSeq() & 0x0F;
    hdr->len_low   = len & 0x00FF;
    hdr->len_heigh = (len >> 8) & 0x00FF;
}

static inline uint32_t initMsgBuf(void *buf, uint16_t buf_len, uint8_t type, void *head, uint16_t head_len, void *data, uint16_t data_len)
{
    LUAT_DEBUG_PRINT("type %s, head_len %d, data_len %d", msgType2Str(type), head_len, data_len);

    uint32_t length = sizeof(msgHdr_t) + head_len + data_len + sizeof(uint16_t);
    EC_ASSERT(buf_len >= length, buf_len, head_len, data_len);

    msgHdr_t *hdr = (msgHdr_t *)buf;
    initMsgHdr(hdr, type, head_len + data_len);

    if (head)
    {
        memcpy(buf + sizeof(msgHdr_t), head, head_len);
    }

    if (data)
    {
        memcpy(buf + sizeof(msgHdr_t) + head_len, data, data_len);
    }

    uint16_t crc = 0xFFFF;
    crc = crc16ccitt(crc, buf, 0, sizeof(msgHdr_t) + head_len + data_len);
    memcpy(buf + sizeof(msgHdr_t) + head_len + data_len, &crc, sizeof(crc));

    return length;
}

static inline uint32_t initAckMsg(void *buf, uint16_t buf_len, uint8_t code)
{
    uint8_t rsp[2] = {code, 0x00};
    return initMsgBuf(buf, buf_len, MSG_TYPE_ACK, NULL, 0, rsp, sizeof(rsp));
}

static void sendPrivMsg(uint8_t type, uint32_t tick)
{
    int status;
    if (type == PRIV_MSG_HRDY)
    {
        status = luat_rtos_message_send(main_msgq, type, (void *)tick);
    }
    else
    {
        status = luat_rtos_message_send(main_msgq, type, 0);
    }
    EC_ASSERT(status == 0, 0, 0, 0);
}

static void newMsg(uint8_t type, void *data, uint32_t len)
{
    if (type == MSG_TYPE_DEVICE_DATA)
    {
        netif_dump_dl_packet(data, len, 5 /*LWIP_NETIF_TYPE_WAN_INTERNET*/);
    }

    privData_t *pd = malloc(sizeof(privData_t));
    if (pd == NULL)
    {
        LUAT_DEBUG_PRINT("Discard new msg");
        free(data);
        return;
    }

    pd->type = type;
    pd->data = data;
    pd->len = len;

    uint32_t cr = luat_rtos_entry_critical();
    bool empty = STAILQ_EMPTY(&dq);
    STAILQ_INSERT_TAIL(&dq, pd, iter);
    luat_rtos_exit_critical(cr);

    LUAT_DEBUG_PRINT("dirty");

    if (empty)
    {
        sendPrivMsg(PRIV_MSG_NEW_DATA, luat_mcu_ticks());
    }
}

static void GPIO_ISR(int pin, void *arg)
{
    //Save current irq mask and diable whole port interrupts to get rid of interrupt overflow
    int expectLevel = (hrdy_debounce_edge == LUAT_GPIO_LOW_IRQ || hrdy_debounce_edge == LUAT_GPIO_FALLING_IRQ) ? 0 : 1;
    int level = luat_gpio_get(HRDY_GPIO_PIN);
    if (level == expectLevel)
    {
        if (level == 0)
        {
            sendPrivMsg(PRIV_MSG_HRDY, luat_mcu_ticks());
        }
        else
        {
            luat_rtos_semaphore_release(hrdy_high_sema);
        }

        luat_gpio_ctrl(HRDY_GPIO_PIN, LUAT_GPIO_CMD_SET_IRQ_MODE, LUAT_GPIO_NO_IRQ);
    }
}

static inline void drdy(bool isHigh)
{
    if (isHigh)
    {
        luat_gpio_set(DRDY_GPIO_PIN, 1);
    }
    else
    {
        luat_gpio_set(DRDY_GPIO_PIN, 0);
    }
}

static inline void notify(uint32_t us)
{
    luat_gpio_set(NOT_GPIO_PIN, 1);
    delay_us(us);
    luat_gpio_set(NOT_GPIO_PIN, 0);
}

static void initGpio()
{
    slpManAONIOPowerOn();
    slpManNormalIOVoltSet(IOVOLT_1_80V);
    slpManAONIOVoltSet(IOVOLT_1_80V);

    // DRDY
    luat_gpio_cfg_t gpio_cfg;
    luat_gpio_set_default_cfg(&gpio_cfg);

    gpio_cfg.pin = DRDY_GPIO_PIN;
    gpio_cfg.mode = LUAT_GPIO_OUTPUT;
    luat_gpio_open(&gpio_cfg);

    // HRDY
    luat_gpio_set_default_cfg(&gpio_cfg);

    gpio_cfg.pin = HRDY_GPIO_PIN;
    gpio_cfg.mode = LUAT_GPIO_INPUT;
    luat_gpio_open(&gpio_cfg);

    // NOT
    luat_gpio_set_default_cfg(&gpio_cfg);

    gpio_cfg.pin = NOT_GPIO_PIN;
    gpio_cfg.mode = LUAT_GPIO_OUTPUT;
    luat_gpio_open(&gpio_cfg);

    // DIRE
    luat_gpio_set_default_cfg(&gpio_cfg);

    gpio_cfg.pin = DIRE_GPIO_PIN;
    gpio_cfg.mode = LUAT_GPIO_INPUT;
    luat_gpio_open(&gpio_cfg);
}

/**
  \fn          void SPI_Callback()
  \brief       SPI callback
  \return
*/
static bool overflow = false;
static void SPI_Callback(uint32_t event)
{
    luat_rtos_semaphore_release(done_sema);
}

static void spiOff()
{
    int ret = spiSlaveDrv->PowerControl(ARM_POWER_OFF);
    RetCheck(ret, "Power off slave");
}

static void spiOn()
{
    // Initialize
    int ret = spiSlaveDrv->Initialize(SPI_Callback);
    RetCheck(ret, "Initialize slave");

    // Power on
    ret = spiSlaveDrv->PowerControl(ARM_POWER_FULL);
    RetCheck(ret, "Power on slave");

    // Configure slave spi bus
    ret = spiSlaveDrv->Control(ARM_SPI_MODE_SLAVE | ARM_SPI_FRAME_FROMAT | ARM_SPI_DATA_BITS(8) |
                                ARM_SPI_MSB_LSB, 0);
    RetCheck(ret, "Control");

    // ret = spiSlaveDrv->Control(ARM_SPI_SET_BUS_SPEED, 10000000);
    // RetCheck(ret, "Bus speed");
}

static uint32_t spiTransfer(void *out, void *in, uint32_t len)
{
    bool dir_out = false;
    if (out && !in)
    {
        dir_out = true;
    }
    else if (!out && !in)
    {
        return 0;
    }

    if (!out)
    {
        out = slave_buffer_out;
    }

    if (!in)
    {
        in = slave_buffer_in;
    }

    memset(in, 0, len);

    __DSB();
    __DMB();

    luat_rtos_semaphore_take(done_sema, 0);
    luat_rtos_semaphore_take(hrdy_high_sema, 0);

    uint32_t cr = luat_rtos_entry_critical();

    if (luat_gpio_get(HRDY_GPIO_PIN) == 1)
    {
        luat_rtos_exit_critical(cr);
        LUAT_DEBUG_PRINT("hrdy high");
        return 0;
    }

    hrdy_debounce_edge = LUAT_GPIO_HIGH_IRQ;
    luat_gpio_cfg_t gpio_cfg;
    luat_gpio_set_default_cfg(&gpio_cfg);

    gpio_cfg.pin = HRDY_GPIO_PIN;
    gpio_cfg.mode = LUAT_GPIO_IRQ;
    gpio_cfg.irq_type = hrdy_debounce_edge;
    gpio_cfg.irq_cb = GPIO_ISR;
    luat_gpio_open(&gpio_cfg);

    luat_rtos_exit_critical(cr);

    spiOn();

    int ret = spiSlaveDrv->Transfer(out, in, dir_out ? len : MAX_TRANSFER_SIZE);
    RetCheck(ret, "DMA INIT");

    if (ret != ARM_DRIVER_OK)
    {
        spiOff();
        spiOn();

        ret = spiSlaveDrv->Transfer(out, in, dir_out ? len : MAX_TRANSFER_SIZE);
        RetCheck(ret, "DMA INIT2");
    }

    drdy(true);

    int trans = 0;
    bool timeout = false; //(ret != ARM_DRIVER_OK);
    uint32_t preWaitTime = luat_mcu_ticks();
    if (luat_rtos_semaphore_take(hrdy_high_sema, MAX_TRSNSFER_TIMEOUT) == osOK)
    {
        LUAT_DEBUG_PRINT("Acquire hrdy_high_sema");

        uint32_t passedTime = (luat_mcu_ticks() - preWaitTime) > 0 ? (luat_mcu_ticks() - preWaitTime) : (0xFFFFFFFF - preWaitTime + luat_mcu_ticks());

        if (dir_out)
        {
            int wait = MAX_TRSNSFER_TIMEOUT - passedTime;
            if (luat_rtos_semaphore_take(done_sema, wait < 0 ? 0 : wait) != osOK)
            {
                LUAT_DEBUG_PRINT("Acquire done_sema timeout");
            }
            else
            {
                trans = len;
            }
        }
    }
    else
    {
        LUAT_DEBUG_PRINT("Acquire hrdy_high_sema timeout");
        timeout = true;
    }

    drdy(false);

    ret = spiSlaveDrv->Control(ARM_SPI_ABORT_TRANSFER, 0);
    RetCheck(ret, "Abort");

    if (!dir_out && !timeout)
    {
        // vTaskDelay(1);

        trans = SPI_RX_FIFO_TRIG_LVL;

        trans += spiSlaveDrv->GetDataCount();
        LUAT_DEBUG_PRINT("Trans count %d", trans);

        trans += spiSlaveDrv->ReadFIFO(&((uint8_t *)in)[trans]);
        LUAT_DEBUG_PRINT("Trans fifo %d", trans);

        if (trans < len)
        {
            timeout = true;
        }

        if (overflow)
        {
            overflow = false;
            LUAT_DEBUG_PRINT("Overflow");
        }

        if (!timeout)
        {
            memcpy(&((char *)in)[trans], in, SPI_RX_FIFO_TRIG_LVL);
        }
    }

    memset(out, 0, len);

    if (timeout)
    {
        LUAT_DEBUG_PRINT("Timeout");

        trans = 0;
    }

    spiOff();

    spi_error = (trans == 0);

    return trans;
}

static uint32_t sendAckMsg(void *buf, uint16_t buf_len, uint8_t code)
{
    if (code != MSG_ERROR_NONE)
    {
        LUAT_DEBUG_PRINT("rsp code %s", msgCode2Str(code));
    }

    return 1;
}

static uint32_t sendDataMsg(void *buf, uint16_t buf_len, uint8_t type, void *msg, uint16_t msg_len)
{
    uint32_t len = initMsgBuf(slave_buffer_out, MAX_TRANSFER_SIZE, type, NULL, 0, msg, msg_len);
    return spiTransfer(slave_buffer_out, NULL, len);
}

static uint32_t sendEthDataMsg(void *buf, uint16_t buf_len, struct eth_hdr *hdr, void *msg, uint16_t msg_len)
{
    uint32_t len = initMsgBuf(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_TYPE_DEVICE_DATA, hdr, sizeof(struct eth_hdr), msg, msg_len);
    return spiTransfer(slave_buffer_out, NULL, len);
}

static uint32_t sendLenMsg(uint16_t len)
{
    len = initMsgBuf(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_TYPE_GET_LEN, NULL, 0, &len, sizeof(uint16_t));
    return spiTransfer(slave_buffer_out, slave_buffer_in, len);
}

static void toPs(void *ctx)
{
    extern BOOL PsifRawUlOutput(UINT8, UINT8 *, UINT16);

    ethPacket_t *pkt = (ethPacket_t *)((void **)ctx)[0];
    uint32_t len = (uint32_t)((void **)ctx)[1];

    struct netif *netif = netif_find_by_ip4_cid(default_cid);

    if (netif == NULL)
    {
        return;
    }

    if (isARPPackage(pkt, len))
    {
        LUAT_DEBUG_PRINT("arp");
        netif_dump_dl_packet(pkt->data, len - sizeof(struct eth_hdr), 5);
        struct etharp_hdr *reply = ARP_reply(pkt, dev_mac);
        if (reply)
        {
            LUAT_DEBUG_PRINT("arp reply");
            void *arp = malloc(sizeof(struct etharp_hdr));
            if (arp != NULL)
            {
                memcpy(arp, reply, sizeof(struct etharp_hdr));
                newMsg(MSG_TYPE_DEVICE_DATA, arp, sizeof(struct etharp_hdr));
            }
        }
    }
    else if (isNeighborSolicitationPackage((ethPacket_t *)pkt, len))
    {
        LUAT_DEBUG_PRINT("ns");
    }
    else
    {
        LUAT_DEBUG_PRINT("ip");
        PsifRawUlOutput(default_cid, pkt->data, len - sizeof(struct eth_hdr));
    }

    free(ctx);
    free(pkt);
}

static void sendIp2Ps(uint8_t *pkt, uint32_t len)
{
    void **param = (void **)malloc(sizeof(void *) * 2);
    param[0] = (void *)pkt;
    param[1] = (void *)len;
    tcpip_callback(toPs, param);
}

u8_t ip4_filter(struct pbuf *p, struct netif *inp)
{
    LUAT_DEBUG_PRINT("inpkt");

    uint16_t offset = 0;
    struct pbuf *q = NULL;

    do
    {
        void *data = malloc(p->tot_len);
        if (data == NULL)
        {
            break;
        }

        for (q = p; q != NULL; q = q->next)
        {
            memcpy(&data[offset], q->payload, q->len);
            offset += q->len;
        }

        newMsg(MSG_TYPE_DEVICE_DATA, data, p->tot_len);
    } while (0);

    pbuf_free(p);

    return 1 /* eaten */;
}

static void SPI_ExampleEntry(void *arg)
{
    uint8_t state = 0;
    uint16_t crc = 0;
    uint16_t dataCrc = 0;
    uint16_t dataLen = 0;
    uint32_t devDataLen = 0;
    msgHdr_t *pHdr = NULL;

    char *at = NULL;
    uint32_t transSize = 0;
    privData_t *pData = NULL;
    uint8_t *pkt = NULL;
    bool queueDirty = false;
    bool dnot = true;

    uint32_t last_tick = 0;

    uint8_t *in_buf = NULL;

    memset(slave_buffer_out, 0, sizeof(slave_buffer_out));
    memset(slave_buffer_in, 0, sizeof(slave_buffer_in));

    luat_pm_set_power_mode(LUAT_PM_SLEEP_MODE_IDLE, 0);

    initGpio();

    LUAT_DEBUG_PRINT("Wait for master to trigger transfer");

    while (1)
    {
        uint32_t cr = luat_rtos_entry_critical();
        queueDirty = (STAILQ_FIRST(&dq) != NULL);
        luat_rtos_exit_critical(cr);

        last_tick = luat_mcu_ticks();

        int l;
        if ((l = luat_gpio_get(HRDY_GPIO_PIN)) == 1 || spi_error)
        {
            LUAT_DEBUG_PRINT("low", spi_error);

            if (spi_error)
            {
                pData = NULL;
            }

            bool skip = false;
            int status = 0;
            uint32_t id;
            uint32_t tick;

            do
            {
                cr = luat_rtos_entry_critical();

                if (luat_gpio_get(HRDY_GPIO_PIN) == 0 && !spi_error)
                {
                    luat_rtos_exit_critical(cr);
                    skip = true;
                    LUAT_DEBUG_PRINT("hrdy low");
                    break;
                }

                hrdy_debounce_edge = spi_error ? LUAT_GPIO_FALLING_IRQ : LUAT_GPIO_LOW_IRQ;
                luat_gpio_cfg_t gpio_cfg;
                luat_gpio_set_default_cfg(&gpio_cfg);

                gpio_cfg.pin = HRDY_GPIO_PIN;
                gpio_cfg.mode = LUAT_GPIO_IRQ;
                gpio_cfg.irq_type = hrdy_debounce_edge;
                gpio_cfg.irq_cb = GPIO_ISR;
                luat_gpio_open(&gpio_cfg);

                luat_rtos_exit_critical(cr);

                if (queueDirty && pData == NULL)
                {
                    if (dnot)
                    {
                        notify(20);
                        dnot = false;
                    }
                }

                spi_error = false;

                LUAT_DEBUG_PRINT("Wait main_msgq");
                status = luat_rtos_message_recv(&main_msgq, &id, &tick, LUAT_WAIT_FOREVER);
            } while (0);

            if (!skip)
            {
                if (status != 0)
                {
                    EC_ASSERT(pData != NULL, 0, 0, 0);
                }
                else if (id == PRIV_MSG_NEW_DATA)
                {
                    LUAT_DEBUG_PRINT("NEW DATA");
                    continue;
                }
                else
                {
                    LUAT_DEBUG_PRINT("HRDY");

                    LUAT_DEBUG_PRINT("last %x, curr %x", last_tick, tick);

                    if (luat_gpio_get(HRDY_GPIO_PIN) == 1
                        || tick < last_tick)
                    {
                        continue;
                    }
                }
            }
        }
        else
        {
            LUAT_DEBUG_PRINT("hrdy low2");
        }

        // GPIO_pinWrite(NOT_GPIO_INSTANCE, 1 << NOT_GPIO_PIN, 0);
        dnot = true;

        // Recv head
        if (luat_gpio_get(DIRE_GPIO_PIN) == 1)
        {
            transSize = spiTransfer(slave_buffer_out, slave_buffer_in, 9);
            if (transSize == 0)
            {
                continue;
            }
        }
        else
        {
            pData = STAILQ_FIRST(&dq);
            if (pData != NULL)
            {
                if (pData->type == MSG_TYPE_DEVICE_DATA)
                {
                    struct eth_hdr eth_hdr;
                    memcpy(&eth_hdr.dest, host_mac, ETH_HWADDR_LEN);
                    memcpy(&eth_hdr.src, dev_mac, ETH_HWADDR_LEN);
                    uint8_t *ipdata = (uint8_t *)pData->data;
                    if ((ipdata[0] & 0xF0) == 0x40)
                        eth_hdr.type = __htons(ETHTYPE_IP);
                    else if ((ipdata[0] & 0xF0) == 0x60)
                        eth_hdr.type = __htons(ETHTYPE_IPV6);
                    else if (ipdata[0] == 0x00 && ipdata[1] == 0x01 &&
                            ipdata[2] == 0x08 && ipdata[3] == 0x00)
                        eth_hdr.type = __htons(ETHTYPE_ARP);
                    else
                        EC_ASSERT(0, 0, 0, 0);

                    sendEthDataMsg(slave_buffer_out, MAX_TRANSFER_SIZE, &eth_hdr, pData->data, pData->len);
                }

                cr = luat_rtos_entry_critical();
                STAILQ_REMOVE_HEAD(&dq, iter);
                luat_rtos_exit_critical(cr);
                free(pData->data);
                free(pData);

                pData = NULL; //notify
            }
            else
            {
                sendDataMsg(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_TYPE_DEVICE_DATA, NULL, 0);
            }

            continue;
        }

        in_buf = &slave_buffer_in[SPI_RX_FIFO_TRIG_LVL];

DECODE:
        switch (state)
        {
            // Wait flag
            case 0:
            {
                if (in_buf[0] == MSG_FLAG)
                {
                    pHdr = (msgHdr_t *)&in_buf[0];

                    if (pHdr->type != MSG_TYPE_GET_DATA)
                    {
                        dataLen = (((uint16_t)pHdr->len_heigh) << 8) | pHdr->len_low;
                        if ((sizeof(msgHdr_t) + dataLen + sizeof(uint16_t)) > transSize)
                        {
                            sendAckMsg(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_ERROR_LEN);
                            break;
                        }
                    }

                    state = 1;
                    goto DECODE;
                }
                else
                {
                    sendAckMsg(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_ERROR_HEADER);
                    break;
                }
            }

            // Parse packet
            case 1:
            {
                state = 0;

                crc = 0xFFFF;
                crc = crc16ccitt(crc, in_buf, 0, sizeof(msgHdr_t) + dataLen);
                dataCrc = in_buf[sizeof(msgHdr_t) + dataLen];
                dataCrc |= (in_buf[sizeof(msgHdr_t) + dataLen + 1] << 8);
                if (crc != dataCrc)
                {
                    LUAT_DEBUG_PRINT("crc %x, dataCrc %x", crc, dataCrc);

                    sendAckMsg(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_ERROR_CRC);
                    break;
                }

                switch (pHdr->type)
                {
                    case MSG_TYPE_HOST_DATA:
                    {
                        pkt = malloc(dataLen);
                        memcpy(pkt, &in_buf[sizeof(msgHdr_t)], dataLen);
                        sendIp2Ps(pkt, dataLen);
                        sendAckMsg(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_ERROR_NONE);
                        break;
                    }

                    default:
                    {
                        sendAckMsg(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_ERROR_TYPE);
                        break;
                    }
                }

                break;
            }

            default:
            {
                break;
            }
        }
    }
}

void spiTaskInit(void)
{
    STAILQ_INIT(&dq);

    luat_rtos_semaphore_create(&done_sema, 1);
    luat_rtos_semaphore_take(done_sema, 0);

    luat_rtos_semaphore_create(&hrdy_high_sema, 1);
    luat_rtos_semaphore_take(hrdy_high_sema, 0);

    luat_rtos_task_create(&main_msgq, 4096, 40, "spi", SPI_ExampleEntry, NULL, 256);
}


INIT_TASK_EXPORT(spiTaskInit, "0");
