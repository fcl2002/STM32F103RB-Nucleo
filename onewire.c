#include "onewire.h"

/* -------------------- state flags -------------------- */
volatile uint8_t g_espion_done = 0;
volatile uint8_t g_motif_done  = 0;

/* -------------------- helpers -------------------- */
static inline void _tim3_enable_irq(void) {
    NVIC_EnableIRQ(TIM3_IRQn);
}
static inline void _tim3_disable_irq(void) {
    NVIC_DisableIRQ(TIM3_IRQn);
}

/* Prescaler for 1 µs tick.
 * Assumes TIM3 clock = 72 MHz (common STM32F103 setup: APB1=36 MHz, timer clock doubled).
 * If your clock differs, adjust PSC accordingly: PSC = (f_tim / 1MHz) - 1.
 */
#define TIM3_PSC_1US   (72u - 1u)

void init_pins_onewire(void)
{
    /* Clocks básicos */
    RCC->APB2ENR |= (1u << 0); /* AFIOEN */
    io_enable_port_clock(ONEWIRE_GPIO);

    /* JTAG off, SWD on (libera PB3/PB4) */
    AFIO->MAPR = (AFIO->MAPR & ~(7u << 24)) | (2u << 24);

    /* Pinos definitivos em AF:
       - PB4 (DQ)     : AF Open-Drain
       - PB5 (ESPION) : AF Push-Pull
       (Conexão ao timer é via CCxE em init_timer/init_motif)
    */
    io_init_simple(ONEWIRE_GPIO, ONEWIRE_DQ_PIN,
                   IO_FUNC_AF_OD, IO_SPEED_50M, IO_NOPULL, IO_LEVEL_HIGH);
    io_init_simple(ONEWIRE_GPIO, ONEWIRE_SPY_PIN,
                   IO_FUNC_AF_PP, IO_SPEED_50M, IO_NOPULL, IO_LEVEL_HIGH);
}




/* -------------------- init common timer base -------------------- */
// void init_timer(void)
// {
//     RCC->APB2ENR |= (1u << 0);   /* AFIOEN */
//     RCC->APB1ENR |= (1u << 1);   /* TIM3EN */

//     AFIO->MAPR = (AFIO->MAPR & ~(3u << 10)) | (1u << 10); /* TIM3_REMAP = 01 */

//     TIM3->PSC = TIM3_PSC_1US;
//     TIM3->ARR = 0xFFFFu;

//     TIM3->CR1  = 0;
//     TIM3->DIER = 0;
//     TIM3->SR   = 0;

//     TIM3->CCER = 0; /* CC1E=0, CC2E=0 */

//     /* >>> CH1/CH2 em modo SAÍDA (CCxS=00) <<< */
//     TIM3->CCMR1 &= ~((3u<<0) | (3u<<8)); /* CC1S=00, CC2S=00 */

//     /* CH1 (DQ): manter desconectado fora do motivo (frozen é OK) */
//     TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (0u<<4);   /* OC1M=frozen */

//     /* CH2 (ESPION): HIGH de repouso => force active (101) e conectar */
//     TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (5u<<12);  /* OC2M=101 */
//     TIM3->CCER  |= TIM_CCER_CC2E;                        /* conecta CH2 */
//     TIM3->EGR    = TIM_EGR_UG;                           /* aplica HIGH já */

//     TIM3->DIER |= (TIM_DIER_CC2IE | TIM_DIER_CC3IE);
//     NVIC_EnableIRQ(TIM3_IRQn);
// }

void init_timer(void)
{
    RCC->APB2ENR |= (1u << 0);   /* AFIOEN */
    RCC->APB1ENR |= (1u << 1);   /* TIM3EN */

    /* TIM3 remap = 10 (o que funcionou na sua placa):
       CH1 -> PB4, CH2 -> PB5
    */
    AFIO->MAPR = (AFIO->MAPR & ~(3u << 10)) | (2u << 10);

    /* Timebase 1 us */
    TIM3->PSC = TIM3_PSC_1US;
    TIM3->ARR = 0xFFFFu;

    /* Estado conhecido */
    TIM3->CR1  = TIM_CR1_ARPE;   /* pré-carregamento ligado (boa prática) */
    TIM3->DIER = 0;
    TIM3->SR   = 0;

    /* Saídas inicialmente não ativas */
    TIM3->CCER = 0; /* CC1E=0 (DQ solto), CC2E=0 por enquanto */

    /* >>> CH1/CH2 em modo SAÍDA (CCxS=00) <<< */
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8)); /* CC1S=00, CC2S=00 */

    /* CH1 (DQ): manter desconectado fora do motivo (frozen é OK) */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (0u<<4);   /* OC1M=frozen */

    /* CH2 (ESPION): queremos HIGH em repouso => force active (101) e conectar */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (5u<<12);  /* OC2M=101 */
    TIM3->CCER  |= TIM_CCER_CC2E;                        /* conecta CH2 */
    TIM3->EGR    = TIM_EGR_UG;                           /* aplica HIGH já */

    /* Interrupções usadas nos testes (CC2 e CC3) */
    TIM3->DIER |= (TIM_DIER_CC2IE | TIM_DIER_CC3IE);
    NVIC_EnableIRQ(TIM3_IRQn);
}




/* -------------------- program & start a motif -------------------- */
void init_motif(uint16_t D1_us, uint16_t D2_us, uint16_t D3_us)
{
    g_espion_done = 0;
    g_motif_done  = 0;

    TIM3->CR1  &= ~TIM_CR1_CEN;      /* para timer */
    TIM3->CCER &= ~TIM_CCER_CC1E;    /* DQ desconectado (repouso via pull-up) */

    /* CH1/CH2 saída (CCxS=00) – redundante, mas garante */
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8)); /* CC1S=00, CC2S=00 */

    /* Estados iniciais do motivo:
       - DQ LOW (OC1M=100)
       - ESPION HIGH (OC2M=101)
    */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (4u<<4);   /* CH1 force LOW  */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (5u<<12);  /* CH2 force HIGH */

    TIM3->CCER |=  TIM_CCER_CC1E;   /* conecta DQ */
    TIM3->EGR   =  TIM_EGR_UG;      /* aplica níveis já */

    /* Tempos */
    TIM3->CCR1 = D1_us;   /* subida de DQ em D1 */
    TIM3->CCR2 = D2_us;   /* queda de ESPION em D2 */
    TIM3->CCR3 = D3_us;   /* fim do motivo em D3   */

    /* Zera contagem/flags e arma TOGGLE */
    TIM3->CNT  = 0;
    TIM3->SR   = 0;
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (3u<<4);   /* CH1 toggle */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (3u<<12);  /* CH2 toggle */

    TIM3->CR1 |= TIM_CR1_CEN;
}


/* -------------------- waits & flags -------------------- */
void onewire_wait_espion_done(void)
{
    while (!g_espion_done) { /* busy wait */ }
}

void onewire_wait_motif_done(void)
{
    while (!g_motif_done) { /* busy wait */ }
}

void onewire_clear_flags(void)
{
    g_espion_done = 0;
    g_motif_done  = 0;
}

/* -------------------- TIM3 ISR -------------------- */
void TIM3_IRQHandler(void)
{
    uint32_t sr = TIM3->SR;

    if (sr & TIM_SR_CC2IF) {
        TIM3->SR &= ~TIM_SR_CC2IF;
        g_espion_done = 1;
    }

    if (sr & TIM_SR_CC3IF) {
        TIM3->SR &= ~TIM_SR_CC3IF;
        g_motif_done = 1;

        /* Para timer */
        TIM3->CR1 &= ~TIM_CR1_CEN;

        /* Repouso:
           - DQ solto (desconecta CH1)
           - ESPION HIGH (force active + UG) conectado
        */
        TIM3->CCER  &= ~TIM_CCER_CC1E;                           /* DQ off */
        TIM3->CCMR1 &= ~((3u<<0) | (3u<<8));                     /* CC1S=CC2S=00 */
        TIM3->CCMR1  = (TIM3->CCMR1 & ~(7u<<12)) | (5u<<12);     /* CH2 force HIGH */
        TIM3->EGR    = TIM_EGR_UG;                               /* aplica HIGH em PB5 */
    }
}

