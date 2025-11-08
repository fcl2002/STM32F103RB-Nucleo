#include "stm32f10x.h"
#include "onewire.h"
#include "io.h"
#include "delay.h"

/* ---------- helpers de marcação no osciloscópio (sem ligar o timer) ---------- */
/* Força níveis diretamente pelos comparadores (OCxM) com CEN=0 */
static void scope_mark_low_both_us(uint32_t us) {
    /* CH1/CH2 como saída */
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8));          /* CC1S=00, CC2S=00 */
    /* Force LOW nos dois canais */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (4u<<4);   /* OC1M=100: LOW */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (4u<<12);  /* OC2M=100: LOW */
    TIM3->CCER  |= (TIM_CCER_CC1E | TIM_CCER_CC2E);      /* conecta CH1/CH2 */
    TIM3->CR1   &= ~TIM_CR1_CEN;                         /* timer parado */
    TIM3->EGR    = TIM_EGR_UG;                           /* aplica agora */
    delay_us(us);
}

static void scope_mark_high_both_us(uint32_t us) {
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8));                 /* CC1S=00, CC2S=00 */
    /* Force HIGH:
       - CH1 (AF-OD) “HIGH” significa solto (pull-up mantém alto)
       - CH2 (AF-PP) dirige alto
    */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (5u<<4);   /* OC1M=101: HIGH */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (5u<<12);  /* OC2M=101: HIGH */
    TIM3->CCER  |= (TIM_CCER_CC1E | TIM_CCER_CC2E);
    TIM3->CR1   &= ~TIM_CR1_CEN;
    TIM3->EGR    = TIM_EGR_UG;
    delay_us(us);
}

int main(void)
{
    /* Pinos e timer base conforme seu onewire.c */
    init_pins_onewire();   /* PB4 = AF-OD, PB5 = AF-PP, JTAG off */
    init_timer();          /* TIM3 remap=10 (CH1->PB4, CH2->PB5), repouso PB5=HIGH */

    while (1) {
        /* --------- PREÂMBULO DE MARCAÇÃO (para saber onde o Record começou) --------- */
        scope_mark_low_both_us(2000);   /* 2 ms LOW nos dois canais */
        scope_mark_high_both_us(2000);  /* 2 ms HIGH nos dois canais */

        /* ------------------------- 1) RESET (OneWire) ------------------------- */
        /* DQ LOW 480 µs; amostra ~70 µs; fim do motivo ~960 µs */
        init_motif(/*D1=*/480, /*D2=*/70, /*D3=*/960);
        onewire_wait_motif_done();
        delay_us(20);  /* espaçamento entre motivos */

        if (etat_one_wire == 0) {
            // presence detectado (linha estava LOW em D2)
        } else {
            // ausência de presença (linha HIGH em D2)
        }

        /* ------------------------- 2) WRITE '1' ------------------------- */
        /* DQ LOW 6 µs; leitura 15 µs; slot termina em 60 µs */
        // init_motif(/*D1=*/6, /*D2=*/15, /*D3=*/60);
        // onewire_wait_motif_done();
        // delay_us(40);  /* espaçamento entre slots */

        /* ------------------------- 3) WRITE '0' ------------------------- */
        /* DQ LOW 60 µs; leitura 15 µs; slot termina em 60 µs */
        // init_motif(/*D1=*/60, /*D2=*/15, /*D3=*/60);
        // onewire_wait_motif_done();

        /* Pausa “humana” para nova captura */
        delay_ms(5);
    }
}
