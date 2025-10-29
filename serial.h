#ifndef SERIAL_H
#define SERIAL_H

#include "stm32f10x.h"
#include <stdint.h>

/* USART2 - STM32F103RB
 * Pinos: PA2 = TX (AF Push-Pull, 2 MHz), PA3 = RX (Input floating)
 * Formato: 8-N-1, baudrate fixo = 9600 (PCLK1 = 36 MHz típico no F1)
 */

/** Inicializa USART2 em 9600 8N1 (configura RCC, GPIOA e USART2). */
void serial2_init(void);

/** Envia um caractere (bloqueia até TXE). */
void serial2_putc(char c);

/** Envia uma string terminada em '\0'. */
void serial2_puts(const char *s);

/** Retorna 1 se há um byte disponível para leitura (SR.RXNE), senão 0. */
uint8_t serial2_readable(void);

/** Lê um byte (bloqueia até chegar). */
char serial2_getc(void);

#endif /* SERIAL_H */
