#include "luat_base.h"
#include "luat_hmeta.h"
#include "luat_mcu.h"
#include "luat_rtos.h"

extern int soc_get_model_name(char *model);

int luat_hmeta_model_name(char* buff) {
    int ret = soc_get_model_name(buff);
    if (ret == 0)
        return 0;
    uint64_t ticks = luat_mcu_tick64_ms();
    if (ticks < 250) {
        luat_rtos_task_sleep(250 - ticks);
    }
    memset(buff, 0, strlen(buff));
    soc_get_model_name(buff);
    return 0;
}
