#ifndef PCF8574_H
#define PCF8574_H

#include <stdint.h>
#include "i2c_bb.h"   /* usa i2cbb_start, i2cbb_stop, i2cbb_write_byte, i2cbb_read_byte */

/* =============================================================================
 * PCF8574 — Expansor de E/S via I²C (8 bits)
 * -----------------------------------------------------------------------------
 * Endereço 7-bit base = 0b0100 A2 A1 A0  (0x20 .. 0x27)
 * Cada pino A2,A1,A0 do chip define os 3 bits menos significativos do endereço.
 *
 * Use o helper abaixo para montar o endereço:
 *     uint8_t addr = PCF8574_ADDR(0,0,0);  // -> 0x20
 * =============================================================================
 */

#define PCF8574_ADDR(a2,a1,a0)   ((uint8_t)(0x20u | (((a2)&1u)<<2) | (((a1)&1u)<<1) | ((a0)&1u)))

/* Retornos padrão */
#define PCF8574_OK     1u
#define PCF8574_FAIL   0u

/* =============================================================================
 * API
 * =============================================================================
 */

/**
 * @brief Envia 1 byte para o PCF8574 (modo write).
 * @param addr7 Endereço de 7 bits do chip (ex: 0x20..0x27)
 * @param value Byte a escrever (bit=1 -> libera pino, bit=0 -> força nível baixo)
 * @return PCF8574_OK (ACK recebido) ou PCF8574_FAIL (NACK)
 */
uint8_t pcf8574_write(uint8_t addr7, uint8_t value);

/**
 * @brief Lê 1 byte do PCF8574 (modo read).
 * @param addr7 Endereço de 7 bits do chip (ex: 0x20..0x27)
 * @param out Ponteiro para armazenar o byte lido
 * @return PCF8574_OK (ACK recebido) ou PCF8574_FAIL (NACK)
 */
uint8_t pcf8574_read(uint8_t addr7, uint8_t *out);

#endif /* PCF8574_H */
