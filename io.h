#ifndef IO_H
#define IO_H

#include "stm32f10x.h"
#include <stdint.h>

/* =============================================================================
 * STM32F1 GPIO (CRL/CRH):
 *   4 bits por pino: [CNF1:CNF0:MODE1:MODE0]
 *   MODE = 00 (entrada), 01 (10 MHz), 10 (2 MHz), 11 (50 MHz)
 *   CNF  = ver abaixo
 * -----------------------------------------------------------------------------
 * Entrada:
 *   ANALOG     CNF=00 -> 0x0  (MODE=00)
 *   FLOATING   CNF=01 -> 0x4  (MODE=00)
 *   PUPD       CNF=10 -> 0x8  (MODE=00)  (pull via ODR)
 *
 * Saída (escolher velocidade por MODE):
 *   PUSH-PULL         CNF=00 -> 0x0 + MODE (01/10/11 -> 0x1/0x2/0x3)
 *   OPEN-DRAIN        CNF=01 -> 0x4 + MODE (            0x5/0x6/0x7)
 *   AF PUSH-PULL      CNF=10 -> 0x8 + MODE (            0x9/0xA/0xB)
 *   AF OPEN-DRAIN     CNF=11 -> 0xC + MODE (            0xD/0xE/0xF)
 * =============================================================================
 */

/* ------------------------------ Bits utilitários --------------------------- */
#define IO_BIT(pin)                (1u << (pin))
#define IO_IS_VALID_PIN(pin)       ((pin) < 16u)

/* ------------------------------ Entrada (MODE=00) ------------------------- */
#define IO_IN_ANALOG               0x0  /* CNF=00, MODE=00 */
#define IO_IN_FLOATING             0x4  /* CNF=01, MODE=00 */
#define IO_IN_PUPD                 0x8  /* CNF=10, MODE=00 (pull via ODR) */

/* ------------------------------ Saída 10 MHz (MODE=01) -------------------- */
#define IO_OUT_PP_10M              0x1  /* CNF=00 */
#define IO_OUT_OD_10M              0x5  /* CNF=01 */
#define IO_AF_PP_10M               0x9  /* CNF=10 */
#define IO_AF_OD_10M               0xD  /* CNF=11 */

/* ------------------------------ Saída 2 MHz (MODE=10) --------------------- */
#define IO_OUT_PP_2M               0x2  /* CNF=00 */
#define IO_OUT_OD_2M               0x6  /* CNF=01 */
#define IO_AF_PP_2M                0xA  /* CNF=10 */
#define IO_AF_OD_2M                0xE  /* CNF=11 */

/* ------------------------------ Saída 50 MHz (MODE=11) -------------------- */
#define IO_OUT_PP_50M              0x3  /* CNF=00 */
#define IO_OUT_OD_50M              0x7  /* CNF=01 */
#define IO_AF_PP_50M               0xB  /* CNF=10 */
#define IO_AF_OD_50M               0xF  /* CNF=11 */

/* ------------------------------ Extras para io_config_ex ------------------ */
#define IO_PULL_DOWN               0u
#define IO_PULL_UP                 1u
#define IO_LEVEL_LOW               0u
#define IO_LEVEL_HIGH              1u
#define IO_EXTRA_NONE              0xFFu

/* TIPOS "ERGONÔMICOS" (wrappers) */
typedef enum {
    IO_FUNC_INPUT_ANALOG = 0,
    IO_FUNC_INPUT_FLOATING,
    IO_FUNC_INPUT_PUPD,
    IO_FUNC_OUTPUT_PP,
    IO_FUNC_OUTPUT_OD,
    IO_FUNC_AF_PP,
    IO_FUNC_AF_OD
} io_func_t;

typedef enum {
    IO_SPEED_10M = 0,  /* MODE=01 */
    IO_SPEED_2M,       /* MODE=10 */
    IO_SPEED_50M       /* MODE=11 */
} io_speed_t;

typedef enum {
    IO_NOPULL = 0,     /* ignore (FLOATING/ANALOG) */
    IO_PULLUP,
    IO_PULLDOWN
} io_pull_t;

/* Identificador simples de pino */
typedef struct {
    GPIO_TypeDef *port; /* GPIOA..GPIOE (F103RB) */
    uint8_t       pin;  /* 0..15 */
} io_pin_t;

/* API BRUTA (nibble CRL/CRH) */

/**
 * @brief Habilita clock do GPIO (A..E) no RCC->APB2ENR.
 *        Aceita GPIOA..GPIOE (Nucleo expõe A/B/C).
 */
void io_enable_port_clock(GPIO_TypeDef *port);

/**
 * @brief Configura o nibble de 4 bits (CRL/CRH) do pino (sem mexer em ODR).
 *        cfg4bits = valores IO_IN_*, IO_OUT_*_*M, IO_AF_*_*M.
 */
void io_config_raw(GPIO_TypeDef *port, uint8_t pin, uint8_t cfg4bits);

/**
 * @brief Versão estendida:
 *        - Se entrada PUPD: aplica pull via ODR (UP/DOWN).
 *        - Se saída/AF: definirá nível inicial via BSRR/BRR.
 *        Passe IO_EXTRA_NONE quando não aplicável.
 */
void io_config_ex_raw(GPIO_TypeDef *port, uint8_t pin, uint8_t cfg4bits, uint8_t extra);

/* API ERGONÔMICA (wrappers) */

/**
 * @brief Inicializa pino com enums legíveis.
 * @param func   INPUT_* / OUTPUT_* / AF_*
 * @param speed  Para OUTPUT/AF: 10M/2M/50M. Ignorado em INPUT.
 * @param pull   Para INPUT_PUPD: PULLUP/PULLDOWN. Para FLOATING/ANALOG, use IO_NOPULL.
 * @param init_level  Para OUTPUT/AF: nível inicial (LOW/HIGH). Ignorado em INPUT.
 *
 * Internamente traduz para cfg4bits e chama io_config_ex_raw().
 */
void io_init_simple(GPIO_TypeDef *port,
                    uint8_t pin,
                    io_func_t func,
                    io_speed_t speed,
                    io_pull_t pull,
                    uint8_t init_level);

/* I/O em tempo de execução */
void io_write(GPIO_TypeDef *port, uint8_t pin, uint8_t level);
uint8_t io_read(GPIO_TypeDef *port, uint8_t pin);
void io_toggle(GPIO_TypeDef *port, uint8_t pin);

/** Constrói cfg4bits de saída a partir de tipo (PP/OD/AF) + velocidade. */
uint8_t io_build_out_cfg(io_func_t func, io_speed_t speed);

#endif /* IO_H */
