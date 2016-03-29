/***********************************************************************
 *
 * 第二版：2015.5
 * ·改进排序算法，用链表大大减少每次寻找虽小相位值的时间
 * ·利用双缓冲，使输出波形稳定
 *
 ******
 *
 * 第三版：2015.7.2
 * ·原来PWM输出，用DMA直接写到GPIO的ODR上，改作：写到BSRR寄存器上
 * ·用BSRR可以避免占用其他的端口
 *
 ***********************************************************************/
 
#include "PWM.h"
#include "PWM_Config.h"
#include "PWM_private.h"

#define IMPROVED_SORT

//一组PWM通道的个数
#define PWM_GROUP 16
//缓冲时间
#define buffertime 600
//空闲时间
#define IDELTIME	0xffff
//自己定义的空指针，为-1。。。
#define NULL_p 0xffff

/* ========================================================================================= */
/* 私有变量 */

//注册的GPIO口
static GPIO_TypeDef *PWM_GPIO;

//记录注册的端口
static u32 ApplyPins = 0;

//记录正在使用的端口(当高16位都为0时，是正常使用；高16位置位了，标志着完成了输出。)
	/* 当完成输出时，不再需要中断，
	但是由于使用了双缓冲，可能还有半个缓冲区的波形没有输出，
	于是利用高16位标志此事 */
static u32 UsingPins = 0;

//要写到ODR的数据
static u32 ODRData = 0;	//输出电平
static u32 BSRRData = 0;//生成输出电平队列

//要写到ARR的数据
static u32 ARRData = 0;	//定时器刷新时间的队列

#define PhaceMax (1L <<19)
//相位数据会溢出；做好标志，若溢出就减去PhaceMax
static u32 overflow_flag = 0;

//所有PWM的信息
//最终要构造一个链表
static PWMInfo PWM_ALL[PWM_GROUP] = { 0 };	
//链表头（以数组的下标代替），-1表示空。。。
static s16	PWM_ALL_head = NULL_p;

//以下是两个队列，给DMA搬数据；其大小在PWM_Config.h中设置

//定时器自动重装寄存器(ARR)的队列
static u16 TIM_ARR_Queue[2][PWM_BUFFER] = { 0 };

//IO口电平改变寄存器(BSRR, Bit Set/Reset Register)的队列（之前用的是ODR）
static u32 IO_BSRR_Queue[2][PWM_BUFFER]= { 0 };

//利用双缓冲；此刻要计算的队列的指针
static u16	*TIM_Queue = TIM_ARR_Queue[0]; 
static u32	*BSRR_Queue = IO_BSRR_Queue[0];




/* =========================================================================== */
/* 私有函数 */
#define CalculateQueues DMA1_Channel5_IRQHandler
//其实是中断函数
void CalculateQueues(void);




/* =========================================================================== */
/* 接口函数 */

/*
 * 底层初始化
 * ·IO口
 * ·定时器（已开启）
 * ··从此以后，每隔一段时间CPU会中断
 */
void PWMLowLevelInit(GPIO_TypeDef* GPIO_Port){
	GPIO_InitTypeDef PWMIO;
	
	PWM_GPIO = GPIO_Port;	//记录端口
	//初始化GPIO
	{
		//暂时只写A,B,C三个端口
		switch((u32)GPIO_Port)
		{
			case (u32)GPIOA:
				RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
				break;
			case (u32)GPIOB:
				RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
				break;
			case (u32)GPIOC:
				RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
				break;
		}
		
		/* 要改进，不能一刀切 */
		//先取消此端口之前的功能
		GPIO_DeInit(GPIO_Port);
		//然后统统定为推挽输出
		PWMIO.GPIO_Pin =		GPIO_Pin_All;
		PWMIO.GPIO_Mode =		GPIO_Mode_Out_PP;
		PWMIO.GPIO_Speed =	GPIO_Speed_50MHz;
		GPIO_Init(GPIO_Port,&PWMIO);
	}
	
	//初始化USART
	//MyUSARTInit();
	
	//初始化两个队列
	{
		//两个队列都放了一些空闲时候的数据，保证隔一段时间CPU获得一次中断
		int n;
		//双缓冲队列，所以要 "<<1"
		for(n = 0; n < (PWM_BUFFER<<1); n++)
		{
			((u16*)TIM_ARR_Queue)[n] = buffertime;
			((u16*)IO_BSRR_Queue)[n] = BSRRData;
		}
	}
	
	//初始化DMA
	{
		DMA_InitTypeDef DMAS;
		
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
		/* 用到了DMA1的通道5、7，分别是
			TIM2_CC1匹配，TIM2_CC2匹配
			的触发通道；*/
		DMAS.DMA_BufferSize = PWM_BUFFER << 1;	//使用双缓冲
		DMAS.DMA_DIR = DMA_DIR_PeripheralDST;
		DMAS.DMA_M2M = DMA_M2M_Disable;
		DMAS.DMA_Mode = DMA_Mode_Circular;
		DMAS.DMA_Priority = DMA_Priority_High;

		DMAS.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
		DMAS.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;

		//通道5：TIM2_CC1匹配触发
		//用于更新TIM2_ARR寄存器
		DMA_DeInit(DMA1_Channel5);
		DMAS.DMA_MemoryBaseAddr = (u32)TIM_ARR_Queue;
		DMAS.DMA_MemoryInc = DMA_MemoryInc_Enable;
		DMAS.DMA_PeripheralBaseAddr = (u32)&TIM2->ARR;
		DMAS.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
		DMA_Init(DMA1_Channel5, &DMAS);
		DMA_Cmd(DMA1_Channel5, ENABLE);
		//通道5要使能传输完成的中断：双缓冲，有传输过半和传输完成的中断
		DMA_ITConfig(DMA1_Channel5, DMA_IT_TC | DMA_IT_HT, ENABLE);
		
		//通道7：TIM2_CC2匹配触发
		//用于更新IO口
		DMA_DeInit(DMA1_Channel7);
		DMAS.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;					//32位
		DMAS.DMA_MemoryBaseAddr = (u32)IO_BSRR_Queue;
		DMAS.DMA_MemoryInc = DMA_MemoryInc_Enable;
		DMAS.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;	//32位
		DMAS.DMA_PeripheralBaseAddr = (u32)&PWM_GPIO->BSRR;					//改成了BSRR
		DMAS.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
		DMA_Init(DMA1_Channel7, &DMAS);
		DMA_Cmd(DMA1_Channel7, ENABLE);
	}
	
	//初始化NVIC
	//使能DMA通道5的中断
	{
		NVIC_InitTypeDef DMA_IRQ;
		
		NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
		DMA_IRQ.NVIC_IRQChannel = DMA1_Channel5_IRQn;
		DMA_IRQ.NVIC_IRQChannelPreemptionPriority = 1;
		DMA_IRQ.NVIC_IRQChannelSubPriority = 0;
		DMA_IRQ.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&DMA_IRQ);
	}
	
	//初始化定时器2
	{
		TIM_TimeBaseInitTypeDef PWMTIM;
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
		//先取消其原来的设置
		TIM_DeInit(TIM2);
		//再继续设置
		PWMTIM.TIM_Prescaler =			MAX_PRESCALER - 1;	//TIM输入时钟分频
		PWMTIM.TIM_CounterMode =		TIM_CounterMode_Up;
		PWMTIM.TIM_Period =					buffertime;	//且作为缓冲时间
		PWMTIM.TIM_RepetitionCounter = 0;
		PWMTIM.TIM_ClockDivision =  0;
		TIM_TimeBaseInit(TIM2, &PWMTIM);
	}
	//使能定时器触发DMA
	TIM_SelectCCDMA(TIM2, ENABLE);
	TIM_DMACmd(TIM2, TIM_DMA_CC1 | TIM_DMA_CC2, ENABLE);
	//开启定时器
//	TIM_Cmd(TIM2, ENABLE);
}

/*
 * 注册一个PWM端口
 * 输入：序号，高、低电平长度，初相，个数
 * ·当个数输入为0时，代表无穷个
 * `随后还要用PWMStart()才能开启输出
 */
void PWMChannelApply(
				u16 Channel,
				u16 HLength,
				u16 LLength, 
				u16 Phace,
				u16 number_of_pulse
		)
{
	//记录数据
	PWM_ALL[Channel].HLength_apply = HLength;
	PWM_ALL[Channel].LLength_apply = LLength;
	PWM_ALL[Channel].FirstPhace_apply = Phace;
	PWM_ALL[Channel].NumberOfPulse_apply = number_of_pulse - 1;
	
	//记录注册情况
	ApplyPins |= (1L << Channel);
}


/*
 * 开始输出PWM
 * ·把端口按相位值排序，形成链表
 * ··计算队列的事，留给中断函数去做
 */
void PWMStart()
{
	u32 DMA_CCR_tmp;
	
	if(ApplyPins == 0)
	{
		return;
	}

	//屏蔽掉DMA5中断
	DMA_CCR_tmp = DMA1_Channel5->CCR;
	DMA1_Channel5->CCR &= (~(DMA_IT_HT | DMA_IT_TC));
	
	//初始化链表，给它排序
	/* 填入数据 */
	{
		register int n;
		for(n = 0; n < PWM_GROUP; n++)
		{
			if((ApplyPins >> n)&1)//如果端口注册了
			{
				PWM_ALL[n].HLength = PWM_ALL[n].HLength_apply;
				PWM_ALL[n].LLength = PWM_ALL[n].LLength_apply;
				PWM_ALL[n].NumberOfPulse = PWM_ALL[n].NumberOfPulse_apply;
				//初始化相位数据，需要与原来的数据在同一层次上，所以加上ARRData, buffertime
				PWM_ALL[n].Phace = PWM_ALL[n].FirstPhace_apply + ARRData + buffertime;
			}
		}
		//更新端口使用情况
		UsingPins &= 0x0000ffff;
		UsingPins |= ApplyPins;
		//取消注册消息
		ApplyPins = 0;
	}
	
	/* 排序，重新整理链表 */
	{
#ifndef IMPROVED_SORT
		int n;
		u32 max_tmp;
		u16 using_tmp = UsingPins;
		u16 biggest_num;
		
		PWM_ALL_here = NULL_p;
		while(using_tmp != 0){
			max_tmp = 0;
			for(n = 0; n < PWM_GROUP; n++)
			{
				if((using_tmp >> n)&1)
				{
					if(PWM_ALL[n].Phace > max_tmp)
					{
						max_tmp = PWM_ALL[n].Phace;
						biggest_num = n;
					}
				}
			}
			using_tmp &= (~(1L << biggest_num));
			PWM_ALL[biggest_num].NextChannel = PWM_ALL_here;
			PWM_ALL_here = biggest_num;
		}
		//重定头指针，此时它指向最小的链结
		PWM_ALL_head = PWM_ALL_here;
#else
		s8 order[16];
		register int n, i = 0;

		PWM_ALL_head = NULL_p;
		
		//为方便排序，将序号整理到一个数组里面(order)
		for(n = 0; n < PWM_GROUP; n++)
		{
			if((UsingPins >> n) & 1)
			{
				order[i] = n;
				i++;
			}
		}
		
		///*选择排序*/（少数据，不用什么堆排序、快排之类的了）
		//从大到小，最后得到的是最小的一块
		//顺便生成了链表
		for(i--; i >= 0; i--)
		{
			register int max = 0, max_num = 0;
			
			for(n = 0; n <= i; n++)
			{
				register int current_num = PWM_ALL[order[n]].Phace;
				if(current_num > max)
				{
					max = current_num;
					max_num = n;
				}
			}
			PWM_ALL[order[max_num]].NextChannel = PWM_ALL_head;
			PWM_ALL_head = order[max_num];
			order[max_num] = order[i];
		}
#endif
	}
			
	TIM_Cmd(TIM2, ENABLE);
	//恢复中断
	DMA1_Channel5->CCR = DMA_CCR_tmp;

	overflow_flag = ~UsingPins;	//
}


/*
 * 计算队列（其实是DMA中断程序）
 * ·通过不断从链表中提取PWM的最小相位值，计算ARR队列
 ******
 * ·第二版：不断对ODRData作异或，计算ODR队列
 ******
 * ·第三版：根据上一轮的端口电平，置位BSRRData的不同部位，生成BSRR的序列
						另外，当不再输出波形时，我关掉了定时器；为此加了一个判断。
						并且，用寄存器以提高速度；虽然在中断中做那么多东西不太好。。。
						不足：占用了10个寄存器，中断程序啊。。。
 */
void CalculateQueues()
{
	u32 tmp_arr;
	u32 n;
	
	if((UsingPins & 0xffff) == 0)
	{
		if(UsingPins == 0)
			/*虽然已经没有输出了，但是还有半个缓冲区的数据未输出；做好标志*/
			UsingPins = 1L << 16;
		else
		{
			/*已经完全没有输出了*/
			UsingPins = 0;
			//关掉定时器
			TIM_Cmd(TIM2, DISABLE);
		}
		
		{
			//没有输出了，要清空这一段输出区
			register int n;
			for(n = 0; n < PWM_BUFFER; n++)
			{
				TIM_Queue[n] = buffertime;
				BSRR_Queue[n] = 0;
			}
		}
		
		if(BSRRData != 0)
		{
			BSRR_Queue[0] = BSRRData;
			BSRRData = 0;
			//还有一位数据。。。这样的话还得再来一次，否则上面关掉定时器了，这一个数据就不能输出了
			//特别是，当输出脉冲个数是缓冲区大小的倍数时，会有”共振“效果产生，这样剩下的数据就迟迟出不来
			UsingPins = 0;
		}
	}//if usingpins == 0xXXXX0000
	else
	{
		for(n = 0; n < PWM_BUFFER; n++)
		{
			//填入ODR数据
			BSRR_Queue[n] = BSRRData;
			
			if(UsingPins == 0){
				BSRRData = 0;
				TIM_Queue[n] = buffertime;
				continue;
			}
			
			//从链表头抽出最小的相位值
			tmp_arr = PWM_ALL[PWM_ALL_head].Phace;
			//填入队列中
			TIM_Queue[n] = tmp_arr - ARRData;
			
			BSRRData = 0;
			//反复查看是否有相同的相位
			do
			{
				//先增加相位数值，然后将该链结插回队列

				//记录当前电平状态
				ODRData ^= (1L << PWM_ALL_head);
				if((ODRData >> PWM_ALL_head) & 1)
				{
					//高电平，置位BSRR的低16位，BSn
					BSRRData |= 1 << PWM_ALL_head;
					
					PWM_ALL[PWM_ALL_head].Phace += PWM_ALL[PWM_ALL_head].HLength;
				}//if ODRData.n == 1
				else
				{
					//低电平，置位BSRR的高16位，BRn
					BSRRData |= 1 << (PWM_ALL_head + 16);
					
					if(PWM_ALL[PWM_ALL_head].NumberOfPulse)		//剩余脉冲个数不为0
					{
						if(PWM_ALL[PWM_ALL_head].NumberOfPulse != 0xffff)	//如果不是-1(-1代表一直输出)
							PWM_ALL[PWM_ALL_head].NumberOfPulse --;	//减少一个波
					
						PWM_ALL[PWM_ALL_head].Phace += PWM_ALL[PWM_ALL_head].LLength;
					}
					else{		//没有剩余脉冲了
						UsingPins &= (~(1L << PWM_ALL_head));	//注销这一通道
						PWM_ALL_head = PWM_ALL[PWM_ALL_head].NextChannel;	//指向下一个链结，丢弃这一链结
						if(PWM_ALL_head == (s16)NULL_p)
							break;
						else
							continue;
					}
					
				}//if ODRData.n == 0
				
				
				/*防止溢出*/
				if(PWM_ALL[PWM_ALL_head].Phace > PhaceMax)
					overflow_flag |= (1L << PWM_ALL_head);
				
				if(!(~overflow_flag ))	//overflow_flag == 0xffff
				{
					u16 tmp_n = PWM_ALL_head;
					
					//每一个链结都减去PhaceMax
					while(tmp_n != NULL_p)
					{
						PWM_ALL[tmp_n].Phace -= PhaceMax;
						tmp_n = PWM_ALL[tmp_n].NextChannel;
					}

					overflow_flag = ~UsingPins;	//
				}//if overflag == 0xffff
				
				
				/* 将处理后的链结插回去 */
				//此处最好可以再作优化，此中断函数竟然用了10个寄存器，其中这一段占用很多。。。
				{
					register int n_t = PWM_ALL[PWM_ALL_head].NextChannel, n_b = PWM_ALL_head, t_h = PWM_ALL_head;
					register int tmp_arr2 = PWM_ALL[PWM_ALL_head].Phace;
					
					while(n_t!= NULL_p && (tmp_arr2 > PWM_ALL[n_t].Phace)){
						n_b = n_t;
						n_t = PWM_ALL[n_t].NextChannel;
					}

					//插入
					PWM_ALL[n_b].NextChannel = PWM_ALL_head;
					//指向下一个链结
					PWM_ALL_head = PWM_ALL[PWM_ALL_head].NextChannel;
					PWM_ALL[t_h].NextChannel = n_t;
				}//插入链结
				
			}while(tmp_arr == PWM_ALL[PWM_ALL_head].Phace);
			
			ARRData = tmp_arr;
			
		}//for n=0..16, 逐个扫描相位信息
		
	}//if usingpins != 0xXXXX0000

	if(*((u32*)0x42400044))
	{
		//重新开启传输过半中断
		*((u32*)0x42400b08) = 1;
		BSRR_Queue = IO_BSRR_Queue[0];
		TIM_Queue = TIM_ARR_Queue[0];
	}
	else 
	{
		//0x40020058.2，取消传输过半中断
		*((u32*)0x42400b08) = 0;
		BSRR_Queue = IO_BSRR_Queue[1];
		TIM_Queue = TIM_ARR_Queue[1];
	}
	//清除DMA_CH5所有中断标志
	DMA1->IFCR = DMA1_IT_GL5;
}

/* 查询PWM剩余个数 */
int 	PWMCheckRest(int channel)
{
	return PWM_ALL[channel].NumberOfPulse;
}


/* 强行停止所有的PWM输出 */
void PWMStopAll(void)
{
	TIM_Cmd(TIM2, DISABLE);
	
	{
		//没有输出了，要清空所有输出区
		register int n;
		for(n = 0; n < (PWM_BUFFER << 1); n++)
		{
			((u16*)TIM_ARR_Queue)[n] = buffertime;
			((u32*)IO_BSRR_Queue)[n] = 0;
		}
	}
	
	//DMA归位
	DMA_Cmd(DMA1_Channel5, DISABLE);
	DMA_Cmd(DMA1_Channel7, DISABLE);
	
	DMA1_Channel5->CNDTR = PWM_BUFFER << 1;
	DMA1_Channel7->CNDTR = PWM_BUFFER << 1; 
	
	DMA_Cmd(DMA1_Channel5, ENABLE);
	DMA_Cmd(DMA1_Channel7, ENABLE);
	
	PWM_ALL[1].NumberOfPulse=0;
	PWM_ALL[2].NumberOfPulse=0;
	PWM_ALL[3].NumberOfPulse=0;
}

/* 停止某一个通道 */
void PWMStop(int channel)
{
	register int n;
	register u32 *p = &IO_BSRR_Queue[1][PWM_BUFFER - 1];

	channel = ~( 1 << channel );
	
	//注销这一通道
	UsingPins &= channel;
	BSRRData = ( BSRRData & channel ) & ( channel << 16 );
	
	//清空这一通道的输出（主要是高电平的输出）
	for(n = 0; n < (PWM_BUFFER << 1); n++)
	{
		*p-- &= channel;
	}
	//最后还要把引脚拉低
	PWM_GPIO->BRR = (~channel);
	PWM_ALL[channel].NumberOfPulse=0;
}



