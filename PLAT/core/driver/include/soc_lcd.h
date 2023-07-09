/*
 * soc_lcd.h
 *
 *  Created on: 2023年7月6日
 *      Author: Administrator
 */

#ifndef DRIVER_INCLUDE_SOC_LCD_H_
#define DRIVER_INCLUDE_SOC_LCD_H_
#include "common_api.h"
typedef struct
{
	uint32_t Speed;
	uint32_t x1;
	uint32_t y1;
	uint32_t x2;
	uint32_t y2;
	uint32_t xoffset;
	uint32_t yoffset;
	uint32_t Size;
	uint32_t DCDelay;
	uint8_t *Data;
	uint8_t SpiID;
	uint8_t Mode;
	uint8_t CSPin;
	uint8_t DCPin;
	uint8_t ColorMode;
}LCD_DrawStruct;

uint32_t LCD_DrawCacheLen(void);
void LCD_Draw(LCD_DrawStruct *Draw);
void LCD_DrawBlock(LCD_DrawStruct *Draw);
void LCD_CameraDraw(LCD_DrawStruct *Draw);

#endif /* DRIVER_INCLUDE_SOC_LCD_H_ */
