#include "stm32f10x.h"
#include "onewire.h"
#include "io.h"
#include "delay.h"

/* ---------- helpers de marcação no osciloscópio (sem ligar o timer) ---------- */
/* (copiei exatamente as tuas funções, sem mudar nada) */
static void scope_mark_low_both_us(uint32_t us) {
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8));
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (4u<<4);
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (4u<<12);
    TIM3->CCER  |= (TIM_CCER_CC1E | TIM_CCER_CC2E);
    TIM3->CR1   &= ~TIM_CR1_CEN;
    TIM3->EGR    = TIM_EGR_UG;
    delay_us(us);
}

static void scope_mark_high_both_us(uint32_t us) {
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8));
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (5u<<4);
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (5u<<12);
    TIM3->CCER  |= (TIM_CCER_CC1E | TIM_CCER_CC2E);
    TIM3->CR1   &= ~TIM_CR1_CEN;
    TIM3->EGR    = TIM_EGR_UG;
    delay_us(us);
}

/* Escolha do teste (troque o valor e recompile) */
#define TEST_RESET   0
#define TEST_WRITE1  1
#define TEST_WRITE0  2
#define TEST_READ    3

#define TEST_MODE TEST_RESET   /* <<< MUDE AQUI: RESET, WRITE1, WRITE0, READ */

int main(void)
{
    /* LED PA5 para visualizar presença / bits lidos */
    io_init_simple(GPIOA, 5, IO_FUNC_OUTPUT_PP, IO_SPEED_2M, IO_NOPULL, IO_LEVEL_LOW);

    /* Pinos e timer base conforme seu onewire.c */
    init_pins_onewire();   /* PB4 = AF-OD, PB5 = AF-PP, JTAG off */
    init_timer();          /* TIM3 remap=10 (CH1->PB4, CH2->PB5), repouso PB5=HIGH */

    while (1) {

        /* --------- PREÂMBULO: 2 ms LOW + 2 ms HIGH nos dois canais --------- */
        scope_mark_low_both_us(2000);   /* 2 ms LOW */
        scope_mark_high_both_us(2000);  /* 2 ms HIGH */

        /* ------------------- Escolha do teste ------------------- */
#if (TEST_MODE == TEST_RESET)

        /* 1) RESET_ONEWIRE: presença do sensor */
        RESET_ONEWIRE();  /* etat_one_wire atualizado na ISR em OW_RESET_TSAMPLE */

        /* LED indica presença:
           - LOW  => presence detectado (linha LOW em D2)
           - HIGH => ausência de presence
         */
        if (etat_one_wire == 0) {
            io_write(GPIOA, 5, 0);  /* presence => LED apagado */
        } else {
            io_write(GPIOA, 5, 1);  /* sem presence => LED aceso */
        }

#elif (TEST_MODE == TEST_WRITE1)

        /* 2) ENVOI_BIT_ONEWIRE(1) */
        ENVOI_BIT_ONEWIRE(1);

#elif (TEST_MODE == TEST_WRITE0)

        /* 3) ENVOI_BIT_ONEWIRE(0) */
        ENVOI_BIT_ONEWIRE(0);

#elif (TEST_MODE == TEST_READ)

        /* 4) LECTURE_BIT_ONEWIRE() */
        {
            uint8_t bit = LECTURE_BIT_ONEWIRE();

            /* LED pisca conforme o bit lido:
               - bit = 1 -> piscadas rápidas
               - bit = 0 -> piscadas lentas
             */
            for (int i = 0; i < 4; i++) {
                io_toggle(GPIOA, 5);
                delay_ms(bit ? 100 : 300);
            }
        }

#endif

        /* Pausa “humana” entre motivos (além dos 1 ms internos) */
        delay_ms(5);
    }
}
