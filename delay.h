#ifndef DELAY_H
#define DELAY_H

#include "stm32f10x.h"
#include <stdint.h>

/* =============================================================================
 * Biblioteca de temporização fixa (SysTick, 72 MHz)
 * -----------------------------------------------------------------------------
 * Usa o clock fixo do sistema de 72 MHz (1 µs = 72 ticks)
 * Gera atrasos de microssegundos e milissegundos com boa precisão.
 * =============================================================================
 */

/**
 * @brief Atraso em microssegundos
 * @param us Tempo em microssegundos
 */
void delay_us(uint32_t us);

/**
 * @brief Atraso em milissegundos
 * @param ms Tempo em milissegundos
 */
void delay_ms(uint32_t ms);

#endif /* DELAY_H */
