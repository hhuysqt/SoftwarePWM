##配置方法
一般只用修改PWM_cfg.h<br>
**TIM_PRESCALER**<br>
可以配置PWM的精度，默认是1us。<br>
**USE_PWMDMA_GROUP**<br>
可以配置你需要的PWM通道。默认三组全开。如果不需要三组PWM全用，或者PWM占用了某些需要用的DMA通道，可做裁剪<br>

##使用方法
1、只需包含PWM.h<br>
2、对每组输出做底层初始化，波形可定位到任意GPIO口处，如<br>
>PWMLowLevelInit(PWM_group_1, GPIOB, PWM_Ch1 | PWM_Ch2);

3、注册通道，需要输入高低电平宽度、初相、脉冲个数，可一次注册多个通道，如<br>
>PWMApply(PWM_group_1, PWM_Ch1 | PWM_Ch2, 100, 500, 10, 20);

4、选择某些通道开始输出，经过两次DMA中断后会开始输出，大约延迟两三百us<br>
>PWMStart(PWM_group_1, PWM_Ch1 | PWM_Ch2);

5、帮我测试其他函数。。。<br>
