#include "common_api.h"
#include "luat_debug.h"
#include "mem_map.h"
void fdb_init(void)
{
    luat_fskv_init(FLASH_FDB_REGION_START + AP_FLASH_XIP_ADDR, FLASH_FDB_REGION_START, 64 * 1024);
    char value[2];
    int ret = luat_fskv_get("flag", value, 2);
    //读取kv数据库用户是否初始化过，如果没有，则写入一个flag和需要初始化的值，表示用户已初始化；如果用户初始化过，则不做任何操作
    LUAT_DEBUG_PRINT("get value result %d", ret);
    if (ret > 0)
    {
        LUAT_DEBUG_PRINT("get value %s", value);
        if(memcmp("1", value, strlen("1")))
        {
            LUAT_DEBUG_PRINT("need init");
            ret = luat_fskv_set("flag", "1", 2);
            LUAT_DEBUG_PRINT("init result1 %d", ret);
            int volume = 15;
            ret = luat_fskv_set("volume", &volume, sizeof(int));
        }
        else
        {
            LUAT_DEBUG_PRINT("no need init");
        }
    }
    else
    {
        ret = luat_fskv_set("flag", "1", 2);
        int volume = 15;
        ret = luat_fskv_set("volume", &volume, sizeof(int));
        LUAT_DEBUG_PRINT("init result2 %d", ret);
    }
}