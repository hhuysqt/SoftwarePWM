/*
 * configure PWM
 */
#ifndef _MY_PWM_CFG_H_
#define _MY_PWM_CFG_H_

/* 
 * PWM DMA group configure:
 *
 *  val  DMA usage               TIMx usage
 *   1   DMA1_ch1 && DMA1_ch7    TIM2
 *   2   DMA1_ch2 && DMA1_ch3    TIM3
 *   3   DMA1_ch4 && DMA1_ch5    TIM4
 */
#define PWMDMA_GROUP1     1
#define PWMDMA_GROUP2     2
#define PWMDMA_GROUP3     4

#define USE_PWMDMA_GROUP (PWMDMA_GROUP1 | PWMDMA_GROUP2 | PWMDMA_GROUP3)

/*
 * buffer size
 *  A smaller value could have better real-time performance, 
 *  with more CPU interrupts.
 */
#define PWM_BUFFER_SIZE  16

/*
 * PWM scale
 *  Set TIM prescaler
 *  Assume that MCU runs at 72MHz
 */
/* 1 us */
#define TIM_PRESCALER 72
/* 10 us */
//#define TIM_PRESCALER 720
/* 100 us */
//#define TIM_PRESCALER 7200

#endif
