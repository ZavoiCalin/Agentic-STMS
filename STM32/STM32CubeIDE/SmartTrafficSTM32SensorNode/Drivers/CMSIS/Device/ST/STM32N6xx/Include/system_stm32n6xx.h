#ifndef SYSTEM_STM32N6XX_H
#define SYSTEM_STM32N6XX_H

#include <stdint.h>

extern uint32_t SystemCoreClock;

void SystemInit(void);
void SystemCoreClockUpdate(void);

#endif
