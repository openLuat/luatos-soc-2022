/*
 * Copyright (c) 2023 OpenLuat & AirM2M
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
#ifndef INCLUDE_LUAT_CAMERA_H_
#define INCLUDE_LUAT_CAMERA_H_
#include "luat_base.h"
#include "common_api.h"

typedef struct
{
	size_t  camera_speed;			//提供给camera时钟频率
    uint16_t sensor_width;			//camera的最大宽度
    uint16_t sensor_height;			//camera的最大高度
    uint16_t one_buf_height;		//1个接收缓存的高度，接收缓存大小=sensor_width * one_buf_height * (1 or 2，only_y=1), 底层根据实际情况会做调整，从而修改这个值
    uint8_t only_y;
	uint8_t rowScaleRatio;
	uint8_t colScaleRatio;
	uint8_t scaleBytes;
	uint8_t spi_mode;
	uint8_t is_msb;	//0 or 1;
	uint8_t is_two_line_rx; //0 or 1;
	uint8_t seq_type;	//0 or 1
} luat_spi_camera_t;

/**
 * @brief 配置camera并且初始化camera
 * @param id camera接收数据总线ID，ec618上有2条，0和1
 * @param conf camera相关配置
 * @param callback camera接收中断回调，注意这是在中断里的回调
 * @param param 中断回调时用户的参数
 * @return 0成功，其他失败
 */
int luat_camera_setup(int id, luat_spi_camera_t *conf, void *callback, void *param);

/**
 * @brief 开始接收camera数据
 * @param id camera接收数据总线ID
 * @return 0成功，其他失败
 */
int luat_camera_start(int id);

/**
 * @brief 停止接收camera数据
 * @param id camera接收数据总线ID
 * @return 0成功，其他失败
 */
int luat_camera_stop(int id);

/**
 * @brief 关闭camera并且释放资源
 * @param id camera接收数据总线ID
 * @return 0成功，其他失败
 */
int luat_camera_close(int id);

#endif /* INCLUDE_LUAT_CAMERA_H_ */
