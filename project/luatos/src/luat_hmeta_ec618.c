#include "luat_base.h"
#include "luat_hmeta.h"
#include "luat_fs.h"
#include "luat_mcu.h"

#define LUAT_LOG_TAG "hmeta"
#include "luat_log.h"

extern int soc_get_model_name(char *model);

int luat_hmeta_model_name(char* buff) {
    int ret = soc_get_model_name(buff);
    if (ret == 0) {
        if (!luat_fs_fexist("/model")) {
            FILE* fd = luat_fs_fopen("/model", "w+");
            if (fd != NULL) {
                luat_fs_fwrite(buff, 1, strlen(buff), fd);
                luat_fs_fclose(fd);
            }
        }
        return 0;
    }
    // 能到这里的, 就是底层还没返回真实的模块类型
    FILE* fd = luat_fs_fopen("/model", "r");
    if (fd == NULL) {
        LLOGW("model name NOT ready yet!!!");
        return -1;
    }
    memset(buff, 0, 16);
    ret = luat_fs_fread(buff, 1, 16, fd);
    luat_fs_fclose(fd);
    if (ret > 0)
        buff[ret] = 0;
    return 0;
}
