//用户接口
#ifndef _MY_PWM_H_
#define _MY_PWM_H_

#define _PWM_VER_3_

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_dma.h"


/* 底层初始化 */
void PWMLowLevelInit(GPIO_TypeDef *GPIO_Port);

/* 注册一个通道 */
void PWMChannelApply(
				u16 Channel,
				u16 HLength,
				u16 LLength, 
				u16 Phace,
				u16 number_of_pulse
		);

/* 开始输出波形 */
void PWMStart(void);

/* 查询PWM剩余个数 */
int 	PWMCheckRest(int channel);

/* 强行停止PWM输出 */
void PWMStopAll(void);

/* 停止某一个通道的输出 */
void PWMStop(int channel);

/*
 * 第二版：2015.5
 * 开放给用户的写GPIO端口的函数
 * 不要用库函数的GPIO_SetBits和GPIO_ResetBits !!!
 ******
 * 第三版：2015.7.2
 * ···特别注意，第三版不需要这些东西。。。
 */
#ifndef _PWM_VER_3_
void PWMSetBits(u16 GPIO_Pin);
void PWMResetBits(u16 GPIO_Pin);
#endif

#endif
