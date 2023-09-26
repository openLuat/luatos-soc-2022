
#include <string.h>
#include <sys/queue.h>
#include "common_api.h"
#include "luat_rtos.h"
#include "luat_debug.h"
#include "luat_gpio.h"
#include "luat_mcu.h"
#include "luat_pm.h"
#include "luat_mobile.h"
#include "platform_define.h"

#include <string.h>
#include <sys/queue.h>
#include "bsp.h"
#include "bsp_custom.h"
#include "bsp_spi.h"
#include "ostask.h"
#include "plat_config.h"
#include "FreeRTOS.h"
#include "slpman.h"
#include "queue.h"
#include "task.h"
#include "pspdu.h"
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

/** \brief transfer data size in one transaction
    \note size shall be greater than DMA request level(8)
 */
#define MAX_TRANSFER_SIZE                (4 * 1024)

#define MAX_TRSNSFER_TIMEOUT             8

#define MAX_GPIO_DEBOUNCE_US             1

#define MAX_RETRY_TIMEOUT                50

#define ARM_SPI_FRAME_FROMAT             ARM_SPI_CPOL1_CPHA1

/** \brief GPIO01
 */
#define DIRE_GPIO_INSTANCE     (0)
#define DIRE_GPIO_PIN          (1)
#define DIRE_PAD_INDEX         (16)
#define DIRE_PAD_ALT_FUNC      (PAD_MUX_ALT0)

/** \brief GPIO27
 */
#define HRDY_GPIO_INSTANCE     (1)
#define HRDY_GPIO_PIN          (11)
#define HRDY_PAD_INDEX         (47)
#define HRDY_PAD_ALT_FUNC      (PAD_MUX_ALT0)

/** \brief GPIO22
 */
#define DRDY_GPIO_INSTANCE     (1)
#define DRDY_GPIO_PIN          (6)
#define DRDY_PAD_INDEX         (42)
#define DRDY_PAD_ALT_FUNC      (PAD_MUX_ALT0)

/** \brief GPIO24
 */
#define NOT_GPIO_INSTANCE      (1)
#define NOT_GPIO_PIN           (8)
#define NOT_PAD_INDEX          (44)
#define NOT_PAD_ALT_FUNC       (PAD_MUX_ALT0)

/** \brief GPIO14
 */
#define G12_GPIO_INSTANCE      (0)
#define G12_GPIO_PIN           (14)
#define G12_PAD_INDEX          (13)
#define G12_PAD_ALT_FUNC       (PAD_MUX_ALT4)

/** \brief GPIO15
 */
#define G13_GPIO_INSTANCE      (0)
#define G13_GPIO_PIN           (15)
#define G13_PAD_INDEX          (14)
#define G13_PAD_ALT_FUNC       (PAD_MUX_ALT4)

#define MSG_FLAG             (0xF9)

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
    PRIV_MSG_WAKEUP,
};

typedef struct
{
    uint8_t type;
    uint32_t ts;
} privMsg_t;

enum
{
    PRIV_DATA_AT,
    PRIV_DATA_IP,
};

typedef struct privData privData_t;
typedef STAILQ_ENTRY(privData) privDataIter_t;
typedef STAILQ_HEAD(, privData) privDataHead_t;
struct privData
{
    uint8_t type;
    DlPduBlock *pPdu;
    DlPduBlock *pCurPdu;
    privDataIter_t iter;
};

typedef struct
{
    uint32_t type;
    void *data;
} atMsg_t;

extern NetifRecvDlIpPkg gPsDlIpPkgProcFunc;

/** \brief driver instance declare */
extern ARM_DRIVER_SPI Driver_SPI0;

extern uint16_t crc16ccitt(uint16_t crc, uint8_t *bytes, int start, int len);

extern void netif_dump_dl_packet(u8_t *data, u16_t len, u8_t type);

static ARM_DRIVER_SPI *spiSlaveDrv = &CREATE_SYMBOL(Driver_SPI, 0);

static osMessageQueueId_t main_msgq;
static osMessageQueueId_t atp_msgq;
static osSemaphoreId_t done_sema;
static osSemaphoreId_t hrdy_high_sema;

static privDataHead_t dq;

static uint8_t hrdy_debounce_edge;

static int seq = 0;

static int ims_cid = 2;

static int voice_cid = 0;

static int default_cid = 1;

static bool spi_error = true;

// static netif_input_fn input_cpy[CMI_PS_MAX_VALID_CID];

// static int slp_count = 0;

const uint8_t host_mac[ETH_HWADDR_LEN] = {0xf0, 0x4b, 0xb3, 0xb9, 0xeb, 0xe5};
const uint8_t dev_mac[ETH_HWADDR_LEN] = {0xfa, 0x32, 0x47, 0x15, 0xe1, 0x88};

void RetCheck(int32_t cond, char * v1)
{
    if (cond == ARM_DRIVER_OK) {
        // LUAT_DEBUG_PRINT("SPI %s OK", (v1));
    } else {
        LUAT_DEBUG_PRINT("SPI %s Failed !!, %d", (v1), cond);
    }
}

void g12toggle()
{
    static int lvl = 0;

    lvl = !lvl;
    GPIO_pinWrite(G12_GPIO_INSTANCE, 1 << G12_GPIO_PIN, lvl << G12_GPIO_PIN);
}

void g13toggle()
{
    static int lvl = 0;

    lvl = !lvl;
    GPIO_pinWrite(G13_GPIO_INSTANCE, 1 << G13_GPIO_PIN, lvl << G13_GPIO_PIN);
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

    uint32_t length = 0;
    if (type != MSG_TYPE_GET_LEN)
    {
        uint16_t len = head_len + data_len;
        len = initMsgBuf(buf, buf_len, MSG_TYPE_GET_LEN, NULL, 0, &len, sizeof(len));
        buf += len;
        length += len;
    }

    length += (sizeof(msgHdr_t) + head_len + data_len + sizeof(uint16_t));
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
    privMsg_t m;
    m.type = type;
    m.ts = tick;

    osStatus_t status;
    if (type == PRIV_MSG_HRDY)
    {
        status = osMessageQueuePutToFront(main_msgq, &m, 0, 0);
    }
    else
    {
        status = osMessageQueuePut(main_msgq, &m, 0, 0);
    }
    EC_ASSERT(status == osOK, 0, 0, 0);
}

static void newMsg(uint8_t type, DlPduBlock *pdu)
{
    if (type == MSG_TYPE_DEVICE_DATA)
    {
        netif_dump_dl_packet(pdu->pPdu, pdu->length, 5 /*LWIP_NETIF_TYPE_WAN_INTERNET*/);
    }

    privData_t *pd = malloc(sizeof(privData_t));
    if (pd == NULL)
    {
        LUAT_DEBUG_PRINT("Discard new msg");
        OsaDlfcFreeMem(pdu);
        return;
    }

    pd->type = type;
    pd->pPdu = pdu;
    pd->pCurPdu = pdu;

    ostaskENTER_CRITICAL();
    bool empty = STAILQ_EMPTY(&dq);
    STAILQ_INSERT_TAIL(&dq, pd, iter);
    ostaskEXIT_CRITICAL();

    if (empty)
    {
        LUAT_DEBUG_PRINT("empty");
        sendPrivMsg(PRIV_MSG_NEW_DATA, osKernelGetTickCount());
    }
}

static void rilMsg(const char *pStr, UINT32 strLen)
{
    DlPduBlock *pdu = (DlPduBlock *)OsaDlfcAllocDlPduNonBlocking(strLen);
    if (pdu != NULL)
    {
        memcpy(pdu->pPdu, pStr, strLen);
        newMsg(MSG_TYPE_CMD_RSP, pdu);
    }
}

static INT32 rilRspCallback(UINT8 chanId, const char *pStr, UINT32 strLen, void *pArg)
{
    rilMsg(pStr, strLen);
    return strLen;
}

static INT32 rilUrcCallback(UINT8 chanId, const char *pStr, UINT32 strLen)
{
    static char *nwAct = "+CGEV: NW ACT ";

    char *s;
    if ((s = strstr(pStr, nwAct)) != NULL)
    {
        int copyLen = strlen(s) - strlen(nwAct) + 1;
        char *copy = malloc(copyLen);
        memcpy(copy, &s[strlen(nwAct)], copyLen - 1);
        copy[copyLen - 1] = '\0';

        LUAT_DEBUG_PRINT("copy %s", copy);

        int i = 0;
        char *tok = strtok(copy, ",");
        while (tok != NULL)
        {
            if (i == 0)
            {
                if (atoi(tok) != ims_cid)
                {
                    LUAT_DEBUG_PRINT("not ims_cid %d", ims_cid);
                    break;
                }
            }
            else if (i == 1)
            {
                voice_cid = atoi(tok);

                LUAT_DEBUG_PRINT("voice_cid %d", voice_cid);

                break;
            }

            i++;
            tok = strtok(NULL, ",");
        }

        free(copy);
    }

    rilMsg(pStr, strLen);
    return strLen;
}

static void newTask(const char *name, osThreadFunc_t entry, uint32_t stack_size, uint32_t priority)
{
    osThreadAttr_t task_attr;
    memset(&task_attr, 0, sizeof(task_attr));

    task_attr.name = name;
    task_attr.stack_size = stack_size;
    task_attr.priority = priority;

    osThreadNew(entry, NULL, &task_attr);
}

static uint32_t enter_sc(void)
{
    if (osIsInISRContext())
    {
        return ostaskENTER_CRITICAL_ISR();
    }
    else
    {
        ostaskENTER_CRITICAL();
        return 0;
    }

    return 0;
}

static void exit_sc(uint32_t isrm)
{
    if (osIsInISRContext())
    {
        ostaskEXIT_CRITICAL_ISR(isrm);
    }
    else
    {
        ostaskEXIT_CRITICAL();
    }
}

/**
  \fn          void GPIO_ISR()
  \brief       GPIO interrupt service routine
  \return
*/
static void GPIO_ISR()
{
    //Save current irq mask and diable whole port interrupts to get rid of interrupt overflow
    int expectLevel = (hrdy_debounce_edge == GPIO_INTERRUPT_LOW_LEVEL || hrdy_debounce_edge == GPIO_INTERRUPT_FALLING_EDGE) ? 0 : 1;
    uint16_t portIrqMask = GPIO_saveAndSetIrqMask(HRDY_GPIO_INSTANCE);

    g12toggle();

    if (GPIO_getInterruptFlags(HRDY_GPIO_INSTANCE) & (1 << HRDY_GPIO_PIN))
    {
        GPIO_clearInterruptFlags(HRDY_GPIO_INSTANCE, 1 << HRDY_GPIO_PIN);

        int level = GPIO_pinRead(HRDY_GPIO_INSTANCE, HRDY_GPIO_PIN);
        if (level == expectLevel)
        {
            if (level == 0)
            {
                sendPrivMsg(PRIV_MSG_HRDY, osKernelGetTickCount());
            }
            else
            {
                osSemaphoreRelease(hrdy_high_sema);
            }

            portIrqMask &= ~(1 << HRDY_GPIO_PIN);
        }
    }

    GPIO_restoreIrqMask(HRDY_GPIO_INSTANCE, portIrqMask);
}

static inline void drdy(bool isHigh)
{
    if (isHigh)
    {
        GPIO_pinWrite(DRDY_GPIO_INSTANCE, 1 << DRDY_GPIO_PIN, 1 << DRDY_GPIO_PIN);
    }
    else
    {
        GPIO_pinWrite(DRDY_GPIO_INSTANCE, 1 << DRDY_GPIO_PIN, 0);
    }
}

static inline void notify(uint32_t us)
{
    GPIO_pinWrite(NOT_GPIO_INSTANCE, 1 << NOT_GPIO_PIN, 1 << NOT_GPIO_PIN);
    delay_us(us);
    GPIO_pinWrite(NOT_GPIO_INSTANCE, 1 << NOT_GPIO_PIN, 0);
}

static void initGpio(bool sleep1)
{
    if (!sleep1)
    {
        slpManAONIOPowerOn();
        slpManNormalIOVoltSet(IOVOLT_1_80V);
        slpManAONIOVoltSet(IOVOLT_1_80V);
    }

    APmuWakeupPadSettings_t wakeupPadSetting;
    wakeupPadSetting.negEdgeEn = false;
    wakeupPadSetting.posEdgeEn = false;
    wakeupPadSetting.pullDownEn = false;
    wakeupPadSetting.pullUpEn = false;

    slpManSetWakeupPadCfg(WAKEUP_PAD_0, false, &wakeupPadSetting);
    slpManSetWakeupPadCfg(WAKEUP_PAD_1, false, &wakeupPadSetting);
    slpManSetWakeupPadCfg(WAKEUP_PAD_2, false, &wakeupPadSetting);
    slpManSetWakeupPadCfg(WAKEUP_PAD_3, false, &wakeupPadSetting);
    slpManSetWakeupPadCfg(WAKEUP_PAD_4, false, &wakeupPadSetting);
    slpManSetWakeupPadCfg(WAKEUP_PAD_5, false, &wakeupPadSetting);

    NVIC_DisableIRQ(PadWakeup0_IRQn);
    NVIC_DisableIRQ(PadWakeup1_IRQn);
    NVIC_DisableIRQ(PadWakeup2_IRQn);
    NVIC_DisableIRQ(PadWakeup3_IRQn);
    NVIC_DisableIRQ(PadWakeup4_IRQn);
    NVIC_DisableIRQ(PadWakeup5_IRQn);

    // DRDY
    PadConfig_t padConfig;
    PAD_getDefaultConfig(&padConfig);
    padConfig.mux = DRDY_PAD_ALT_FUNC;
    PAD_setPinConfig(DRDY_PAD_INDEX, &padConfig);

    GpioPinConfig_t config;
    config.pinDirection = GPIO_DIRECTION_OUTPUT;
    config.misc.initOutput = 0;
    GPIO_pinConfig(DRDY_GPIO_INSTANCE, DRDY_GPIO_PIN, &config);

    // HRDY
    padConfig.mux = HRDY_PAD_ALT_FUNC;
    PAD_setPinConfig(HRDY_PAD_INDEX, &padConfig);

    config.pinDirection = GPIO_DIRECTION_INPUT;
    config.misc.interruptConfig = GPIO_INTERRUPT_DISABLED;
    GPIO_pinConfig(HRDY_GPIO_INSTANCE, HRDY_GPIO_PIN, &config);

    // NOT
    padConfig.mux = NOT_PAD_ALT_FUNC;
    PAD_setPinConfig(NOT_PAD_INDEX, &padConfig);

    config.pinDirection = GPIO_DIRECTION_OUTPUT;
    config.misc.initOutput = 0;
    GPIO_pinConfig(NOT_GPIO_INSTANCE, NOT_GPIO_PIN, &config);

    // DIRE
    padConfig.mux = DIRE_PAD_ALT_FUNC;
    PAD_setPinConfig(DIRE_PAD_INDEX, &padConfig);

    config.pinDirection = GPIO_DIRECTION_INPUT;
    config.misc.interruptConfig = GPIO_INTERRUPT_DISABLED;
    GPIO_pinConfig(DIRE_GPIO_INSTANCE, DIRE_GPIO_PIN, &config);

    // G12
    padConfig.mux = G12_PAD_ALT_FUNC;
    PAD_setPinConfig(G12_PAD_INDEX, &padConfig);

    config.pinDirection = GPIO_DIRECTION_OUTPUT;
    config.misc.initOutput = 0;
    GPIO_pinConfig(G12_GPIO_INSTANCE, G12_GPIO_PIN, &config);

    // G13
    padConfig.mux = G13_PAD_ALT_FUNC;
    PAD_setPinConfig(G13_PAD_INDEX, &padConfig);

    config.pinDirection = GPIO_DIRECTION_OUTPUT;
    config.misc.initOutput = 0;
    GPIO_pinConfig(G13_GPIO_INSTANCE, G13_GPIO_PIN, &config);

    if (!sleep1)
    {
        hrdy_high_sema = osSemaphoreNew(1, 0, NULL);

        // Enable IRQ
        XIC_DisableIRQ(PXIC1_GPIO_IRQn);
        XIC_SetVector(PXIC1_GPIO_IRQn, GPIO_ISR);
        XIC_EnableIRQ(PXIC1_GPIO_IRQn);
    }
}

void sendAtMsg(uint32_t type, void *data)
{
    atMsg_t msg;
    msg.type = type;
    msg.data = data;

    EC_ASSERT(osMessageQueuePut(atp_msgq, &msg, 0, 0) == osOK, 0, 0, 0);
}

/**
  \fn          void SPI_Callback()
  \brief       SPI callback
  \return
*/
static bool overflow = false;
static void SPI_Callback(uint32_t event)
{
    osSemaphoreRelease(done_sema);
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

    // ret = spiSlaveDrv->Control(ARM_SPI_SET_BUS_SPEED, 6000000);
    // RetCheck(ret, "Bus speed");
}

static uint32_t spiTransfer(void *out, void *in, uint32_t len)
{
    //EC_ASSERT(len >= 4, 0, 0, 0);

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

    osSemaphoreAcquire(done_sema, 0);
    osSemaphoreAcquire(hrdy_high_sema, 0);

    uint32_t sc = enter_sc();

    if (GPIO_pinRead(HRDY_GPIO_INSTANCE, HRDY_GPIO_PIN) == 1)
    {
        exit_sc(sc);
        LUAT_DEBUG_PRINT("hrdy high");
        return 0;
    }

    hrdy_debounce_edge = GPIO_INTERRUPT_HIGH_LEVEL;
    GpioPinConfig_t config;
    config.pinDirection = GPIO_DIRECTION_INPUT;
    config.misc.interruptConfig = hrdy_debounce_edge;
    GPIO_pinConfig(HRDY_GPIO_INSTANCE, HRDY_GPIO_PIN, &config);

    exit_sc(sc);

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
    uint32_t preWaitTime = osKernelGetTickCount();
    if (osSemaphoreAcquire(hrdy_high_sema, MAX_TRSNSFER_TIMEOUT) == osOK)
    {
        LUAT_DEBUG_PRINT("Acquire hrdy_high_sema");

        uint32_t passedTime = (osKernelGetTickCount() - preWaitTime) > 0 ? (osKernelGetTickCount() - preWaitTime) : (0xFFFFFFFF - preWaitTime + osKernelGetTickCount());

        if (dir_out)
        {
            int wait = MAX_TRSNSFER_TIMEOUT - passedTime;
            if (osSemaphoreAcquire(done_sema, wait < 0 ? 0 : wait) != osOK)
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

        //GPIO_pinWrite(DIRE_GPIO_INSTANCE, 1 << DIRE_GPIO_PIN, 1 << DIRE_GPIO_PIN);

        //delay_us(100);

        //GPIO_pinWrite(DIRE_GPIO_INSTANCE, 1 << DIRE_GPIO_PIN, 0);
    }

    return 1;

#if 0
    uint32_t len = initAckMsg(slave_buffer_out, MAX_TRANSFER_SIZE, code);
    return 1; //spiTransfer(slave_buffer_out, slave_buffer_in, len);
#endif
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
            DlPduBlock *pdu = OsaDlfcAllocDlPduBlocking(sizeof(struct etharp_hdr));
            if (pdu != NULL)
            {
                memcpy(pdu->pPdu, reply, sizeof(struct etharp_hdr));
                newMsg(MSG_TYPE_DEVICE_DATA, pdu);
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

void dlIpPkgCb(UINT8 cid, DlPduBlock *pPduHdr)
{
    LUAT_DEBUG_PRINT("cid %d pkt len %d", cid, pPduHdr->length);
    DlPduBlock *pNext = pPduHdr;
    while (pNext)
    {
        DlPduBlock *pdu = OsaDlfcAllocDlPduBlocking(pNext->length);
        memcpy(pdu->pPdu, pNext->pPdu, pNext->length);
        newMsg(MSG_TYPE_DEVICE_DATA, pdu);
        pNext = pNext->pNext;
    }

    PsifFreeDlIpPkgBlockList(pPduHdr);
}

u8_t ip4_filter(struct pbuf *p, struct netif *inp)
{
    LUAT_DEBUG_PRINT("inpkt");

    uint16_t offset = 0;
    struct pbuf *q = NULL;

    do
    {
        DlPduBlock *pdu = OsaDlfcAllocDlPduBlocking(p->tot_len);
        if (pdu == NULL)
        {
            break;
        }

        uint8_t *pData = (uint8_t *)pdu->pPdu;

        for (q = p; q != NULL; q = q->next)
        {
            memcpy(&pData[offset], q->payload, q->len);
            offset += q->len;
        }

        newMsg(MSG_TYPE_DEVICE_DATA, pdu);
    } while (0);

    pbuf_free(p);

    return 1 /* eaten */;
}

// static err_t netifInput(struct pbuf *p, struct netif *inp)
// {
//     LUAT_DEBUG_PRINT("inpkt");

//     pbuf_free(p);

//     return ERR_OK;
// }

// static void hookNetifInput(void *ctx)
// {
//     uint8_t cid = (uint32_t)ctx;
//     struct netif *netif = netif_find_by_ip4_cid(cid);
//     if (netif)
//     {
//         LUAT_DEBUG_PRINT("cid %d, hook", cid);

//         input_cpy[cid] = netif->input;
//         netif->input = netifInput;
//     }
// }

INT32 psUrcCallback(PsEventID eventID, void *param, UINT32 paramLen)
{
    // uint32_t cid;
    NmAtiNetInfoInd *netif = NULL;

    switch(eventID)
    {
        case PS_URC_ID_PS_NETINFO:
        {
            netif = (NmAtiNetInfoInd *)param;
            if (netif->netifInfo.netStatus == NM_NETIF_ACTIVATED){
                LUAT_DEBUG_PRINT("netif acivated");

                LUAT_DEBUG_PRINT("ip type %d netifType %d", netif->netifInfo.ipType, netif->netifInfo.netifType);

                if (netif->netifInfo.ipType == NM_NET_TYPE_IPV6
                    && netif->netifInfo.netifType == NM_OTHER_NETIF)
                {
                    // if (netif->netifInfo.ipv6Cid == 2)
                    //     gPsDlIpPkgProcFunc = dlIpPkgCb;

                    // ims_cid = netif->netifInfo.ipv6Cid;
                }

                // if (netif->netifInfo.ipType == NM_NET_TYPE_IPV4 || netif->netifInfo.ipType == NM_NET_TYPE_IPV4V6)
                // {
                //     cid = netif->netifInfo.ipv4Cid;
                //     tcpip_callback(hookNetifInput, (void *)cid);
                // }
            }
            else if (netif->netifInfo.netStatus == NM_NETIF_OOS)
            {
                LUAT_DEBUG_PRINT("PSIF network OOS");
            }
            else if (netif->netifInfo.netStatus == NM_NO_NETIF_OR_DEACTIVATED ||
                     netif->netifInfo.netStatus == NM_NO_NETIF_NOT_DIAL)
            {
                LUAT_DEBUG_PRINT("PSIF network deactive");
            }
            break;
        }
        default:
            break;

    }
    return 0;
}

static void beforeSleep(void *pdata, slpManLpState state)
{
    APmuWakeupPadSettings_t wakeupPadSetting;
    wakeupPadSetting.negEdgeEn = true;
    wakeupPadSetting.posEdgeEn = false;
    wakeupPadSetting.pullDownEn = false;
    wakeupPadSetting.pullUpEn = true;

    switch(state)
    {
        case SLPMAN_SLEEP1_STATE:
        case SLPMAN_SLEEP2_STATE:
        case SLPMAN_HIBERNATE_STATE:
            slpManSetWakeupPadCfg(WAKEUP_PAD_5, true, &wakeupPadSetting);
            NVIC_EnableIRQ(PadWakeup5_IRQn);
            break;
        default:
            break;
    }
}

static void afterSleep(void *pdata, slpManLpState state)
{
    switch(state)
    {
        case SLPMAN_SLEEP1_STATE:
        case SLPMAN_SLEEP2_STATE:
        case SLPMAN_HIBERNATE_STATE:
            //initGpio(true);
            sendPrivMsg(PRIV_MSG_WAKEUP, osKernelGetTickCount());
            break;
        default:
            break;
    }
}

void _NOT(void *arg)
{
    while (1)
    {
        GPIO_pinWrite(NOT_GPIO_INSTANCE, 1 << NOT_GPIO_PIN, 1 << NOT_GPIO_PIN);
        osDelay(10);
        GPIO_pinWrite(NOT_GPIO_INSTANCE, 1 << NOT_GPIO_PIN, 0);
        osDelay(10);
    }
}

static void SPI_ExampleEntry(void *arg)
{
    //osDelay(1000 * 5);

    //int i;
    //int ret;
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

    slpManRegisterUsrdefinedBackupCb(beforeSleep, NULL);
    slpManRegisterUsrdefinedRestoreCb(afterSleep, NULL);

    slpManWakeSrc_e wakeSrc = slpManGetWakeupSrc();
    slpManSlpState_t slpState = slpManGetLastSlpState();
    if ((slpState == SLP_ACTIVE_STATE)
        || ((slpState == SLP_SLP2_STATE || slpState == SLP_HIB_STATE) && wakeSrc == WAKEUP_FROM_PAD))
    {
        apmuSetDeepestSleepMode(AP_STATE_IDLE);
    }

    LUAT_DEBUG_PRINT("wakeSrc %d, slpState %d", wakeSrc, slpState);

    memset(slave_buffer_out, 0, sizeof(slave_buffer_out));
    memset(slave_buffer_in, 0, sizeof(slave_buffer_in));

    initGpio(false);

    //newTask("spi", _NOT, 4096, osPriorityAboveNormal);

    registerPSEventCallback(PS_GROUP_ALL_MASK, psUrcCallback);
    //gPsDlIpPkgProcFunc = dlIpPkgCb;

    LUAT_DEBUG_PRINT("SPI Example Start");

    LUAT_DEBUG_PRINT("Wait for master to trigger transfer");

    while (1)
    {
        g13toggle();

        ostaskENTER_CRITICAL();
        queueDirty = (STAILQ_FIRST(&dq) != NULL);
        ostaskEXIT_CRITICAL();

        last_tick = osKernelGetTickCount();

        int l;
        if ((l = GPIO_pinRead(HRDY_GPIO_INSTANCE, HRDY_GPIO_PIN)) == 1 || spi_error)
        {
            LUAT_DEBUG_PRINT("hrdy %s, %d", l == 1 ? "high" : "low", spi_error);

            if (spi_error)
            {
                pData = NULL;
            }

            privMsg_t m;
            memset(&m, 0, sizeof(m));

            bool skip = false;
            osStatus_t status = osOK;

            do
            {
                uint32_t sc = enter_sc();

                if (GPIO_pinRead(HRDY_GPIO_INSTANCE, HRDY_GPIO_PIN) == 0 && !spi_error)
                {
                    exit_sc(sc);
                    skip = true;
                    LUAT_DEBUG_PRINT("hrdy low");
                    break;
                }

                hrdy_debounce_edge = spi_error ? GPIO_INTERRUPT_FALLING_EDGE : GPIO_INTERRUPT_LOW_LEVEL;
                GpioPinConfig_t config;
                config.pinDirection = GPIO_DIRECTION_INPUT;
                config.misc.interruptConfig = hrdy_debounce_edge;
                GPIO_pinConfig(HRDY_GPIO_INSTANCE, HRDY_GPIO_PIN, &config);

                exit_sc(sc);

                if (queueDirty && pData == NULL)
                {
                    apmuSetDeepestSleepMode(AP_STATE_IDLE);
                    if (dnot)
                    {
                        notify(20);
                        dnot = false;
                    }
                }

                spi_error = false;

                LUAT_DEBUG_PRINT("Wait main_msgq");
                status = osMessageQueueGet(main_msgq, &m, NULL, osWaitForever);
            } while (0);

            if (!skip)
            {
                if (status != osOK)
                {
                    EC_ASSERT(pData != NULL, 0, 0, 0);
                }
                else if (m.type == PRIV_MSG_NEW_DATA)
                {
                    LUAT_DEBUG_PRINT("NEW DATA");
                    continue;
                }
                else if (m.type == PRIV_MSG_HRDY)
                {
                    LUAT_DEBUG_PRINT("HRDY");

                    LUAT_DEBUG_PRINT("last %x, curr %x", last_tick, m.ts);

                    if (GPIO_pinRead(HRDY_GPIO_INSTANCE, HRDY_GPIO_PIN) == 1
                        || m.ts < last_tick)
                    {
                        continue;
                    }
                }
                else if (m.type == PRIV_MSG_WAKEUP)
                {
                    wakeSrc = slpManGetWakeupSrc();
                    if (wakeSrc == WAKEUP_FROM_PAD)
                    {
                        apmuSetDeepestSleepMode(AP_STATE_IDLE);
                    }

                    APmuWakeupPadSettings_t wakeupPadSetting;
                    wakeupPadSetting.negEdgeEn = false;
                    wakeupPadSetting.posEdgeEn = false;
                    wakeupPadSetting.pullDownEn = false;
                    wakeupPadSetting.pullUpEn = false;

                    slpManSetWakeupPadCfg(WAKEUP_PAD_5, false, &wakeupPadSetting);
                    NVIC_DisableIRQ(PadWakeup5_IRQn);
                    initGpio(true);

                    continue;
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
        if (GPIO_pinRead(DIRE_GPIO_INSTANCE, DIRE_GPIO_PIN) == 1)
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
                if (pData->type == MSG_TYPE_CMD_RSP)
                {
                    sendDataMsg(slave_buffer_out, MAX_TRANSFER_SIZE, pData->type, pData->pCurPdu->pPdu, pData->pCurPdu->length);
                }
                else if (pData->type == MSG_TYPE_DEVICE_DATA)
                {
                    struct eth_hdr eth_hdr;
                    memcpy(&eth_hdr.dest, host_mac, ETH_HWADDR_LEN);
                    memcpy(&eth_hdr.src, dev_mac, ETH_HWADDR_LEN);
                    uint8_t *ipdata = (uint8_t *)pData->pCurPdu->pPdu;
                    if ((ipdata[0] & 0xF0) == 0x40)
                        eth_hdr.type = __htons(ETHTYPE_IP);
                    else if ((ipdata[0] & 0xF0) == 0x60)
                        eth_hdr.type = __htons(ETHTYPE_IPV6);
                    else if (ipdata[0] == 0x00 && ipdata[1] == 0x01 &&
                            ipdata[2] == 0x08 && ipdata[3] == 0x00)
                        eth_hdr.type = __htons(ETHTYPE_ARP);
                    else
                        EC_ASSERT(0, 0, 0, 0);

                    sendEthDataMsg(slave_buffer_out, MAX_TRANSFER_SIZE, &eth_hdr, pData->pCurPdu->pPdu, pData->pCurPdu->length);
                }

                if (pData->pCurPdu->pNext == NULL)
                {
                    ostaskENTER_CRITICAL();
                    STAILQ_REMOVE_HEAD(&dq, iter);
                    ostaskEXIT_CRITICAL();
                    OsaDlfcFreeMem(pData->pPdu);
                    free(pData);
                }
                else
                {
                    pData->pCurPdu = pData->pCurPdu->pNext;
                }

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
#if 0
                        transSize = spiTransfer(slave_buffer_out, &in_buf[sizeof(msgHdr_t)], dataLen + sizeof(uint16_t));
                        if (transSize == 0)
                        {
                            break;
                        }
#endif
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

                if (pHdr->type == MSG_TYPE_GET_DATA)
                {
                    pData = STAILQ_FIRST(&dq);

                    if (pData->type == MSG_TYPE_CMD_RSP)
                    {
                        sendDataMsg(slave_buffer_out, MAX_TRANSFER_SIZE, pData->type, pData->pCurPdu->pPdu, pData->pCurPdu->length);
                    }
                    else if (pData->type == MSG_TYPE_DEVICE_DATA)
                    {
                        struct eth_hdr eth_hdr;
                        memcpy(&eth_hdr.dest, host_mac, ETH_HWADDR_LEN);
                        memcpy(&eth_hdr.src, dev_mac, ETH_HWADDR_LEN);
                        uint8_t *ipdata = (uint8_t *)pData->pCurPdu->pPdu;
                        if ((ipdata[0] & 0xF0) == 0x40)
                            eth_hdr.type = __htons(ETHTYPE_IP);
                        else if ((ipdata[0] & 0xF0) == 0x60)
                            eth_hdr.type = __htons(ETHTYPE_IPV6);
                        else if (ipdata[0] == 0x00 && ipdata[1] == 0x01 &&
                                ipdata[2] == 0x08 && ipdata[3] == 0x00)
                            eth_hdr.type = __htons(ETHTYPE_ARP);
                        else
                            EC_ASSERT(0, 0, 0, 0);

                        sendEthDataMsg(slave_buffer_out, MAX_TRANSFER_SIZE, &eth_hdr, pData->pCurPdu->pPdu, pData->pCurPdu->length);
                    }

                    if (pData->pCurPdu->pNext == NULL)
                    {
                        ostaskENTER_CRITICAL();
                        STAILQ_REMOVE_HEAD(&dq, iter);
                        ostaskEXIT_CRITICAL();
                        OsaDlfcFreeMem(pData->pPdu);
                        free(pData);
                    }
                    else
                    {
                        pData->pCurPdu = pData->pCurPdu->pNext;
                    }

                    pData = NULL;//notify

                    break;
                }

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
                    case MSG_TYPE_GET_LEN:
                    {
                        pData = STAILQ_FIRST(&dq);
                        devDataLen = pData == NULL ? 0 : pData->pPdu->length;
                        sendLenMsg(devDataLen);
                        break;
                    }

                    case MSG_TYPE_AT_CMD:
                    {
                        at = malloc(dataLen + 1);
                        memcpy(at, &in_buf[sizeof(msgHdr_t)], dataLen);
                        at[dataLen] = '\0';
                        sendAtMsg(0, at);
                        sendAckMsg(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_ERROR_NONE);
                        break;
                    }

                    case MSG_TYPE_HOST_DATA:
                    {
                        pkt = malloc(dataLen);
                        memcpy(pkt, &in_buf[sizeof(msgHdr_t)], dataLen);
                        sendIp2Ps(pkt, dataLen);
                        sendAckMsg(slave_buffer_out, MAX_TRANSFER_SIZE, MSG_ERROR_NONE);

                        transSize -= (sizeof(msgHdr_t) + dataLen + sizeof(uint16_t));
                        if (transSize)
                        {
                            in_buf = &in_buf[sizeof(msgHdr_t) + dataLen + sizeof(uint16_t)];
                            goto DECODE;
                        }
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

static void mobile_event_cb(LUAT_MOBILE_EVENT_E event, uint8_t index, uint8_t status)
{
    ip_addr_t ipv4;
    ip_addr_t ipv6;
    switch(event)
    {
    case LUAT_MOBILE_EVENT_NETIF:
        switch (status)
        {
        case LUAT_MOBILE_NETIF_LINK_ON:
            luat_mobile_get_local_ip(0, 1, &ipv4, &ipv6);
            if (ipv4.type != 0xff)
            {
                LUAT_DEBUG_PRINT("IPV4 %s", ip4addr_ntoa(&ipv4.u_addr.ip4));
            }
            if (ipv6.type != 0xff)
            {
                LUAT_DEBUG_PRINT("IPV6 %s", ip6addr_ntoa(&ipv4.u_addr.ip6));
            }
            break;
        default:
            LUAT_DEBUG_PRINT("不能上网");
            break;
        }
        break;
    default:
        break;
    }
}

void spiTaskInit(void)
{
    luat_mobile_event_register_handler(mobile_event_cb);

    STAILQ_INIT(&dq);

    done_sema = osSemaphoreNew(1U, 0, PNULL);

    main_msgq = osMessageQueueNew(256, sizeof(privMsg_t), NULL);

    newTask("spi", SPI_ExampleEntry, 4096, osPriorityHigh);
}

INIT_TASK_EXPORT(spiTaskInit, "0");
