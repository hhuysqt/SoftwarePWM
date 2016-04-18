#include "PWM.H"

int main()
{
	PWMLowLevelInit(PWM_group_1, GPIOB, PWM_Ch1);
	PWMApply(PWM_group_1, PWM_Ch1 | PWM_Ch2, 100, 500, 10, 20);
	PWMApply(PWM_group_1, PWM_Ch3, 1000, 100, 30, 10);
	PWMApply(PWM_group_1, PWM_Ch4, 100, 100, 40, 10);
	PWMApply(PWM_group_1, PWM_Ch5, 100, 100, 50, 20);
	PWMApply(PWM_group_1, PWM_Ch6, 100, 100, 60, 10);
	PWMApply(PWM_group_1, PWM_Ch7, 100, 100, 70, 20);
	PWMApply(PWM_group_1, PWM_Ch8, 100, 100, 80, 10);
	PWMApply(PWM_group_1, PWM_Ch9, 100, 100, 90, 20);
	PWMApply(PWM_group_1, PWM_Ch10, 100, 100, 100, 10);
	PWMApply(PWM_group_1, PWM_Ch11, 100, 100, 110, 20);
	PWMApply(PWM_group_1, PWM_Ch12, 100, 100, 120, 10);
	PWMApply(PWM_group_1, PWM_Ch13, 100, 100, 130, 10);
	PWMApply(PWM_group_1, PWM_Ch14, 100, 100, 140, 10);
	PWMApply(PWM_group_1, PWM_Ch15, 100, 100, 150, 20);
	PWMApply(PWM_group_1, PWM_Ch16, 100, 100, 160, 30);

	PWMLowLevelInit(PWM_group_2, GPIOA, PWM_Ch1 | PWM_Ch2);
	PWMApply(PWM_group_2, PWM_Ch1 | PWM_Ch2, 100, 500, 10, 10);
	PWMApply(PWM_group_2, PWM_Ch3, 1000, 100, 30, 10);
	PWMApply(PWM_group_2, PWM_Ch4, 100, 100, 40, 10);

	PWMLowLevelInit(PWM_group_3, GPIOC, PWM_Ch1 | PWM_Ch2);
	PWMApply(PWM_group_3, PWM_Ch1 | PWM_Ch2, 100, 500, 10, 10);
	PWMApply(PWM_group_3, PWM_Ch3, 1000, 100, 30, 10);
	PWMApply(PWM_group_3, PWM_Ch4, 100, 100, 40, 10);

	PWMStart(PWM_group_1, PWM_Ch1 | PWM_Ch2 | PWM_Ch3 | PWM_Ch4 | PWM_Ch5 | PWM_Ch6 | PWM_Ch7 | PWM_Ch8 | PWM_Ch9 | PWM_Ch10| PWM_Ch11| PWM_Ch12| PWM_Ch13| PWM_Ch14| PWM_Ch15 | PWM_Ch16);
	PWMStart(PWM_group_2, PWM_Ch1 | PWM_Ch2 | PWM_Ch3 | PWM_Ch4);
	PWMStart(PWM_group_3, PWM_Ch1 | PWM_Ch2 | PWM_Ch3 | PWM_Ch4);
	
	while(1)
	{
		volatile int i;
		for(i = 0; i < 200000; i++);
		PWMApply(PWM_group_2, PWM_Ch1 | PWM_Ch2, 100, 100, 0, 10);
		PWMStart(PWM_group_2, PWM_Ch1 | PWM_Ch2);
		PWMApply(PWM_group_1, PWM_Ch1 | PWM_Ch2 | PWM_Ch3 | PWM_Ch4 | PWM_Ch5 | PWM_Ch6 | PWM_Ch7 | PWM_Ch8 | PWM_Ch9 | PWM_Ch10| PWM_Ch11| PWM_Ch12| PWM_Ch13| PWM_Ch14| PWM_Ch15 | PWM_Ch16, 100, 80, 10, 20);
		PWMStart(PWM_group_1, PWM_Ch1 | PWM_Ch2 | PWM_Ch3 | PWM_Ch4 | PWM_Ch5 | PWM_Ch6 | PWM_Ch7 | PWM_Ch8 | PWM_Ch9 | PWM_Ch10| PWM_Ch11| PWM_Ch12| PWM_Ch13| PWM_Ch14| PWM_Ch15 | PWM_Ch16);
		PWMApply(PWM_group_3, PWM_Ch1 | PWM_Ch2, 100, 100, 0, 10);
		PWMStart(PWM_group_3, PWM_Ch1 | PWM_Ch2);
	}
	
	return 0;
}
