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

/*
 * PWM操作
 * 
 */
#include "common_api.h"
#include "FreeRTOS.h"
#include "task.h"

#include "luat_base.h"
#ifdef __LUATOS__
#include "luat_malloc.h"
#include "luat_msgbus.h"
#include "luat_timer.h"
#endif
#include "luat_pwm.h"

#include "timer.h"
#include "hal_adc.h"
#include "ic.h"
#include "hal_trim.h"
#include "timer.h"
#include "ec618.h"
#include "clock.h"
#include "pad.h"
#include "luat_debug.h"
#include "RTE_Device.h"
#define EIGEN_TIMER(n)             ((TIMER_TypeDef *) (AP_TIMER0_BASE_ADDR + 0x1000*n))

static signed char if_initialized_timer(const int channel)
{
   if( 0 != EIGEN_TIMER(channel)->TCTLR )
    {

        return -1;
    }

    return 0;
}

/*设置PWM脉冲个数*/
static int g_s_pnum_set[6] = {0};

PLAT_PA_RAMCODE volatile static void Timer_ISR()
{
    volatile static int update[6] = {0};/*当前PWM个数*/
    volatile static int i = 0;
    for(i = 0;i < 6;i++)
    {
        if (TIMER_getInterruptFlags(i) & TIMER_MATCH2_INTERRUPT_FLAG)
        {
            TIMER_clearInterruptFlags(i, TIMER_MATCH2_INTERRUPT_FLAG);
            update[i]++;
            if (update[i] >= g_s_pnum_set[i])
            {
                TIMER_stop(i);
                update[i] = 0;
                //TIMER_updatePwmDutyCycle(i,0);
                //LUAT_DEBUG_PRINT("PWM STOP %d",update[i]);
            }
        }
    }
}

/*最高频率应是26M*/ 
#define MAX_FREQ (26*1000*1000)

int luat_pwm_open(int channel, size_t freq,  size_t pulse, int pnum) {

    TimerConfig_t timerConfig;
    unsigned int clockId,clockId_slect;
    IRQn_Type time_req;

    PadConfig_t config = {0};
    TimerPwmConfig_t pwmConfig = {0};

    // LUAT_DEBUG_PRINT("luat_pwm_open channel:%d perio:%d pulse:%d pnum:%d",channel,period,pulse,pnum);
    if ( channel > 5 || channel < 0)
        return -1;
    if (freq > MAX_FREQ || freq < 0)
        return -2;
    if (pulse > 100)
        pulse = 100;
    else if (pulse < 0)
        pulse = 0;

    if(if_initialized_timer(channel) < 0)
    {
        return -4;   // hardware timer is used
    }

    switch(channel)
    {
        case  0:
            clockId =  FCLK_TIMER0;
            clockId_slect = FCLK_TIMER0_SEL_26M;
            time_req = PXIC0_TIMER0_IRQn;
            g_s_pnum_set[0] = pnum;
            break;
        case  1:
            clockId =  FCLK_TIMER1;
            clockId_slect = FCLK_TIMER1_SEL_26M;
            time_req = PXIC0_TIMER1_IRQn;
            g_s_pnum_set[1] = pnum;
            break;
        case  2:
            clockId =  FCLK_TIMER2;
            clockId_slect = FCLK_TIMER2_SEL_26M;
            time_req = PXIC0_TIMER2_IRQn;
            g_s_pnum_set[2] = pnum;
            break;
        case  3:
            clockId =  FCLK_TIMER3;
            clockId_slect = FCLK_TIMER3_SEL_26M;
            time_req = PXIC0_TIMER3_IRQn;
            g_s_pnum_set[3] = pnum;
            break;
        case  4:
            clockId =  FCLK_TIMER4;
            clockId_slect = FCLK_TIMER4_SEL_26M;
            time_req = PXIC0_TIMER4_IRQn;
            g_s_pnum_set[4] = pnum;
            break;
        case  5:
            clockId =  FCLK_TIMER5;
            clockId_slect = FCLK_TIMER5_SEL_26M;
            time_req = PXIC0_TIMER5_IRQn;
            g_s_pnum_set[5] = pnum;
            break;
        default :
            break;
    }
    // LUAT_DEBUG_PRINT("luat_pwm_open get timer_id %d",timer_id);
    PAD_getDefaultConfig(&config);
    config.mux = PAD_MUX_ALT5;
    
    
#if RTE_PWM0
    if(0 == channel){
        PAD_setPinConfig(RTE_PWM0, &config);   
    }
#endif
#if RTE_PWM1
    if(1 == channel){
        PAD_setPinConfig(RTE_PWM1, &config);
    }
#endif
#if RTE_PWM2
    if(2 == channel){
        PAD_setPinConfig(RTE_PWM2, &config);
    }
#endif
#if RTE_PWM3
    if(3 == channel){
        PAD_setPinConfig(RTE_PWM3, &config);
    }
#endif
#if RTE_PWM4
    if(4 == channel){
        PAD_setPinConfig(RTE_PWM4, &config);
    }
#endif
#if RTE_PWM5
    if( 5 == channel){
        PAD_setPinConfig(RTE_PWM5, &config);
    }
#endif
    CLOCK_setClockSrc(clockId, clockId_slect);
    CLOCK_setClockDiv(clockId, 1);
    TIMER_driverInit();

    pwmConfig.pwmFreq_HZ = freq;
    pwmConfig.srcClock_HZ = GPR_getClockFreq(clockId);  
    pwmConfig.dutyCyclePercent = pulse;
    TIMER_setupPwm(channel, &pwmConfig);
    if(0 != pnum)
    {
        TIMER_interruptConfig(channel, TIMER_MATCH0_SELECT, TIMER_INTERRUPT_DISABLED);
        TIMER_interruptConfig(channel, TIMER_MATCH1_SELECT, TIMER_INTERRUPT_DISABLED);
        TIMER_interruptConfig(channel, TIMER_MATCH2_SELECT, TIMER_INTERRUPT_LEVEL);

        XIC_SetVector(time_req,Timer_ISR);
        XIC_EnableIRQ(time_req);
    }

    TIMER_start(channel);

    return 0;
}

int luat_pwm_setup(luat_pwm_conf_t* conf)
{
    return luat_pwm_open(conf->channel,conf->period,conf->pulse,conf->pnum);
}

int luat_pwm_capture(int channel,int freq)
{
    return EIGEN_TIMER(channel)->TMR[0];
}

int luat_pwm_close(int channel)
{
    TIMER_stop(channel);
    return 0;
}

