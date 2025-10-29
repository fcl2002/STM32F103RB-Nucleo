#include "delay.h"

/* Clock fixo de 72 MHz -> 1 µs = 72 ticks */
#define TICKS_PER_US 72u

void delay_us(uint32_t us)
{
    /* Configura SysTick */
    SysTick->LOAD = (TICKS_PER_US * us);
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    /* Espera até COUNTFLAG ser setado (estouro do contador) */
    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0);

    /* Desliga SysTick para evitar interferência */
    SysTick->CTRL = 0;
}

void delay_ms(uint32_t ms)
{
    while (ms--) {
        delay_us(1000u);
    }
}
