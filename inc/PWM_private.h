/*
 * 第二版：2015.5
 * ·使用结构体储存PWM通道信息，而不是单纯用数组
 ******
 * 第三版：2015.7.2
 * ·维持原样
 */

//定义PWM的一些数据结构
#ifndef _PWM_PRIVATE_H_
#define _PWM_PRIVATE_H_

#include "stm32f10x.h"

/* 记录每一个通道的PWM信息 */
//其实是一个链表
typedef struct tag_pwm{
	//注册一个通道所填信息
	u16 HLength_apply;
	u16 LLength_apply;
	u16 FirstPhace_apply;
	u16 NumberOfPulse_apply;
	
	//通道运行时所用的信息
	u16 HLength;
	u16 LLength;
	u32 Phace;
	u16 NumberOfPulse;
	
	//构成链表（下一个节点的序号）
	u16 NextChannel;
} PWMInfo;

#endif
