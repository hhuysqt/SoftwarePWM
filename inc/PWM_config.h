//用户定义的一些参数：
#ifndef _PWM_CONFIG_H_
#define _PWM_CONFIG_H_

/* 需要输出的GPIO口，要开启时钟 */
#define RCC_APB2Periph_GPIO_Out RCC_APB2Periph_GPIOA 

/* 设置：分度值 */
//定时器的分频数，越大就越慢
//72M的主频下：
/* 分频数为72，即定时器时钟频率为1M，高低电平长度的单位是1us */
#define MAX_PRESCALER 72
/* 10us */
// #define MAX_PRESCALER 720
/* 100us */
// #define MAX_PRESCALER 7200
/* 1ms */
// #define MAX_PRESCALER 72000


//PWM缓冲区的大小；越小，实时性越强，但是CPU中断越频繁
#define PWM_BUFFER 32

/**
	* 如果注册了4个端口，
	* 处理器大约要花6000个机器周期，来计算每一轮的16组数据，
	* 因此PWM一周期时长最好不要短于15us，否则波形会混乱
	* 而且，如果设置频率太高，CPU会忙于计算PWM数据而做不了其他事。。。
	*/

#endif
