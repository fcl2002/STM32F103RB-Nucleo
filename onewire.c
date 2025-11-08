#include "onewire.h"
#include "delay.h" 

/* -------------------- state / flags -------------------- */
volatile uint8_t etat_one_wire       = 1;  /* amostra de DQ em D2 (0/1) */
volatile uint8_t motif_one_wire_fini = 0;  /* 0 = em curso, 1 = terminado */

/* -------------------- helpers -------------------- */
static inline void _tim3_enable_irq(void)  { NVIC_EnableIRQ(TIM3_IRQn); }
static inline void _tim3_disable_irq(void) { NVIC_DisableIRQ(TIM3_IRQn); }

/* 1 µs/tick @ 72 MHz (APB1=36 MHz com x2 nos timers) */
#define TIM3_PSC_1US   (72u - 1u)

/* ========================================================================== */
/* 2.1  Pinos: PB4 = DQ (AF-OD), PB5 = ESPION (AF-PP), JTAG off               */
/* ========================================================================== */
void init_pins_onewire(void)
{
    RCC->APB2ENR |= (1u << 0); /* AFIOEN */
    io_enable_port_clock(ONEWIRE_GPIO);

    /* Libera PB3/PB4: JTAG off, SWD on */
    AFIO->MAPR = (AFIO->MAPR & ~(7u << 24)) | (2u << 24);

    /* PB4 (DQ) -> AF Open-Drain; PB5 (ESPION) -> AF Push-Pull */
    io_init_simple(ONEWIRE_GPIO, ONEWIRE_DQ_PIN,
                   IO_FUNC_AF_OD, IO_SPEED_50M, IO_NOPULL, IO_LEVEL_HIGH);
    io_init_simple(ONEWIRE_GPIO, ONEWIRE_SPY_PIN,
                   IO_FUNC_AF_PP, IO_SPEED_50M, IO_NOPULL, IO_LEVEL_HIGH);
}

/* ========================================================================== */
/* 2.2  Timer base: remap=0b10 (CH1->PB4, CH2->PB5), tick=1 µs                */
/*      ESPION em repouso = LOW (para ter pulso POSITIVO 0→D2)                */
/* ========================================================================== */
void init_timer(void)
{
    RCC->APB2ENR |= (1u << 0);   /* AFIOEN */
    RCC->APB1ENR |= (1u << 1);   /* TIM3EN */

    /* TIM3 remap = 10: CH1->PB4, CH2->PB5 */
    AFIO->MAPR = (AFIO->MAPR & ~(3u << 10)) | (2u << 10);

    /* Timebase 1 µs, ARR livre */
    TIM3->PSC = TIM3_PSC_1US;
    TIM3->ARR = 0xFFFFu;

    /* Estado conhecido */
    TIM3->CR1  = TIM_CR1_ARPE;
    TIM3->DIER = 0;
    TIM3->SR   = 0;
    TIM3->CCER = 0;

    /* Canais como saída (CCxS=00) */
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8)); /* CC1S=00, CC2S=00 */

    /* CH1 (DQ): frozen fora do motivo (repouso = solto via pull-up) */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (0u<<4);   /* OC1M=frozen */

    /* CH2 (ESPION): repouso = LOW (OC2M=100) + conecta saída */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (4u<<12);  /* OC2M=100 (LOW) */
    TIM3->CCER  |= TIM_CCER_CC2E;
    TIM3->EGR    = TIM_EGR_UG;                           /* aplica LOW já */

    /* ITs de D2 (CC2) e D3 (CC3) */
    TIM3->DIER |= (TIM_DIER_CC2IE | TIM_DIER_CC3IE);
    _tim3_enable_irq();
}

/* ========================================================================== */
/* 2.2  Motif:                                                                */
/*      - CH1 começa LOW e sobe em D1 (toggle)                                */
/*      - CH2 começa HIGH e cai em D2 (toggle) → pulso POSITIVO largura D2     */
/*      - CH3 dispara IT em D3; ISR para timer e volta repouso (LOW)          */
/* ========================================================================== */
void init_motif(uint16_t D1_us, uint16_t D2_us, uint16_t D3_us)
{
    motif_one_wire_fini = 0;

    /* Para timer e solta DQ */
    TIM3->CR1  &= ~TIM_CR1_CEN;
    TIM3->CCER &= ~TIM_CCER_CC1E;           /* DQ desconectado */

    /* Canais como saída */
    TIM3->CCMR1 &= ~((3u<<0) | (3u<<8));    /* CC1S=00, CC2S=00 */

    /* Estados iniciais do motivo:
       - DQ (CH1)     = LOW  (OC1M=100)
       - ESPION (CH2) = HIGH (OC2M=101)  → com toggle em D2, faz pulso POSITIVO 0→D2
    */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (4u<<4);   /* CH1 force LOW  */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (5u<<12);  /* CH2 force HIGH */

    /* Conecta DQ e aplica níveis nos pinos */
    TIM3->CCER |= TIM_CCER_CC1E;
    TIM3->EGR   = TIM_EGR_UG;

    /* Programa tempos e zera contador/flags */
    TIM3->CCR1 = D1_us;    /* subida de DQ em D1 */
    TIM3->CCR2 = D2_us;    /* queda de ESPION em D2 (fim do pulso +) */
    TIM3->CCR3 = D3_us;    /* fim do motivo em D3 */
    TIM3->CNT  = 0;
    TIM3->SR   = 0;

    /* Arma TOGGLE em CH1/CH2 para gerar as bordas únicas */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))  | (3u<<4);   /* CH1 toggle */
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12)) | (3u<<12);  /* CH2 toggle */

    /* Inicia o timer */
    TIM3->CR1 |= TIM_CR1_CEN;
}

/* ========================================================================== */
/* Espera bloqueante do fim do motivo (2.2/2.3)                               */
/* ========================================================================== */
void onewire_wait_motif_done(void)
{
    while (!motif_one_wire_fini) { /* busy wait */ }
}

void onewire_clear_flags(void)
{
    motif_one_wire_fini = 0;
}

/* ========================================================================== */
/* 2.3  ISR do TIM3:                                                          */
/*      - CC2IF (D2): amostra DQ (PB4) e segue                                */
/*      - CC3IF (D3): para timer, repouso LOW e sinaliza fim                  */
/* ========================================================================== */
void TIM3_IRQHandler(void)
{
    uint32_t sr = TIM3->SR;

    /* D2: janela de leitura → ler DQ (PB4) */
    if (sr & TIM_SR_CC2IF) {
        TIM3->SR &= ~TIM_SR_CC2IF;     /* acquit */
        etat_one_wire = ((ONEWIRE_GPIO->IDR >> ONEWIRE_DQ_PIN) & 1u);
    }

    /* D3: fim do motivo */
    if (sr & TIM_SR_CC3IF) {
        TIM3->SR &= ~TIM_SR_CC3IF;     /* acquit */

        motif_one_wire_fini = 1;

        /* Para timer e solta DQ */
        TIM3->CR1  &= ~TIM_CR1_CEN;
        TIM3->CCER &= ~TIM_CCER_CC1E;  /* DQ off (repouso via pull-up) */

        /* Garante saída e repouso do ESPION = LOW (para próximo pulso positivo) */
        TIM3->CCMR1 &= ~((3u<<0) | (3u<<8));              /* CC1S=CC2S=00 */
        TIM3->CCMR1  = (TIM3->CCMR1 & ~(7u<<12)) | (4u<<12); /* CH2 force LOW */
        TIM3->EGR    = TIM_EGR_UG;                        /* aplica no pino */
    }
}


/* RESET: gera motivo e deixa 1 ms entre motivos */
void RESET_ONEWIRE(void)
{
    onewire_clear_flags();

    /* D1=480 µs (master segura LOW), amostra ~70 µs após soltar (D2=D1+70),
       D3 ~ 960 µs fecha o motivo. */
    init_motif(/*D1=*/480, /*D2=*/480+70, /*D3=*/960);
    onewire_wait_motif_done();

    /* espaçamento entre motivos */
    delay_us(1000);
}

/* ENVOI_BIT_ONEWIRE: envia '1' (LOW curto) ou '0' (LOW longo) */
void ENVOI_BIT_ONEWIRE(uint8_t bit_a_envoyer)
{
    onewire_clear_flags();

    if (bit_a_envoyer) {
        /* WRITE '1': LOW curto, solta ≤15 µs; slot ≥60 µs */
        init_motif(/*D1=*/OW_W1_TLOW,
                   /*D2=*/OW_R_TSAMPLE,     /* não usado p/ write, ok */
                   /*D3=*/OW_W1_TEND);
    } else {
        /* WRITE '0': manter LOW ~60 µs; slot ≥60 µs */
        init_motif(/*D1=*/OW_W0_TLOW,
                   /*D2=*/OW_R_TSAMPLE,     /* não usado p/ write, ok */
                   /*D3=*/OW_W0_TEND);
    }

    onewire_wait_motif_done();
    delay_us(OW_TREC_US);  /* TREC ≥1 µs */
}

/* LECTURE_BIT_ONEWIRE: master puxa curto e amostra em ~15 µs.
   Retorna 0/1 conforme o escravo manteve a linha em LOW/HIGH na janela. */
uint8_t LECTURE_BIT_ONEWIRE(void)
{
    onewire_clear_flags();

    /* READ: “kick” curto (≥1 µs), sample @15 µs, slot ≥60 µs */
    init_motif(/*D1=*/OW_R_TINIT,
               /*D2=*/OW_R_TSAMPLE,
               /*D3=*/OW_R_TEND);
    onewire_wait_motif_done();

    /* etat_one_wire foi capturado na ISR no instante D2 */
    uint8_t bit_lu = (etat_one_wire ? 1u : 0u);

    delay_us(OW_TREC_US);  /* TREC ≥1 µs */
    return bit_lu;
}

/* -------------------------------------------------------------------------- */
/* Envoi d’un octet complet (bloquant)                                        */
/*  - bit 0 envoyé en premier (LSB-first, conforme au protocole DS18B20)      */
/* -------------------------------------------------------------------------- */
void ENVOI_OCTET_ONEWIRE(uint8_t octet)
{
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t bit = (octet >> i) & 0x01u;  /* bit LSB -> MSB */
        ENVOI_BIT_ONEWIRE(bit);
    }
}

/* -------------------------------------------------------------------------- */
/* Lecture d’un octet complet (bloquant)                                      */
/*  - bit 0 reçu en premier (LSB-first)                                       */
/* -------------------------------------------------------------------------- */
uint8_t LECTURE_OCTET_ONEWIRE(void)
{
    uint8_t octet = 0;

    for (uint8_t i = 0; i < 8; i++) {
        uint8_t bit = LECTURE_BIT_ONEWIRE();
        if (bit) {
            octet |= (1u << i);   /* reconstrução LSB-first */
        }
    }
    return octet;
}

void SKIP_ROM(void)        { ENVOI_OCTET_ONEWIRE(0xCC); }
void CONVERT_T(void)       { ENVOI_OCTET_ONEWIRE(0x44); }
void READ_SCRATCHPAD(void) { ENVOI_OCTET_ONEWIRE(0xBE); }
/* ========================================================================== */

/* Envia o comando 0xB4 (Read Power Supply) */
void READ_POWER_SUPPLY_CMD(void)
{
    ENVOI_OCTET_ONEWIRE(0xB4);
}

/* Sequência completa: RESET → SKIP ROM (0xCC) → 0xB4 → ler 1 bit.
   Esperado: 1 (VDD externo). */
uint8_t READ_POWER_SUPPLY_BIT(void)
{
    RESET_ONEWIRE();
    delay_us(20);             /* pequeno guard time entre reset e comando */
    SKIP_ROM();               /* 0xCC */
    READ_POWER_SUPPLY_CMD();  /* 0xB4 */
    return LECTURE_BIT_ONEWIRE();
}
