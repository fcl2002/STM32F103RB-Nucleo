#include "serial.h"

/* -------------------------------------------------------------
 * Configuração fixa:
 *  - PCLK1 = 36 MHz (típico com SYSCLK 72 MHz)
 *  - Baudrate = 9600, oversampling x16
 *  - USARTDIV = 36e6 / (16 * 9600) = 234.375
 *    Mantissa = 234 (0xEA), Fração = 0.375*16 = 6 (0x6)
 *    BRR = (0xEA << 4) | 0x6 = 0xEA6
 * ------------------------------------------------------------- */
#define USART2_BRR_9600_AT_36MHZ   ((uint16_t)0x0EA6)

/* Bits de controle/status (para clareza) */
#ifndef USART_SR_TXE
#define USART_SR_TXE  (1U << 7)   /* Transmit data register empty */
#endif
#ifndef USART_SR_RXNE
#define USART_SR_RXNE (1U << 5)   /* Read data register not empty */
#endif
#ifndef USART_CR1_UE
#define USART_CR1_UE  (1U << 13)  /* USART enable */
#endif
#ifndef USART_CR1_TE
#define USART_CR1_TE  (1U << 3)   /* Transmitter enable */
#endif
#ifndef USART_CR1_RE
#define USART_CR1_RE  (1U << 2)   /* Receiver enable */
#endif

void serial2_init(void)
{
    /* --- Clocks: GPIOA e USART2 (e AFIO por segurança) --- */
    RCC->APB2ENR |= (1U << 2);   /* IOPAEN */
    RCC->APB2ENR |= (1U << 0);   /* AFIOEN */
    RCC->APB1ENR |= (1U << 17);  /* USART2EN */

    /* (Opcional) Garantir sem remap: USART2 em PA2/PA3 */
#ifdef AFIO_MAPR_USART2_REMAP
    AFIO->MAPR &= ~AFIO_MAPR_USART2_REMAP;
#endif

    /* --- GPIOA Config ---
     * PA2 (TX): AF Push-Pull, 2 MHz  => CRL nibble pino 2 = 0xA
     * PA3 (RX): Input floating       => CRL nibble pino 3 = 0x4
     */
    /* Limpa nibbles de PA2 e PA3 */
    GPIOA->CRL &= ~((0xFU << (2U * 4U)) | (0xFU << (3U * 4U)));
    /* PA2: 0xA (AF-PP 2MHz), PA3: 0x4 (IN floating) */
    GPIOA->CRL |=  ((0xAU << (2U * 4U)) | (0x4U << (3U * 4U)));

    /* --- USART2 Config --- */
    USART2->CR1 = 0;                         /* 8N1 padrão, sem parity */
    USART2->BRR = USART2_BRR_9600_AT_36MHZ;  /* 9600 @ PCLK1 = 36 MHz */
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE);  /* TX e RX habilitados */
    USART2->CR1 |= USART_CR1_UE;             /* USART enable */
}

void serial2_putc(char c)
{
    /* Espera TXE (TDR vazio) */
    while ((USART2->SR & USART_SR_TXE) == 0U) { /* wait */ }
    USART2->DR = (uint16_t)(uint8_t)c;
}

void serial2_puts(const char *s)
{
    if (!s) return;
    while (*s) {
        serial2_putc(*s++);
    }
}

uint8_t serial2_readable(void)
{
    return (USART2->SR & USART_SR_RXNE) ? 1U : 0U;
}

char serial2_getc(void)
{
    /* Bloqueia até RXNE = 1 */
    while ((USART2->SR & USART_SR_RXNE) == 0U) { /* wait */ }
    return (char)(USART2->DR & 0xFFU);
}
