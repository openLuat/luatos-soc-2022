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

#include "common_api.h"
#include "bsp_custom.h"

#include "lfs.h"

luat_rtos_task_handle sfud_task_handle;


#define LFS_BLOCK_SIZE (4096)
#define LFS_BLOCK_DEVICE_READ_SIZE (256)
#define LFS_BLOCK_DEVICE_PROG_SIZE (256)
#define LFS_BLOCK_DEVICE_CACHE_SIZE (256)
#define LFS_BLOCK_DEVICE_ERASE_SIZE (4096) // one sector 4KB
#define LFS_BLOCK_DEVICE_TOTOAL_SIZE (FLASH_FS_REGION_SIZE)
#define LFS_BLOCK_DEVICE_LOOK_AHEAD (16)


static int sfud_block_device_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    sfud_flash* flash = (sfud_flash*)cfg->context;
    int ret = sfud_read(flash, block * LFS_BLOCK_SIZE + off, size, buffer);
    return LFS_ERR_OK;
}

static int sfud_block_device_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    sfud_flash* flash = (sfud_flash*)cfg->context;
    int ret = sfud_write(flash, block * LFS_BLOCK_SIZE + off, size, buffer);
    return LFS_ERR_OK;
}

static int sfud_block_device_erase(const struct lfs_config *cfg, lfs_block_t block) {
    sfud_flash* flash = (sfud_flash*)cfg->context;
    int ret = sfud_erase(flash, block * LFS_BLOCK_SIZE, LFS_BLOCK_SIZE);
    return LFS_ERR_OK;
}

static int sfud_block_device_sync(const struct lfs_config *cfg) {
    return LFS_ERR_OK;
}

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

// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;

static void task_test_sfud(void *param)
{

    int re = -1;
    luat_spi_setup(&sfud_spi_flash);

    if (re = sfud_init()!=0){
        LUAT_DEBUG_PRINT("sfud_init error is %d\n", re);
    }
    const sfud_flash *flash = sfud_get_device_table();

    // configuration of the filesystem is provided by this struct
    const struct lfs_config sfud_lfs_cfg = {
        .context = flash,
        // block device operations
        .read  = sfud_block_device_read,
        .prog  = sfud_block_device_prog,
        .erase = sfud_block_device_erase,
        .sync  = sfud_block_device_sync,

        // block device configuration
        .read_size = LFS_BLOCK_DEVICE_READ_SIZE,
        .prog_size = LFS_BLOCK_DEVICE_PROG_SIZE,
        .block_size = flash->chip.erase_gran,
        .block_count = flash->chip.capacity / flash->chip.erase_gran,
        .cache_size = LFS_BLOCK_DEVICE_CACHE_SIZE,
        .lookahead_size = LFS_BLOCK_DEVICE_LOOK_AHEAD,
        .block_cycles = 200,
    };

    // mount the filesystem
    int err = lfs_mount(&lfs, &sfud_lfs_cfg);
    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err) {
        LUAT_DEBUG_PRINT("lfs_mount err: %d\n", err);
        lfs_format(&lfs, &sfud_lfs_cfg);
        lfs_mount(&lfs, &sfud_lfs_cfg);
    }
    // read current count
    uint32_t boot_count = 0;
    lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));
    // update boot count
    boot_count += 1;
    lfs_file_rewind(&lfs, &file);
    lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));
    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&lfs, &file);
    // release any resources we were using
    lfs_unmount(&lfs);
    // print the boot count
    LUAT_DEBUG_PRINT("boot_count: %d\n", boot_count);
    
    while (1)
    {
        luat_rtos_task_sleep(1000);
    }
}

// 合宙云喇叭开发板打开下面注释使能flash
// #define FLASH_EN	HAL_GPIO_26
// #define FLASH_EN_ALT_FUN	0

static void task_demo_sfud(void)
{
    // luat_gpio_cfg_t gpio_cfg;
	// luat_gpio_set_default_cfg(&gpio_cfg);

	// gpio_cfg.pin = FLASH_EN;
	// gpio_cfg.alt_fun = FLASH_EN_ALT_FUN;
	// luat_gpio_open(&gpio_cfg);
	// luat_gpio_set(FLASH_EN, LUAT_GPIO_HIGH);

    luat_rtos_task_create(&sfud_task_handle, 2048, 20, "sfud", task_test_sfud, NULL, NULL);
}

INIT_TASK_EXPORT(task_demo_sfud,"1");



