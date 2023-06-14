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

#ifndef LUAT_FSKV_H
#define LUAT_FSKV_H

#include "lfs.h"



/**
 * @defgroup luatos_fskv 持久化数据存储接口
 * @{
 */

/**
 * @brief 初始化kv数据存储
 * @param read_start_address[IN] 用于fskv空间的读首地址
 * @param write_start_address[IN] 用于fskv空间的擦写首地址
 * @param total_len[IN] 用于fskv空间的总长度，单位byte
 * @return int == 0 正常 != 0失败
 */
int luat_fskv_init(uint32_t read_start_address, uint32_t write_start_address, uint32_t total_len);

/**
 * @brief 删除指定的key
 * @param key[IN] 待删除的key值
 * @return int == 0 正常 != 0失败
 */
int luat_fskv_del(const char* key);

/**
 * @brief 写入指定key的数据
 * @param key[IN] 待写入的key值,不能为NULL,必须是\0结尾,最大长度64字节
 * @param data[IN] 待写入的数据, 不需要\0结尾
 * @param len[IN] 待写入的数据长度, 不含\0,当前支持最大长度4096字节
 * @return int == 0 正常 != 0失败
 */
int luat_fskv_set(const char* key, void* data, size_t len);

int luat_fskv_size(const char* key, char buff[4]);

/**
 * @brief 读取指定key的数据
 * @param key[IN] 待读取的key值,不能为NULL,必须是\0结尾
 * @param data[IN] 待读取的数据, 可写入空间必须大于等于len值
 * @param len[IN] 待读取的数据长度最大长度, 不含\0
 * @return int > 0 实际读取的长度, <=0 失败
 */
int luat_fskv_get(const char* key, void* data, size_t len);

/**
 * @brief 清空所有数据
 * @return int == 0 正常 != 0失败
 */
int luat_fskv_clear(void);

int luat_fskv_stat(size_t *using_sz, size_t *max_sz, size_t *kv_count);

int luat_fskv_next(char* buff, size_t offset);

/**
 * @}
 */

#endif
