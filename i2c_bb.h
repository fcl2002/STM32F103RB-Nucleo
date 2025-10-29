#ifndef I2CBB_H
#define I2CBB_H

#include <stdint.h>
#include "stm32f10x.h"
#include "io.h"      /* io_init_simple, io_write, io_read */
#include "delay.h"   /* delay_us */

/* =============================================================================
 * I2C Bit-Bang (STM32F103RB) — Estilo bare-metal
 * -----------------------------------------------------------------------------
 * Linhas I2C devem ser OPEN-DRAIN com pull-ups (externos ou internos):
 *   SCL = PB6
 *   SDA = PB7
 *
 * Níveis (open-drain):
 *   - "LOW"  -> pino força 0 (drive)
 *   - "HIGH" -> pino libera linha (release) e pull-up puxa para 1
 *
 * Timing default (~100 kHz): T_LOW = 5 us, T_HIGH = 5 us
 * Ajuste conforme necessário.
 * =============================================================================
 */

/* --------- Pinos (ajuste se necessário) --------- */
#define I2CBB_SCL_PORT   GPIOB
#define I2CBB_SCL_PIN    6u

#define I2CBB_SDA_PORT   GPIOB
#define I2CBB_SDA_PIN    7u

/* --------- Temporização (em microssegundos) --------- */
#define I2CBB_T_LOW_US    15u
#define I2CBB_T_HIGH_US   15u
#define I2CBB_T_SU_STA_US 5u   /* setup START   */
#define I2CBB_T_HD_STA_US 5u   /* hold  START   */
#define I2CBB_T_SU_STO_US 5u   /* setup STOP    */
#define I2CBB_T_SU_DAT_US 2u   /* data setup    */
#define I2CBB_T_HD_DAT_US 0u   /* data hold (0: garantido por bordas) */

/* --------- Convenções de ACK/NACK --------- */
#define I2CBB_ACK   0u
#define I2CBB_NACK  1u

/* =============================================================================
 * API
 * =============================================================================
 */

/**
 * @brief Inicializa os pinos como Open-Drain e coloca o barramento em repouso (SCL=1, SDA=1).
 *        - SCL: PB6 -> AF OD 2 MHz (ou OD)
 *        - SDA: PB7 -> AF OD 2 MHz (ou OD)
 *        Observação: Se não houver pull-ups externos, habilite PUPD como PULL-UP (menos robusto).
 */
void i2cbb_init(void);

/** Gera condição START (SDA: 1->0 enquanto SCL=1, depois SCL=0). */
void i2cbb_start(void);

/** Gera condição STOP (SDA: 0->1 enquanto SCL=1). */
void i2cbb_stop(void);

/** Escreve 1 bit na linha SDA (0 = força baixo, 1 = libera) com clock em SCL. */
void i2cbb_write_bit(uint8_t bit);

/** Lê 1 bit da linha SDA durante pulso de SCL. Retorna 0/1. */
uint8_t i2cbb_read_bit(void);

/**
 * @brief Escreve 1 byte (MSB primeiro) e lê ACK do slave.
 * @return I2CBB_ACK (0) se escravo reconheceu, I2CBB_NACK (1) caso contrário.
 */
uint8_t i2cbb_write_byte(uint8_t byte);

/**
 * @brief Lê 1 byte (MSB primeiro) e envia ACK/NACK ao final.
 * @param ack  I2CBB_ACK para continuar leitura; I2CBB_NACK para último byte.
 * @return Byte lido.
 */
uint8_t i2cbb_read_byte(uint8_t ack);

/* =============================================================================
 * (Opcional) Helpers de alto nível — declare aqui, implemente se desejar
 * =============================================================================
 */
/* Escrita em registrador de um dispositivo 8-bit addr (7-bit <<1 + W): */
/* uint8_t i2cbb_write_reg(uint8_t addr7, uint8_t reg, uint8_t data);    */
/* Leitura de registrador:                                               */
/* uint8_t i2cbb_read_reg(uint8_t addr7, uint8_t reg, uint8_t *data);    */

#endif /* I2CBB_H */
