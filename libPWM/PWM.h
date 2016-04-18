#ifndef _MY_LIBPWM_H_
#define _MY_LIBPWM_H_

#include <stm32f10x.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_dma.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_rcc.h>
#include <misc.h>

#include "PWM_cfg.h"

typedef u32 PWM_group_t;
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP1)
#define PWM_group_1    1
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP2)
#define PWM_group_2    2
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP3)
#define PWM_group_3   3
#endif

typedef u32 PWM_channel;
#define PWM_Ch1 0x0001
#define PWM_Ch2 0x0002
#define PWM_Ch3 0x0004
#define PWM_Ch4 0x0008
#define PWM_Ch5 0x0010
#define PWM_Ch6 0x0020
#define PWM_Ch7 0x0040
#define PWM_Ch8 0x0080
#define PWM_Ch9 0x0100
#define PWM_Ch10 0x0200
#define PWM_Ch11 0x0400
#define PWM_Ch12 0x0800
#define PWM_Ch13 0x1000
#define PWM_Ch14 0x2000
#define PWM_Ch15 0x4000
#define PWM_Ch16 0x8000

/* Interfaces */
/* 
 * Low level initialization
 *  Pay attention to NVIC settings!!!
 *  Return 0 when success
 */
int PWMLowLevelInit(PWM_group_t group, GPIO_TypeDef *GPIO, PWM_channel channel);
/* Apply a PWM channel */
void PWMApply(PWM_group_t group, PWM_channel channel, u16 h_length, u16 l_length, u16 init_phace, int number_of_pulses);
/* Start PWM channel */
int PWMStart(PWM_group_t group, PWM_channel channel);
/* Stop PWM channel */
void PWMStop(PWM_group_t group, PWM_channel channel);
/* Get rest pulses of one channel */
u32 GetRestPulse(PWM_group_t group, PWM_channel channel);

#endif
