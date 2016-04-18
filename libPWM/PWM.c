#include "PWM.H"

#define PWMCHANNELS 16
#define TIM_ARR_STANDBY_VAR 15
#define ENTER_CRITICAL __asm("CPSID I");
#define EXIT_CRITICAL  __asm("CPSIE I");


/************************************
 *
 * private data structure 
 *
 ************************************/
#define MAX_HEAP_SIZE 18
#define HEAP_INFINATE 0xffffffff
#define H_PARENT(i) (i >> 1)
#define H_LEFT(i)   (i << 1)
#define H_RIGHT(i)  ((i << 1) + 1)
typedef union {
	u32 data[MAX_HEAP_SIZE];
	u32 size;
} heap_t;

typedef struct {
	/* double buffer */
	u32 gpio_bsrr_queue[PWMCHANNELS*2];
	u16 tim_arr_queue[PWMCHANNELS*2];
} pwm_dma_queue_t;

typedef struct {
	u16 h_length;
	u16 l_length;
	u32 init_phace;
	u32 number_of_pulses;
	u32 remaining_pulses;
}pwm_channel_t;

typedef struct {
	/* the applied channels */
	u16 applied_channels;
	/* the active channels */
	u16 active_channels;
	/* the minimum phace value */
	u32 phace_base;
	
	/* Detail channel info */
	pwm_channel_t channels[PWMCHANNELS];
	/* a heap of the phace values of all active channels */
	heap_t phace_heap;
	/* the queue for DMA request */
	pwm_dma_queue_t dma_queue;
} pwm_info_t;



/************************************
 *
 * private varibles 
 *
 ************************************/
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP1)
pwm_info_t PWM1;
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP2)
pwm_info_t PWM2;
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP3)
pwm_info_t PWM3;
#endif


/************************************
 *
 * Private functions
 *  Build and retain a min-heap
 *
 ************************************/
static __inline void HeapInit(heap_t *h)
{
	int i;
	for(i = 1; i < MAX_HEAP_SIZE; i++)
		h->data[i] = HEAP_INFINATE;
	h->size = 0;
}

static __inline void HeapPercolateUp(heap_t *h, int n)
{
	u32 tmp = h->data[n];
	u32 parent;
	
	for(parent = H_PARENT(n); parent > 0; parent = H_PARENT(n))
	{
		if(h->data[parent] > tmp)
			h->data[n] = h->data[parent];
		else
			break;
		/* Percolate up */
		n = parent;
	}
	h->data[n] = tmp;
}

static __inline void HeapPercolateDown(heap_t *h, int n)
{
	u32 tmp = h->data[n];
	u32 child;
	
	for(child = H_LEFT(n); child <= h->size; child = H_LEFT(n))
	{
		/* First find the minimun child */
		if(h->data[child+1] < h->data[child])
			child++;
		/* Then compare it with tmp */
		if(h->data[child] < tmp)
			h->data[n] = h->data[child];
		else
			break;
		/* Percolate down */
		n = child;
	}
	h->data[n] = tmp;
}

static __inline int HeapInsert(heap_t *h, u32 data)
{
	if(h->size < MAX_HEAP_SIZE)
	{
		h->data[++h->size] = data;
		HeapPercolateUp(h, h->size);
		return 0;
	}
	else
		return -1;
}

static __inline void HeapDelete(heap_t *h, int n)
{
	if(n <= h->size)
	{
		h->data[n] = h->data[h->size];
		h->data[h->size--] = HEAP_INFINATE;
		HeapPercolateDown(h, n);
	}
}

static __inline void HeapBuild(heap_t *h)
{
	int i;
	for(i = h->size >> 1; i >= 1; i--)
		HeapPercolateDown(h, i);
}



/************************************
 *
 * Interfaces
 *
 ************************************/
/*
 * Low level initialization 
 *  Init GPIO, TIM, DMA, NVIC for each group
 */
int PWMLowLevelInit(PWM_group_t group, GPIO_TypeDef *GPIO, PWM_channel channel)
{
	typedef enum {
		USED_NONE  = 0,
		USED_GPIOA = 0x0001,
		USED_GPIOB = 0x0002,
		USED_GPIOC = 0x0004,
		USED_GPIOD = 0x0008,
		USED_GPIOE = 0x0010,
		USED_GPIOF = 0x0020,
		USED_GPIOG = 0x0040,
	} gpio_occupy_t;
	/* avoid GPIO duplication */
	static gpio_occupy_t gpio_occupy = USED_NONE;
	/* We only support 3 PWM groups */
	static int used_groups = 0;
	GPIO_InitTypeDef io;
	
	if(used_groups >= 3)
		/* We only support 3 PWM groups */
		return -1;
	else if(GPIO == GPIOA && !(gpio_occupy & USED_GPIOA))
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE), gpio_occupy |= USED_GPIOA;
	else if(GPIO == GPIOB && !(gpio_occupy & USED_GPIOB))
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE), gpio_occupy |= USED_GPIOB;
	else if(GPIO == GPIOC && !(gpio_occupy & USED_GPIOC))
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE), gpio_occupy |= USED_GPIOC;
	else if(GPIO == GPIOD && !(gpio_occupy & USED_GPIOD))
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE), gpio_occupy |= USED_GPIOD;
	else if(GPIO == GPIOE && !(gpio_occupy & USED_GPIOE))
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE), gpio_occupy |= USED_GPIOE;
	else if(GPIO == GPIOF && !(gpio_occupy & USED_GPIOF))
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE), gpio_occupy |= USED_GPIOF;
	else if(GPIO == GPIOG && !(gpio_occupy & USED_GPIOG))
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE), gpio_occupy |= USED_GPIOG;
	else
		/* GPIO duplicated!!! */
		return -1;

	/* Initialze GPIO */
	io.GPIO_Speed = GPIO_Speed_50MHz;
	io.GPIO_Mode = GPIO_Mode_Out_PP;
	io.GPIO_Pin = channel;
	GPIO_Init(GPIO, &io);
	
	/* 
	 * PWM DMA group configure:
	 *
	 *  val  DMA usage               TIMx usage
	 *   1   DMA1_ch1 && DMA1_ch7    TIM2
	 *   2   DMA1_ch2 && DMA1_ch3    TIM3
	 *   3   DMA1_ch4 && DMA1_ch5    TIM4
	 */
	{
		pwm_info_t *pwm;
		DMA_Channel_TypeDef *dma_gpio, *dma_tim;
		TIM_TypeDef *tim;
		uint8_t nvic_channel;
		
		switch(group)
		{
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP1)
			case PWM_group_1:
				pwm = &PWM1;
				dma_gpio = DMA1_Channel1;
				dma_tim = DMA1_Channel7;
			
				RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
				tim = TIM2;
				TIM_DeInit(TIM2);
				TIM_DMACmd(TIM2, TIM_DMA_CC3 | TIM_DMA_CC4, ENABLE);
			
				nvic_channel = DMA1_Channel1_IRQn;
				break;
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP2)
			case PWM_group_2:
				pwm = &PWM2;
				dma_gpio = DMA1_Channel2;
				dma_tim = DMA1_Channel3;
			
				RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
				tim = TIM3;
				TIM_DeInit(TIM3);
				TIM_DMACmd(TIM3, TIM_DMA_CC3 | TIM_DMA_CC4, ENABLE);

				nvic_channel = DMA1_Channel2_IRQn;
				break;
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP3)
			case PWM_group_3:
				pwm = &PWM3;
				dma_gpio = DMA1_Channel4;
				dma_tim = DMA1_Channel5;
			
				RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
				tim = TIM4;
				TIM_DeInit(TIM4);
				TIM_DMACmd(TIM4, TIM_DMA_CC2 | TIM_DMA_CC3, ENABLE);
			
				nvic_channel = DMA1_Channel4_IRQn;
				break;
#endif
			default:
				return -1;
		}
		
		/* Initialize them all */
		{
			/* Private data structure */
			int i;
			for(i = 0; i < PWMCHANNELS*2; i++)
				pwm->dma_queue.gpio_bsrr_queue[i] = 0,
				pwm->dma_queue.tim_arr_queue[i] = TIM_ARR_STANDBY_VAR;
			pwm->phace_base = TIM_ARR_STANDBY_VAR;
			HeapInit(&pwm->phace_heap);
		}
		{
			/* DMA */
			DMA_InitTypeDef dmas;
			
			RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
			
			/* Common settings */
			dmas.DMA_DIR = DMA_DIR_PeripheralDST;
			dmas.DMA_Mode = DMA_Mode_Circular;
			dmas.DMA_M2M = DMA_M2M_Disable;
			dmas.DMA_MemoryInc = DMA_MemoryInc_Enable;
			dmas.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
			dmas.DMA_BufferSize = PWMCHANNELS*2;
			dmas.DMA_Priority = DMA_Priority_High;
			
			/* DMA channel for GPIO->BSRR */
			DMA_DeInit(dma_gpio);
			dmas.DMA_MemoryBaseAddr = (u32)&pwm->dma_queue.gpio_bsrr_queue[0];
			dmas.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
			dmas.DMA_PeripheralBaseAddr = (u32)&GPIO->BSRR;
			dmas.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
			DMA_Init(dma_gpio, &dmas);
			
			/* DMA channel for TIM->ARR */
			DMA_DeInit(dma_tim);
			dmas.DMA_MemoryBaseAddr = (u32)&pwm->dma_queue.tim_arr_queue[0];
			dmas.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
			dmas.DMA_PeripheralBaseAddr = (u32)&tim->ARR;
			dmas.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
			DMA_Init(dma_tim, &dmas);
			
			/* Select a DMA channel IRQ */
			DMA_ITConfig(dma_gpio, DMA_IT_HT | DMA_IT_TC, ENABLE);
		}
		{
			/* NVIC */
			NVIC_InitTypeDef nvic;
			
			nvic.NVIC_IRQChannel = nvic_channel;
			nvic.NVIC_IRQChannelCmd = ENABLE;
			// highest priority
			nvic.NVIC_IRQChannelPreemptionPriority = 0;
			nvic.NVIC_IRQChannelSubPriority = 0;
			NVIC_Init(&nvic);
		}
		{
			/* timer */
			TIM_TimeBaseInitTypeDef timbase;
			
			timbase.TIM_CounterMode = TIM_CounterMode_Up;
			timbase.TIM_Period = TIM_ARR_STANDBY_VAR;
			timbase.TIM_Prescaler = TIM_PRESCALER - 1;
			timbase.TIM_ClockDivision = 0;
			timbase.TIM_RepetitionCounter = 0;
			TIM_TimeBaseInit(tim, &timbase);
			TIM_SelectCCDMA(tim, ENABLE);
		}
	}
	return 0;
}

/*
 * Apply a PWM channel 
 *  It will overlay its old value
 */
void PWMApply(PWM_group_t group, PWM_channel channel, u16 h_length, u16 l_length, u16 init_phace, int number_of_pulses)
{
	switch(group)
	{
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP1)
		case PWM_group_1:
		{
			PWM1.applied_channels |= channel;
			while(channel)
			{
				int ch;
				__asm("clz ch, channel");
				ch = 31 - ch;
				PWM1.channels[ch].h_length = h_length;
				PWM1.channels[ch].l_length = l_length;
				PWM1.channels[ch].init_phace = init_phace;
				/* Pulse number multiplied by 2 means that,
				 * IO level changes twice each pulse */
				PWM1.channels[ch].number_of_pulses = number_of_pulses*2;
				channel &= ~(1 << ch);
			}
			break;
		}
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP2)
		case PWM_group_2:
		{
			PWM2.applied_channels |= channel;
			while(channel)
			{
				int ch;
				__asm("clz ch, channel");
				ch = 31 - ch;
				PWM2.channels[ch].h_length = h_length;
				PWM2.channels[ch].l_length = l_length;
				PWM2.channels[ch].init_phace = init_phace;
				PWM2.channels[ch].number_of_pulses = number_of_pulses*2;
				channel &= ~(1 << ch);
			}
			break;
		}
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP3)
		case PWM_group_3:
		{
			PWM3.applied_channels |= channel;
			while(channel)
			{
				int ch;
				__asm("clz ch, channel");
				ch = 31 - ch;
				PWM3.channels[ch].h_length = h_length;
				PWM3.channels[ch].l_length = l_length;
				PWM3.channels[ch].init_phace = init_phace;
				PWM3.channels[ch].number_of_pulses = number_of_pulses*2;
				channel &= ~(1 << ch);
			}
			break;
		}
#endif
		default:
			break;
	}
}

/*
 * Start PWM channel 
 *  Return 0 when success
 */
int PWMStart(PWM_group_t group, PWM_channel channel)
{
	pwm_info_t *pwm;
	DMA_Channel_TypeDef *dma_gpio, *dma_tim;
	TIM_TypeDef *tim;
	
	ENTER_CRITICAL;

	switch(group)
	{
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP1)
		case PWM_group_1:
		{
			pwm = &PWM1;
			dma_gpio = DMA1_Channel1;
			dma_tim = DMA1_Channel7;
			tim = TIM2;
			break;
		}
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP2)
		case PWM_group_2:
		{
			pwm = &PWM2;
			dma_gpio = DMA1_Channel2;
			dma_tim = DMA1_Channel3;
			tim = TIM3;
			break;
		}
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP3)
		case PWM_group_3:
		{
			pwm = &PWM3;
			dma_gpio = DMA1_Channel4;
			dma_tim = DMA1_Channel5;
			tim = TIM4;
			break;
		}
#endif
		default:
			EXIT_CRITICAL;
			return -1;
	}
	
	/* Detail settings */
	if( /* Attempt to start an unapplied channel */
		(pwm->applied_channels & channel) != channel ||
		/* Attempt to start an active channel */
		(pwm->active_channels & channel) != 0)
	{
		EXIT_CRITICAL;
		return -1;
	}
	else
	{
		pwm->active_channels |= channel;
		pwm->applied_channels &= ~channel;
		while(channel)
		{
			int ch;
			u32 heapnum;
			__asm("clz ch, channel");
			ch = 31 - ch;
			/*
			 * heap number template:
			 * |---- 27 bits ----|- 4 bits -|- 1 bit -|
			 * |   phace value   |  channel | IO level|
			 */
			heapnum = 
				((pwm->channels[ch].init_phace + pwm->phace_base) << 5) |
				(ch << 1) |
				1;
			HeapInsert(&pwm->phace_heap, heapnum);
			pwm->channels[ch].remaining_pulses = pwm->channels[ch].number_of_pulses;
			channel &= ~(1 << ch);
		}
	}
	
	DMA_Cmd(dma_gpio, ENABLE);
	DMA_Cmd(dma_tim, ENABLE);
	TIM_Cmd(tim, ENABLE);
	
	EXIT_CRITICAL;
	
	return 0;
}
/*
 * Stop PWM channel 
 */
void PWMStop(PWM_group_t group, PWM_channel channel)
{
	pwm_info_t *pwm;
	PWM_channel tmpchannel = channel;
	int i;
	
	ENTER_CRITICAL;
	
	switch(group)
	{
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP1)
		case PWM_group_1:
			pwm = &PWM1;
			break;
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP2)
		case PWM_group_2:
			pwm = &PWM2;
			break;
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP3)
		case PWM_group_3:
			pwm = &PWM3;
			break;
#endif
		default:
			EXIT_CRITICAL;
			return;
	}

	/* Reset those channels' pulse number */
	while(tmpchannel)
	{
		int ch;
		__asm("CLZ ch, tmpchannel");
		ch = 31 - ch;
		pwm->channels[ch].remaining_pulses = 1;
		tmpchannel &= ~(1 << ch);
	}
	/* Clear the output bits */
	channel = ~channel;
	pwm->active_channels &= channel;
	for(i = 0; i < PWMCHANNELS*2; i++)
		pwm->dma_queue.gpio_bsrr_queue[i] &= channel;
	
	EXIT_CRITICAL;
}

/* 
 * Get rest pulses of one channel 
 */
u32 GetRestPulse(PWM_group_t group, PWM_channel channel)
{
	__asm("CLZ channel, channel");
	channel = 31 - channel;
	switch(group)
	{
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP1)
		case PWM_group_1:
			return PWM1.channels[channel].remaining_pulses;
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP2)
		case PWM_group_2:
			return PWM2.channels[channel].remaining_pulses;
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP3)
		case PWM_group_3:
			return PWM3.channels[channel].remaining_pulses;
#endif
		default:
			return 0;
	}
}


/*
 * Private function
 *  Calculate the 2 dma queues
 *  Only referenced in the DMA IRQ functions
 *  Inlined in them when compiled with -O3, -Otime
 */
static int _CalcQueue(pwm_info_t *pwm, int curnum)
{
	int i;
	heap_t *h = &pwm->phace_heap;
	u32 firstheapnum = h->data[1];
	
	for(i = 0; i < PWMCHANNELS; i++,curnum++)
	{
		if(firstheapnum != HEAP_INFINATE)
		{
			u32 curphace = firstheapnum >> 5;
			u32 bsrr = 0;
			int n = 1;
			u32 size = h->size;
			
			/* Traverse the heap in post-order
			 * to find the element with the same phace number
			 * to form BSRR value
			 */
			do
			{
				n = H_LEFT(n);
				if(n <= size)
				{
					u32 left = h->data[n],
						right = h->data[n+1];
					if((left >> 5) == curphace)
						continue;
					if((right>> 5) == curphace)
					{
						n++;
						continue;
					}
				}
				n = H_PARENT(n);

				{
					/* heap number template:
					 * |---- 27 bits ----|- 4 bits -|- 1 bit -|
					 * |   phace value   |  channel | IO level|
					 */
					u32 curheapnum = h->data[n];
					u32 curchannel = (curheapnum>>1) & 0xf;
					u32 curiolevel = curheapnum & 0x1;
					/* set bsrr */
					bsrr |= (curiolevel | ((curiolevel ^ 1) << 16)) << curchannel;
					if(-- pwm->channels[curchannel].remaining_pulses)
					{
						int child;
						u32 tmpheapnum = curiolevel ? 
							(pwm->channels[curchannel].h_length << 5) + curheapnum:
							(pwm->channels[curchannel].l_length << 5) + curheapnum;
						/* swap its LSB */
						tmpheapnum ^= 0x1;
						/* percolate down */
						for(child = H_LEFT(n); child <= size; child = H_LEFT(child))
						{
							u32 left = h->data[child];
							u32 right= h->data[child+1];
							if(left > right)
								child++, left = right;
							if(left < tmpheapnum)
								h->data[H_PARENT(child)] = left;
							else
								break;
						}
						h->data[H_PARENT(child)] = tmpheapnum;
					}
					else
					{
						pwm->active_channels &= ~(1 << curchannel);
						HeapDelete(&pwm->phace_heap, n);
					}
				}
				n = H_PARENT(n);
			}while(n >= 1);
			
			/* Fill DMA queues */
			pwm->dma_queue.gpio_bsrr_queue[curnum] = bsrr;
			firstheapnum = h->data[1];
			pwm->dma_queue.tim_arr_queue[curnum] = firstheapnum != HEAP_INFINATE ?
				(firstheapnum>>5) - curphace :
				/* In case it's the last pulse of the last active channel... */
				TIM_ARR_STANDBY_VAR;
		}
		else
		{
			pwm->dma_queue.gpio_bsrr_queue[curnum] = 0;
			pwm->dma_queue.tim_arr_queue[curnum] = TIM_ARR_STANDBY_VAR;
		}
	}
	if(h->data[1] & 0xf0000000)
	{
		/* "if(first heap number > 0xf000 0000)"
		 * Do this to avoid overflow.
		 */
		int j;
		for(j = h->size; j > 0; j--)
			h->data[j] &= 0x0fffffff;
	}
	return curnum;
}
/*
 * DMA IRQs
 *  Calculate 2 queues for GPIO->BSRR and TIM->ARR
 */
/* 
 * Double-buffered calculate model:
 *
 * start:
 *   |________| |________|
 *   ^
 *   DMA -->
 *
 * DMA HT interrupt:
 *   |--------| |________|
 *   ^          ^
 *   CPU ====>| DMA -->
 *
 * DMA TC interrupt:
 *   |________| |--------|
 *   ^          ^
 *   DMA -->    CPU ====>|
 * 
 */
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP1)
void DMA1_Channel1_IRQHandler(void)
{
	static int curnum = 0;
	static int sign_complete = 0;
	ENTER_CRITICAL;
	
	/* On simulator, DMA sets HTIF everytime it transfers one word
	 * after the half of the buffer, making loads of interrupts.
	 * So I disable HT interrupt on HT IRQ, and enable it on TC IRQ... 
	 */
	DMA1_Channel1->CCR ^= DMA_IT_HT;
	/* Clear all IT pending bits */
	DMA1->IFCR = DMA1_IT_GL1;

	if(PWM1.active_channels)
	{
		sign_complete = 0;
		curnum = _CalcQueue(&PWM1, curnum);
	}
	else
	{
		int i;
		for(i = 0; i < PWMCHANNELS; i++,curnum++)
		{
			/* Set buffer to default value, for the next run */
			PWM1.dma_queue.gpio_bsrr_queue[curnum] = 0;
			PWM1.dma_queue.tim_arr_queue[curnum] = TIM_ARR_STANDBY_VAR;
		}
		if(sign_complete == 2)
		{
			/* PWM completed */
			PWM1.phace_base = TIM_ARR_STANDBY_VAR;
			curnum = 0;
			
			TIM_Cmd(TIM2, DISABLE);
			DMA_Cmd(DMA1_Channel1, DISABLE);
			DMA_Cmd(DMA1_Channel7, DISABLE);
			/* restore DMA counter */
			DMA1_Channel1->CNDTR = 
			DMA1_Channel7->CNDTR = PWMCHANNELS*2;
			
			DMA1->IFCR = DMA1_IT_GL1;
			DMA1_Channel1->CCR |= DMA_IT_HT;
		}
	}
	
	if(!PWM1.active_channels)
		sign_complete++;
	curnum %= PWMCHANNELS*2;
	
	EXIT_CRITICAL;
}
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP2)
void DMA1_Channel2_IRQHandler(void)
{
	static int curnum = 0;
	static int sign_complete = 0;
	ENTER_CRITICAL;
	
	DMA1_Channel2->CCR ^= DMA_IT_HT;
	/* Clear all IT pending bits */
	DMA1->IFCR = DMA1_IT_GL2;
	
	if(PWM2.active_channels)
	{
		sign_complete = 0;
		curnum = _CalcQueue(&PWM2, curnum);
	}
	else
	{
		int i;
		for(i = 0; i < PWMCHANNELS; i++,curnum++)
		{
			/* Set buffer to default value, for the next run */
			PWM2.dma_queue.gpio_bsrr_queue[curnum] = 0;
			PWM2.dma_queue.tim_arr_queue[curnum] = TIM_ARR_STANDBY_VAR;
		}
		if(sign_complete == 2)
		{
			/* PWM completed */
			PWM2.phace_base = TIM_ARR_STANDBY_VAR;
			curnum = 0;
			
			TIM_Cmd(TIM3, DISABLE);
			DMA_Cmd(DMA1_Channel2, DISABLE);
			DMA_Cmd(DMA1_Channel3, DISABLE);
			/* restore DMA counter */
			DMA1_Channel2->CNDTR = 
			DMA1_Channel3->CNDTR = PWMCHANNELS*2;
			
			DMA1->IFCR = DMA1_IT_GL2;
			DMA1_Channel2->CCR |= DMA_IT_HT;
		}
	}
	
	if(!PWM2.active_channels)
		sign_complete++;
	curnum %= PWMCHANNELS*2;
	
	EXIT_CRITICAL;
}
#endif
#if (USE_PWMDMA_GROUP & PWMDMA_GROUP3)
void DMA1_Channel4_IRQHandler(void)
{
	static int curnum = 0;
	static int sign_complete = 0;
	ENTER_CRITICAL;
	
	DMA1_Channel4->CCR ^= DMA_IT_HT;
	/* Clear all IT pending bits */
	DMA1->IFCR = DMA1_IT_GL4;
	
	if(PWM3.active_channels)
	{
		sign_complete = 0;
		curnum = _CalcQueue(&PWM3, curnum);
	}
	else
	{
		int i;
		for(i = 0; i < PWMCHANNELS; i++,curnum++)
		{
			/* Set buffer to default value, for the next run */
			PWM3.dma_queue.gpio_bsrr_queue[curnum] = 0;
			PWM3.dma_queue.tim_arr_queue[curnum] = TIM_ARR_STANDBY_VAR;
		}
		if(sign_complete == 2)
		{
			/* PWM completed */
			PWM3.phace_base = TIM_ARR_STANDBY_VAR;
			curnum = 0;
			
			TIM_Cmd(TIM4, DISABLE);
			DMA_Cmd(DMA1_Channel4, DISABLE);
			DMA_Cmd(DMA1_Channel5, DISABLE);
			/* restore DMA counter */
			DMA1_Channel4->CNDTR = 
			DMA1_Channel5->CNDTR = PWMCHANNELS*2;
			
			DMA1->IFCR = DMA1_IT_GL4;
			DMA1_Channel4->CCR |= DMA_IT_HT;
		}
	}
	
	if(!PWM3.active_channels)
		sign_complete++;
	curnum %= PWMCHANNELS*2;
	
	EXIT_CRITICAL;
}
#endif
