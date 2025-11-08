#include "stm32f10x.h"
#include "onewire.h"
#include "io.h"
#include "delay.h"

/* ========================= SELETOR DE TESTE ========================= */
#define TEST_RESET   1
#define TEST_WRITE1  2
#define TEST_WRITE0  3
#define TEST_READ    4

/* escolha aqui UMA função de teste */
#define TEST_FUNC TEST_WRITE1
/* ==================================================================== */

/* Habilita preâmbulo de marcação no AD2 (CH1=PB4, CH2=PB5) */
#define USE_SCOPE_MARKERS 1

/* ===== LED da NUCLEO-F103RB (LD2) =====
 * NUCLEO-64 → LD2 em PA5.
 */
#define LED_GPIO   GPIOA
#define LED_PIN    5u

static inline void led_init(void) {
    io_enable_port_clock(LED_GPIO);
    io_init_simple(LED_GPIO, LED_PIN, IO_FUNC_OUTPUT_PP, IO_SPEED_2M, IO_NOPULL, IO_LEVEL_LOW);
}
static inline void led_on(void)        { io_write(LED_GPIO, LED_PIN, 1); }
static inline void led_off(void)       { io_write(LED_GPIO, LED_PIN, 0); }
static inline void led_set(uint8_t on) { io_write(LED_GPIO, LED_PIN, on ? 1 : 0); }

/* ---------- Marcadores no AD2 (sem iniciar o timer) ---------- */
#if USE_SCOPE_MARKERS
static void scope_mark_low_both_us(uint32_t us) {
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8));          /* CC1S=00, CC2S=00 */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (4u<<4);   /* CH1 LOW */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (4u<<12);  /* CH2 LOW */
    TIM3->CCER  |= (TIM_CCER_CC1E | TIM_CCER_CC2E);
    TIM3->CR1   &= ~TIM_CR1_CEN;
    TIM3->EGR    = TIM_EGR_UG;
    delay_us(us);
}
static void scope_mark_high_both_us(uint32_t us) {
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8));                 /* CC1S=00, CC2S=00 */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (5u<<4);   /* CH1 HIGH (solto em AF-OD) */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (5u<<12);  /* CH2 HIGH (push-pull) */
    TIM3->CCER  |= (TIM_CCER_CC1E | TIM_CCER_CC2E);
    TIM3->CR1   &= ~TIM_CR1_CEN;
    TIM3->EGR    = TIM_EGR_UG;
    delay_us(us);
}
#endif

/* ========= wrappers de execução única por teste ========= */
static void run_test_reset_once(void) {
    /* Executa um único RESET e usa LED como indicador de presença */
    RESET_ONEWIRE();
    /* etat_one_wire == 0 → presence detectado */
    led_set(etat_one_wire == 0 ? 1 : 0);
}

static void run_test_write1_once(void) {
    ENVOI_BIT_ONEWIRE(1);
    /* feedback visual curto só para marcar que passou por aqui */
    led_on();  delay_ms(20);  led_off();
}

static void run_test_write0_once(void) {
    ENVOI_BIT_ONEWIRE(0);
    /* feedback visual diferente */
    led_on();  delay_ms(60);  led_off();
}

static void run_test_read_once(void) {
    uint8_t bit = LECTURE_BIT_ONEWIRE();
    /* LED reflete o bit lido (opcional):
       - aceso para '0' (escravo puxou LOW na janela)
       - apagado para '1' */
    led_set(bit == 0 ? 1 : 0);
}

int main(void)
{
    /* Inicializações */
    led_init();
    init_pins_onewire();   /* PB4 = AF-OD, PB5 = AF-PP, JTAG off */
    init_timer();          /* TIM3 remap=10, 1 µs/tick, IT CC2/CC3 habilitadas */

#if USE_SCOPE_MARKERS
    /* Preamble de marcação pro Logic Analyzer (não interfere no motivo) */
    scope_mark_low_both_us(2000);
    scope_mark_high_both_us(2000);
#endif

    while (1) {
    #if   (TEST_FUNC == TEST_RESET)
        run_test_reset_once();
        /* recomende 1 ms entre motivos conforme TP */
        delay_ms(1);

    #elif (TEST_FUNC == TEST_WRITE1)
        run_test_write1_once();
        delay_ms(1);

    #elif (TEST_FUNC == TEST_WRITE0)
        run_test_write0_once();
        delay_ms(1);

    #elif (TEST_FUNC == TEST_READ)
        run_test_read_once();
        delay_ms(1);

    #else
      #error "Selecione um TEST_FUNC válido (TEST_RESET/TEST_WRITE1/TEST_WRITE0/TEST_READ)"
    #endif

        /* pausa humana entre iterações para facilitar a captura */
        delay_ms(8);
    }
}
