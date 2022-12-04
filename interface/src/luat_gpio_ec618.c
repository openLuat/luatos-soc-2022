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
#include "luat_gpio.h"
#include "driver_gpio.h"
#include "slpman.h"
#include "FreeRTOS.h"
__attribute__((weak)) int luat_gpio_irq_default(int pin, void* args)
{
	return 0;
}

static int luat_gpio_irq_callback(void *ptr, void *pParam)
{
    int pin = (int)ptr;
    luat_gpio_irq_default(pin, (void*)luat_gpio_get(pin));
    return 0;
}

void luat_gpio_set_default_cfg(luat_gpio_cfg_t* gpio)
{
	memset(gpio, 0, sizeof(luat_gpio_cfg_t));
}

int luat_gpio_open(luat_gpio_cfg_t* gpio)
{
	if (((uint32_t)(gpio->pin)) > (HAL_GPIO_MAX + 1)) return -1;
	GPIO_GlobalInit(NULL);
	if (gpio->pin >= HAL_GPIO_MAX)
	{
		APmuWakeupPadSettings_t padConfig = {0};
		if (LUAT_GPIO_OUTPUT == gpio->mode) return -1;
		uint8_t pad_id = (gpio->pin > HAL_GPIO_MAX)?2:0;
		switch (gpio->pull)
		{
		case LUAT_GPIO_PULLUP:
			padConfig.pullUpEn = 1;
			break;
		case LUAT_GPIO_PULLDOWN:
			padConfig.pullDownEn = 1;
			break;
		default:
			break;
		}
		if (LUAT_GPIO_IRQ == gpio->mode)
		{
			if (gpio->irq_cb) {
				GPIO_ExtiSetCB(gpio->pin, gpio->irq_cb, gpio->irq_args);
			}
			else
			{
				GPIO_ExtiSetCB(gpio->pin, luat_gpio_irq_callback, gpio->irq_args);
			}
			switch (gpio->irq_type)
			{
			case LUAT_GPIO_RISING_IRQ:
				padConfig.posEdgeEn = 1;
				break;
			case LUAT_GPIO_FALLING_IRQ:
				padConfig.negEdgeEn = 1;
				break;
			case LUAT_GPIO_BOTH_IRQ:
				padConfig.posEdgeEn = 1;
				padConfig.negEdgeEn = 1;
				break;
			default:
				return -1;
				break;
			}
			apmuSetWakeupPadCfg(pad_id, true, &padConfig);
			NVIC_EnableIRQ(pad_id);
		}
		else
		{
			NVIC_DisableIRQ(pad_id);
			apmuSetWakeupPadCfg(pad_id, false, &padConfig);
			GPIO_ExtiSetCB(gpio->pin, NULL, gpio->irq_args);
		}
		return 0;
	}

	uint8_t is_pull;
	uint8_t is_pullup;
	uint8_t is_input = (LUAT_GPIO_OUTPUT == gpio->mode)?0:1;
    switch (gpio->pull)
    {
	case LUAT_GPIO_PULLUP:
		is_pull = 1;
		is_pullup = 1;
		break;
	case LUAT_GPIO_PULLDOWN:
		is_pull = 1;
		is_pullup = 0;
		break;
	default:
		is_pull = 0;
		is_pullup = 0;
		break;
    }
    GPIO_Config(gpio->pin, is_input, gpio->output_level);
    GPIO_PullConfig(GPIO_ToPadEC618(gpio->pin, gpio->alt_fun), is_pull, is_pullup);

    if (LUAT_GPIO_IRQ == gpio->mode)
    {
        if (gpio->irq_cb) {
        	GPIO_ExtiSetCB(gpio->pin, gpio->irq_cb, gpio->irq_args);
        }
        else
        {
        	GPIO_ExtiSetCB(gpio->pin, luat_gpio_irq_callback, gpio->irq_args);
        }
        switch (gpio->irq_type)
        {
		case LUAT_GPIO_RISING_IRQ:
			GPIO_ExtiConfig(gpio->pin, 0,1,0);
			break;
		case LUAT_GPIO_FALLING_IRQ:
			GPIO_ExtiConfig(gpio->pin, 0,0,1);
			break;
		case LUAT_GPIO_BOTH_IRQ:
			GPIO_ExtiConfig(gpio->pin, 0,1,1);
			break;
		case LUAT_GPIO_HIGH_IRQ:
			GPIO_ExtiConfig(gpio->pin, 1,1,0);
			break;
		case LUAT_GPIO_LOW_IRQ:
			GPIO_ExtiConfig(gpio->pin, 1,0,1);
			break;
		default:
			GPIO_ExtiConfig(gpio->pin, 0,0,0);
			break;
        }

    }
    else
    {
    	GPIO_ExtiConfig(gpio->pin, 0,0,0);
    	GPIO_ExtiSetCB(gpio->pin, NULL, NULL);
    }

    GPIO_IomuxEC618(GPIO_ToPadEC618(gpio->pin, gpio->alt_fun), gpio->alt_fun, 0, 0);
    return 0;
}

int luat_gpio_setup(luat_gpio_t *gpio){
	if (((uint32_t)(gpio->pin)) > (HAL_GPIO_MAX + 1)) return -1;
	GPIO_GlobalInit(NULL);
	if (gpio->pin >= HAL_GPIO_MAX)
	{
		APmuWakeupPadSettings_t padConfig = {0};
		if (LUAT_GPIO_OUTPUT == gpio->mode) return -1;
		uint8_t pad_id = (gpio->pin > HAL_GPIO_MAX)?2:0;
	    switch (gpio->pull)
	    {
		case LUAT_GPIO_PULLUP:
			padConfig.pullUpEn = 1;
			break;
		case LUAT_GPIO_PULLDOWN:
			padConfig.pullDownEn = 1;
			break;
		default:
			break;
	    }
	    if (LUAT_GPIO_IRQ == gpio->mode)
	    {
	        if (gpio->irq_cb) {
	        	GPIO_ExtiSetCB(gpio->pin, gpio->irq_cb, gpio->irq_args);
	        }
	        else
	        {
	        	GPIO_ExtiSetCB(gpio->pin, luat_gpio_irq_callback, gpio->irq_args);
	        }
	        switch (gpio->irq)
	        {
			case LUAT_GPIO_RISING_IRQ:
				padConfig.posEdgeEn = 1;
				break;
			case LUAT_GPIO_FALLING_IRQ:
				padConfig.negEdgeEn = 1;
				break;
			case LUAT_GPIO_BOTH_IRQ:
				padConfig.posEdgeEn = 1;
				padConfig.negEdgeEn = 1;
				break;
			default:
		    	return -1;
				break;
	        }
	        apmuSetWakeupPadCfg(pad_id, true, &padConfig);
	        NVIC_EnableIRQ(pad_id);
	    }
	    else
	    {
	    	NVIC_DisableIRQ(pad_id);
	    	apmuSetWakeupPadCfg(pad_id, false, &padConfig);
	    	GPIO_ExtiSetCB(gpio->pin, NULL, gpio->irq_args);
	    }
		return 0;
	}
	uint8_t is_pull;
	uint8_t is_pullup;
	uint8_t is_input = (LUAT_GPIO_OUTPUT == gpio->mode)?0:1;
    switch (gpio->pull)
    {
	case LUAT_GPIO_PULLUP:
		is_pull = 1;
		is_pullup = 1;
		break;
	case LUAT_GPIO_PULLDOWN:
		is_pull = 1;
		is_pullup = 0;
		break;
	default:
		is_pull = 0;
		is_pullup = 0;
		break;
    }
    GPIO_Config(gpio->pin, is_input, is_pullup);
    GPIO_PullConfig(GPIO_ToPadEC618(gpio->pin, 0), is_pull, is_pullup);
    if (LUAT_GPIO_IRQ == gpio->mode)
    {
        if (gpio->irq_cb) {
        	GPIO_ExtiSetCB(gpio->pin, gpio->irq_cb, gpio->irq_args);
        }
        else
        {
        	GPIO_ExtiSetCB(gpio->pin, luat_gpio_irq_callback, gpio->irq_args);
        }
        switch (gpio->irq)
        {
		case LUAT_GPIO_RISING_IRQ:
			GPIO_ExtiConfig(gpio->pin, 0,1,0);
			break;
		case LUAT_GPIO_FALLING_IRQ:
			GPIO_ExtiConfig(gpio->pin, 0,0,1);
			break;
		case LUAT_GPIO_BOTH_IRQ:
			GPIO_ExtiConfig(gpio->pin, 0,1,1);
			break;
		case LUAT_GPIO_HIGH_IRQ:
			GPIO_ExtiConfig(gpio->pin, 1,1,0);
			break;
		case LUAT_GPIO_LOW_IRQ:
			GPIO_ExtiConfig(gpio->pin, 1,0,1);
			break;
		default:
			GPIO_ExtiConfig(gpio->pin, 0,0,0);
			break;
        }

    }
    else
    {
    	GPIO_ExtiConfig(gpio->pin, 0,0,0);
    	GPIO_ExtiSetCB(gpio->pin, NULL, NULL);
    }

    GPIO_IomuxEC618(GPIO_ToPadEC618(gpio->pin, 0), 0, 0, 0);
	return 0;
}

int luat_gpio_set(int pin, int level){
    if (((uint32_t)(pin)) >= HAL_GPIO_MAX) return -1;
    GPIO_Output(pin, level);
    return 0;
}

int luat_gpio_get(int pin){
	if (((uint32_t)(pin)) > HAL_GPIO_MAX + 1) return 0;
    uint8_t re;
    if (pin >= HAL_GPIO_MAX)
    {
    	if (pin > HAL_GPIO_MAX)
    	{
    		re = slpManGetWakeupPinValue() & (1 << 2);
    	}
    	else
    	{
    		re = slpManGetWakeupPinValue() & (1 << 0);
    	}
    }
//    else if (pin >= HAL_GPIO_20 && pin <= HAL_GPIO_22)
//    {
//		uint8_t pad_id = pin - 17;
//		if (NVIC_GetEnableIRQ(pad_id))
//		{
//			re = slpManGetWakeupPinValue() & (1 << (pin - 17));
//		}
//		else
//		{
//			re = GPIO_Input(pin);
//		}

//    }
    else
    {
    	re = GPIO_Input(pin);
    }
    return re?1:0;
}

void luat_gpio_close(int pin){
    if (pin > (HAL_GPIO_MAX + 1)) return ;
    GPIO_ExtiSetCB(pin, NULL, 0);
    GPIO_ExtiConfig(pin, 0,0,0);
    return ;
}
int luat_gpio_set_irq_cb(int pin, luat_gpio_irq_cb cb, void* args)
{
	if (pin > (HAL_GPIO_MAX + 1)) return -1;
	GPIO_ExtiSetCB(pin, cb, args);
	return 0;
}

void luat_gpio_pulse(int pin, uint8_t *level, uint16_t len, uint16_t delay_ns)
{
	GPIO_OutPulse(pin, level, len, delay_ns);
}

int luat_gpio_ctrl(int pin, LUAT_GPIO_CTRL_CMD_E cmd, int param)
{
	if (pin > (HAL_GPIO_MAX + 1)) return -1;
	uint8_t alt_fun = (param >> 28);
	switch(cmd)
	{
	case LUAT_GPIO_CMD_SET_PULL_MODE:
		switch(param)
		{
		case LUAT_GPIO_PULLUP:
			GPIO_Config(GPIO_ToPadEC618(pin, alt_fun), 1, 1);
			break;
		case LUAT_GPIO_PULLDOWN:
			GPIO_Config(GPIO_ToPadEC618(pin, alt_fun), 1, 0);
			break;
		default:
			GPIO_Config(GPIO_ToPadEC618(pin, alt_fun), 0, 0);
			break;
		}
		break;
	case LUAT_GPIO_CMD_SET_IRQ_MODE:
		switch(param)
        {
		case LUAT_GPIO_RISING_IRQ:
			GPIO_ExtiConfig(pin, 0,1,0);
			break;
		case LUAT_GPIO_FALLING_IRQ:
			GPIO_ExtiConfig(pin, 0,0,1);
			break;
		case LUAT_GPIO_BOTH_IRQ:
			GPIO_ExtiConfig(pin, 0,1,1);
			break;
		case LUAT_GPIO_HIGH_IRQ:
			GPIO_ExtiConfig(pin, 1,1,0);
			break;
		case LUAT_GPIO_LOW_IRQ:
			GPIO_ExtiConfig(pin, 1,0,1);
			break;
		default:
			GPIO_ExtiConfig(pin, 0,0,0);
			break;
        }
		break;
	default:
		return -1;
	}
	return 0;
}
