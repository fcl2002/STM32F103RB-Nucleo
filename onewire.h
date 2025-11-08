#ifndef ONEWIRE_H
#define ONEWIRE_H

#include "stm32f10x.h"
#include "io.h"

/* Pinos usados no TP5 */
#define ONEWIRE_GPIO      GPIOB
#define ONEWIRE_DQ_PIN    4u   /* SIGNAL ONEWIRE  (TIM3_CH1 futuramente) */
#define ONEWIRE_SPY_PIN   5u   /* SIGNAL ESPION_INSTANT_LECTURE (TIM3_CH2 futuramente) */

/* -------- OneWire timings (µs) conforme DS18B20 Fig.12–14 -------- */
#define OW_TREC_US        2u    /* Recovery time ≥1 µs entre slots          */
/* -------- tempos em µs (ajuste se o prof pedir outros) -------- */

/* Reset + presence */
#define OW_RESET_TLOW      480u   /* master mantém DQ LOW         */
#define OW_RESET_TSAMPLE   550u   /* instante para amostrar presença */
#define OW_RESET_TEND      960u   /* fim do motivo                */

/* Write slots (slot ≥60 µs) */
#define OW_W1_TLOW         10u  /* write '1': LOW curto, solta ≤15 µs        */
#define OW_W1_TEND         65u  /* fim do slot (≥60)                         */
#define OW_W0_TLOW         60u  /* write '0': LOW ≈60 µs                     */
#define OW_W0_TEND         65u  /* fim do slot (≥60)                         */

/* Read slot (Fig.13–14): TINIT pequeno, sample em 15 µs, slot ≥60 µs */
#define OW_R_TINIT          3u  /* ≥1 µs e “pequeno”                         */
#define OW_R_TSAMPLE       15u  /* master samples at 15 µs                   */
#define OW_R_TEND          65u  /* slot (≥60 µs)                             */


/* 2.1 — Configuração dos pinos:
 * - PB4 (DQ): AF Open-Drain (linha 1-Wire, pull-up externo)
 * - PB5 (Espion): Saída Push-Pull, inicia em nível alto
 */
void init_pins_onewire(void);

/* -------------------- 2.2 — Timer (TIM3 only) -------------------- */
/* Common init:
 * - Enable clocks
 * - Disable JTAG (keep SWD)
 * - TIM3 partial remap (CH1->PB4, CH2->PB5)
 * - Prescaler to 1 µs tick
 * - Configure OC modes (idle released on CH1, idle HIGH on CH2)
 * - Enable CC interrupts for CH2 (D2) and CH3 (D3)
 */
void init_timer(void);

/* Program and start a motif:
 * D1_us → CH1 compare (release after negative pulse on DQ)
 * D2_us → CH2 compare (positive pulse on ESPION, raise interrupt at end)
 * D3_us → CH3 compare (interrupt-only: end of motif)
 * - Resets TIM3->CNT, presets outputs (DQ released, ESPION HIGH), loads CCR1/CCR2/CCR3, then starts.
 */
void init_motif(uint16_t D1_us, uint16_t D2_us, uint16_t D3_us);

/* -------------------- Sync flags & helpers -------------------- */
extern volatile uint8_t g_espion_done;  /* set at end of D2 (TIM3 CC2) */
extern volatile uint8_t g_motif_done;   /* set at end of D3 (TIM3 CC3) */

/* 2.3 — variables de protocole */
extern volatile uint8_t etat_one_wire;        /* valeur lue sur la ligne DQ à l’instant D2 */
extern volatile uint8_t motif_one_wire_fini;  /* sémaphore: motif terminé (D3 atteint) */

/* helpers de “espera bloqueante” (facilitam os testes no main) */
void onewire_wait_motif_done(void);
void onewire_clear_flags(void);

void onewire_wait_espion_done(void);
void onewire_wait_motif_done(void);
void onewire_clear_flags(void);

/* 2.4 — Fonctions élémentaires bloquantes OneWire */
void RESET_ONEWIRE(void);
void ENVOI_BIT_ONEWIRE(uint8_t bit_a_envoyer);
uint8_t LECTURE_BIT_ONEWIRE(void);

void ENVOI_OCTET_ONEWIRE(uint8_t octet);
uint8_t LECTURE_OCTET_ONEWIRE(void);


/* -------------------- Interrupt handler -------------------- */
/* Handles TIM3 CC interrupts (CC2 = D2 end, CC3 = D3 end) */
void TIM3_IRQHandler(void);

/* ---- 2.4: primitivas bloqueantes para testes ---- */
void RESET_ONEWIRE(void);
void ENVOI_BIT_ONEWIRE(uint8_t bit_a_envoyer);
uint8_t LECTURE_BIT_ONEWIRE(void);
void ENVOI_OCTET_ONEWIRE(uint8_t octet);
uint8_t LECTURE_OCTET_ONEWIRE(void);

void SKIP_ROM(void);
void CONVERT_T(void);
void READ_SCRATCHPAD(void);

void READ_POWER_SUPPLY_CMD(void);    /* envia 0xB4 */
uint8_t READ_POWER_SUPPLY_BIT(void); /* RESET+0xCC+0xB4+READ → retorna 1 (VDD) ou 0 (parasita) */



#endif /* ONEWIRE_H */
