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
    RCC->APB2ENR |= (1u << 0); /* AFIOEN */
    io_enable_port_clock(ONEWIRE_GPIO);

    AFIO->MAPR = (AFIO->MAPR & ~(7u << 24)) | (2u << 24); /* JTAG off, SWD on */

    io_init_simple(ONEWIRE_GPIO, ONEWIRE_DQ_PIN,
                   IO_FUNC_AF_OD, IO_SPEED_50M,
                   IO_NOPULL,
                   IO_LEVEL_HIGH);

    io_init_simple(ONEWIRE_GPIO, ONEWIRE_SPY_PIN,
                   IO_FUNC_OUTPUT_PP, IO_SPEED_50M,
                   IO_NOPULL,
                   IO_LEVEL_HIGH);
}

/* -------------------- init common timer base -------------------- */
void init_timer(void)
{
    /* Clocks */
    RCC->APB2ENR |= (1u << 0);   /* AFIOEN */
    RCC->APB1ENR |= (1u << 1);   /* TIM3EN */

    /* Free PB4 from JTAG (keep SWD) and apply TIM3 partial remap (CH1->PB4, CH2->PB5) */
    AFIO->MAPR = (AFIO->MAPR & ~(7u << 24)) | (2u << 24);          /* SWJ_CFG: JTAG-DIS, SWD-EN */
    AFIO->MAPR = (AFIO->MAPR & ~(3u << 10)) | (1u << 10);          /* TIM3_REMAP = 01 (partial) */

    /* Move pins to AF for timer control:
       - PB4 (DQ): AF Open-Drain
       - PB5 (ESPION): AF Push-Pull
    */
    io_init_simple(ONEWIRE_GPIO, ONEWIRE_DQ_PIN,
                   IO_FUNC_AF_OD, IO_SPEED_50M, IO_NOPULL, IO_LEVEL_HIGH);
    io_init_simple(ONEWIRE_GPIO, ONEWIRE_SPY_PIN,
                   IO_FUNC_AF_PP, IO_SPEED_50M, IO_NOPULL, IO_LEVEL_HIGH);

    /* Timebase: 1 µs tick, free-running up to 0xFFFF */
    TIM3->PSC = TIM3_PSC_1US;
    TIM3->ARR = 0xFFFFu;

    /* Disable and clear everything before configuring */
    TIM3->CR1 = 0;
    TIM3->CCER = 0;
    TIM3->DIER = 0;
    TIM3->SR   = 0;

    /* --- Output Compare configuration ---
     * We'll use "toggle on match" for CH1 and CH2 so:
     *  - CH1 starts forced LOW and toggles HIGH at CCR1 (D1)
     *  - CH2 starts forced HIGH and toggles LOW at CCR2 (D2)
     * CH3 is interrupt-only (no output pin).
     */

    /* Start with forced states (we switch to TOGGLE at start of a motif) */
    /* CH1: force inactive (LOW) now */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u << 4)) | (4u << 4);   /* OC1M=100 (force inactive) */
    /* CH2: force active (HIGH) now */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u << 12)) | (5u << 12); /* OC2M=101 (force active)  */

    /* Normal polarity, enable outputs */
    TIM3->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC2P);
    TIM3->CCER |=  (TIM_CCER_CC1E | TIM_CCER_CC2E);

    /* Interrupts we will use: CC2 (end of D2) and CC3 (end of D3) */
    TIM3->DIER |= (TIM_DIER_CC2IE | TIM_DIER_CC3IE);

    _tim3_enable_irq();
}

/* -------------------- program & start a motif -------------------- */
void init_motif(uint16_t D1_us, uint16_t D2_us, uint16_t D3_us)
{
    g_espion_done = 0;
    g_motif_done  = 0;

    /* Program compare points (in µs) */
    TIM3->CCR1 = D1_us;  /* DQ will rise at D1 */
    TIM3->CCR2 = D2_us;  /* ESPION will fall at D2 (and raise CC2 interrupt) */
    TIM3->CCR3 = D3_us;  /* End-of-motif interrupt */

    /* Force initial output states just before starting:
       - CH1 LOW (force inactive) then set to TOGGLE for the event at CCR1
       - CH2 HIGH (force active)  then set to TOGGLE for the event at CCR2
    */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u << 4))  | (4u << 4);  /* OC1M=force LOW  */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u << 12)) | (5u << 12); /* OC2M=force HIGH */

    /* Reset counter and clear pending flags */
    TIM3->CNT = 0;
    TIM3->SR  = 0;

    /* Now arm TOGGLE so the next compare flips the level once */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u << 4))  | (3u << 4);  /* OC1M=011 toggle */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u << 12)) | (3u << 12); /* OC2M=011 toggle */

    /* Start timer */
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

    /* End of D2 (ESPION) */
    if (sr & TIM_SR_CC2IF) {
        TIM3->SR &= ~TIM_SR_CC2IF;   /* clear */
        g_espion_done = 1;
    }

    /* End of D3 (motif) */
    if (sr & TIM_SR_CC3IF) {
        TIM3->SR &= ~TIM_SR_CC3IF;   /* clear */
        g_motif_done = 1;
        TIM3->CR1 &= ~TIM_CR1_CEN;   /* stop timer */
    }
}
