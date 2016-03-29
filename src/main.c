#include "reg52.h"
#include "pwm.h"

void InitWorld(void)
{
}

void main(void)
{
	char i;

	InitWorld();
	for(i = 1; i <= 12; i++)
		PWMSet(i, 2);
	PWMStart();

	while(1);
}
