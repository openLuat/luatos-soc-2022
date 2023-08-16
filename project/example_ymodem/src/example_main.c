/*
 * Copyright (c) 2022 OpenLuat & AirM2M
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "common_api.h"
#include "luat_rtos.h"
#include "luat_debug.h"

#include "luat_gpio.h"
#include "luat_spi.h"
#include "sfud.h"
#include "luat_vfs.h"
#include "common_api.h"
#include "bsp_custom.h"
#include "luat_mem.h"

#include "lfs.h"

#include "luat_uart.h"

#include "luat_ymodem.h"

/*

    注意,sfud下的文件操作目前线程不安全,需要注意!!!!!!!!

*/

#define UART_ID LUAT_VUART_ID_0

luat_rtos_task_handle ymodem_task_handle;
void *ymodem_handler = NULL;
static HANDLE ymodem_timer;
static bool ymodem_running = false;


luat_spi_t sfud_spi_flash = {
        .id = 0,
        .CPHA = 0,
        .CPOL = 0,
        .dataw = 8,
        .bit_dict = 0,
        .master = 1,
        .mode = 0,
        .bandrate=25600000,
        .cs = 8
};
extern lfs_t* flash_lfs_sfud(sfud_flash* flash, size_t offset, size_t maxsize);

void luat_uart_recv_cb(int uart_id, uint32_t data_len){
    int result = 0;
    uint8_t ack, flag, file_ok, all_done;
    uint8_t send_buff[2] = {0};
    char* data_buff = malloc(data_len+1);
    memset(data_buff,0,data_len+1);
    luat_uart_read(uart_id, data_buff, data_len);
    if (ymodem_handler){
        result = luat_ymodem_receive(ymodem_handler, (uint8_t*)data_buff,data_len,&ack, &flag, &file_ok, &all_done);
        ymodem_running = (bool)!result;
        if (result == 0){
            send_buff[0]=ack;
            send_buff[1]=flag;
            luat_uart_write(uart_id, send_buff, 2);
        }
        if (all_done){
            ymodem_running = false;
        }
        if (file_ok){
            LUAT_DEBUG_PRINT("ymodem file_ok");
            luat_rtos_timer_stop(ymodem_timer);
            luat_ymodem_release(ymodem_handler);
            ymodem_handler = NULL;
        }
    }
    free(data_buff);
}

void ymodem_timer_cb(uint32_t arg){
    char send_buff[1]={'C'};
    if (ymodem_running == false){
        luat_uart_write(UART_ID, send_buff, 1);
        luat_ymodem_reset(ymodem_handler);
    }
}
static int recur_fs(const char* dir_path)
{
    luat_fs_dirent_t *fs_dirent = LUAT_MEM_MALLOC(sizeof(luat_fs_dirent_t)*100);
    memset(fs_dirent, 0, sizeof(luat_fs_dirent_t)*100);

    int lsdir_cnt = luat_fs_lsdir(dir_path, fs_dirent, 0, 100);

    if (lsdir_cnt > 0)
    {
        char path[255] = {0};

        LUAT_DEBUG_PRINT("dir_path=%s, lsdir_cnt=%d", dir_path, lsdir_cnt);

        for (size_t i = 0; i < lsdir_cnt; i++)
        {
            memset(path, 0, sizeof(path));            

            switch ((fs_dirent+i)->d_type)
            {
            // 文件类型
            case 0:   
                snprintf(path, sizeof(path)-1, "%s%s", dir_path, (fs_dirent+i)->d_name);             
                LUAT_DEBUG_PRINT("\tfile=%s, size=%d", path, luat_fs_fsize(path));
                break;
            case 1:
                snprintf(path, sizeof(path)-1, "%s/%s/", dir_path, (fs_dirent+i)->d_name);
                recur_fs(path);
                break;

            default:
                break;
            }
        }        
    }

    LUAT_MEM_FREE(fs_dirent);
    fs_dirent = NULL;
    
    return lsdir_cnt;
}

static void task_test_ymodem(void *param){
    luat_rtos_task_sleep(1000);
    int re = -1;
    luat_uart_t uart = {
        .id = UART_ID,
        .baud_rate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity    = 0,
        .bufsz     = 4096
    };

    luat_spi_setup(&sfud_spi_flash);

    if (re = sfud_init()!=0){
        LUAT_DEBUG_PRINT("sfud_init error is %d\n", re);
    }
    const sfud_flash *flash = sfud_get_device_table();

if(1){//    此为向文件系统传输文件
    luat_fs_init();
    lfs_t* lfs = flash_lfs_sfud(flash, 0, 16*1024);
    if (lfs) {
	    luat_fs_conf_t conf = {
		    .busname = (char*)lfs,
		    .type = "lfs2",
		    .filesystem = "lfs2",
		    .mount_point = "/sfud",
	    };
	    int ret = luat_fs_mount(&conf);
        LUAT_DEBUG_PRINT("vfs mount %s ret %d", "/sfud", ret);
    }
    else {
        LUAT_DEBUG_PRINT("flash_lfs_sfud error");
    }
    recur_fs("/sfud");
    ymodem_handler = luat_ymodem_create_handler("/sfud", NULL);
}else{//    此为直接向flash写数据
    ymodem_handler = luat_ymodem_create_handler_sfud(flash, 0);
}

    luat_uart_setup(&uart);
    luat_uart_ctrl(UART_ID, LUAT_UART_SET_RECV_CALLBACK, luat_uart_recv_cb);

    luat_rtos_timer_create(&ymodem_timer);
    luat_rtos_timer_start(ymodem_timer, 500, 1, ymodem_timer_cb, NULL);
    
    while (1){
        luat_rtos_task_sleep(1000);
    }
}

// 合宙云喇叭开发板打开下面注释使能flash
#define FLASH_EN	HAL_GPIO_26
#define FLASH_EN_ALT_FUN	0

static void task_demo_ymodem(void)
{
    luat_gpio_cfg_t gpio_cfg;
	luat_gpio_set_default_cfg(&gpio_cfg);

	gpio_cfg.pin = FLASH_EN;
	gpio_cfg.alt_fun = FLASH_EN_ALT_FUN;
	luat_gpio_open(&gpio_cfg);
	luat_gpio_set(FLASH_EN, LUAT_GPIO_HIGH);

    luat_rtos_task_create(&ymodem_task_handle, 4096, 20, "ymodem", task_test_ymodem, NULL, NULL);
}

INIT_TASK_EXPORT(task_demo_ymodem,"1");



