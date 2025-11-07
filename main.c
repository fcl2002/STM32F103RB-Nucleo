#include "stm32f10x.h"
#include "delay.h"
#include "io.h"
#include "onewire.h"

int main(void)
{
    /* LED (PA5) só para “batimento” visual */
    io_init_simple(GPIOA, 5, IO_FUNC_OUTPUT_PP, IO_SPEED_2M, IO_NOPULL, IO_LEVEL_LOW);

    /* 2.1: pinos (PB4/PB5) */
    init_pins_onewire();

    /* 2.2: timer — CERTIFIQUE-SE: onewire.c tem
       #define TIM3_PSC_1US (72u - 1u)   // tick = 1 µs  (TIM3 @ 72 MHz)
    */
    init_timer();

    /* Durações em µs (tick = 1 µs) — todas ≤ 65535 */
    const uint16_t D1 = 48000;  /* 48.000 µs = 48 ms  (pulso negativo do DQ) */
    const uint16_t D2 =  6000;  /*  6.000 µs =  6 ms  (ESPION) */
    const uint16_t D3 = 60000;  /* 60.000 µs = 60 ms  (fim do motivo) */

    while (1) {
        onewire_clear_flags();

        /* Programa e inicia o motivo */
        init_motif(D1, D2, D3);

        /* Opcional: aguarde o espion e dê um toque no LED */
        onewire_wait_espion_done();
        io_toggle(GPIOA, 5);

        /* Aguarde o fim do motivo (D3) */
        onewire_wait_motif_done();

        /* Batimento para saber que um ciclo terminou */
        io_toggle(GPIOA, 5);
        delay_ms(200);
    }
}
