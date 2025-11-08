/* main.c – testes TP5 OneWire
 * AD2: CH1 = PB4 (DQ) – laranja; CH2 = PB5 (ESPION) – azul
 * LED: PA5 (LD2)
 */

#include "stm32f10x.h"
#include "onewire.h"
#include "io.h"
#include "delay.h"
#include "serial.h"

/* ========================= SELETOR DE TESTE ========================= */
#define TEST_RESET           1
#define TEST_WRITE1          2
#define TEST_WRITE0          3
#define TEST_READ            4
#define TEST_SEND_BYTE_A4    5   /* envia 0xA4 (LSB-first) */
#define TEST_READ_ROM        6   /* 0x33 → lê 8 bytes ROM */
#define TEST_READ_SCRATCHPAD 7   /* 0xCC 0xBE → lê 9 bytes */
#define TEST_READ_PWR_SUPPLY 8   /* 0xCC 0xB4 → lê 1 bit */
#define TEST_SCOPE_CMDS      9   /* envia 0x33, 0xCC, 0xBE para ver no AD2 */
#define TEST_CONVERT_T       10  /* converte temperatura e lê scratchpad */
#define TEST_GPIO_PROBE      11  /* Sonda o fio: LED reflete PB4 (0=acende) */
#define TEST_BITBANG_READ_SP 12  /* 1-Wire por GPIO: RESET, 0xCC, 0xBE, lê 9 bytes */

/* escolha aqui UMA função de teste */
#define TEST_FUNC TEST_SEND_BYTE_A4
/* ==================================================================== */

#define USE_SCOPE_MARKERS 1

/* ===== LED da NUCLEO-F103RB (LD2) ===== */
#define LED_GPIO   GPIOA
#define LED_PIN    5u
static inline void led_init(void) {
    io_enable_port_clock(LED_GPIO);
    io_init_simple(LED_GPIO, LED_PIN, IO_FUNC_OUTPUT_PP, IO_SPEED_2M, IO_NOPULL, IO_LEVEL_LOW);
}
static inline void led_on(void)        { io_write(LED_GPIO, LED_PIN, 1); }
static inline void led_off(void)       { io_write(LED_GPIO, LED_PIN, 0); }
static inline void led_set(uint8_t on) { io_write(LED_GPIO, LED_PIN, on ? 1 : 0); }

/* ---------- Marcadores no AD2 ---------- */
#if USE_SCOPE_MARKERS
static void scope_mark_low_both_us(uint32_t us) {
    TIM3->CCMR1 &= ~((3u<<0)|(3u<<8));
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))| (4u<<4);
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12))| (4u<<12);
    TIM3->CCER |= (TIM_CCER_CC1E|TIM_CCER_CC2E);
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->EGR = TIM_EGR_UG;
    delay_us(us);
}
static void scope_mark_high_both_us(uint32_t us) {
    TIM3->CCMR1 &= ~((3u<<0)|(3u<<8));
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<4))| (5u<<4);
    TIM3->CCMR1 = (TIM3->CCMR1 & ~(7u<<12))| (5u<<12);
    TIM3->CCER |= (TIM_CCER_CC1E|TIM_CCER_CC2E);
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->EGR = TIM_EGR_UG;
    delay_us(us);
}
#endif

/* ---------- print helpers ---------- */
static void print_hex8(uint8_t b){
    const char* h="0123456789ABCDEF";
    char s[3]={h[(b>>4)&0xF],h[b&0xF],0}; serial2_puts(s);
}
static void print_hex8_sp(uint8_t b){ print_hex8(b); serial2_putc(' '); }

static uint8_t crc8_maxim(const uint8_t *data,int len){
    uint8_t crc=0;
    for(int i=0;i<len;i++){
        uint8_t in=data[i];
        for(int b=0;b<8;b++){
            uint8_t mix=(crc^in)&1; crc>>=1;
            if(mix) crc^=0x8C; in>>=1;
        }
    }
    return crc;
}

/* ========== Funções de teste ========= */
static void run_test_reset_once(void){
    RESET_ONEWIRE();
    uint8_t presence=(etat_one_wire==0u);
    led_set(presence);
    serial2_puts("[RESET] presence=");
    serial2_puts(presence?"YES\r\n":"NO\r\n");
    delay_us(OW_TREC_US);
}

static void run_test_write1_once(void){
    ENVOI_BIT_ONEWIRE(1);
    delay_us(OW_TREC_US);
    led_on();delay_ms(20);led_off();
    serial2_puts("[WRITE] bit=1\r\n");
}
static void run_test_write0_once(void){
    ENVOI_BIT_ONEWIRE(0);
    delay_us(OW_TREC_US);
    led_on();delay_ms(60);led_off();
    serial2_puts("[WRITE] bit=0\r\n");
}
static void run_test_read_once(void){
    uint8_t bit=LECTURE_BIT_ONEWIRE();
    delay_us(OW_TREC_US);
    led_set(bit==0?1:0);
    serial2_puts("[READ] bit=");serial2_putc(bit?'1':'0');serial2_puts("\r\n");
}
static void run_test_send_byte_a4_once(void){
    RESET_ONEWIRE(); delay_us(OW_TREC_US);
    ENVOI_OCTET_ONEWIRE(0xA4);
    delay_us(OW_TREC_US);
    serial2_puts("[SEND BYTE] 0xA4\r\n");
}
static void run_test_read_rom_once(void){
    uint8_t rom[8];
    RESET_ONEWIRE(); delay_us(OW_TREC_US);
    ENVOI_OCTET_ONEWIRE(0x33); delay_us(OW_TREC_US);
    for(int i=0;i<8;i++){ rom[i]=LECTURE_OCTET_ONEWIRE(); delay_us(OW_TREC_US); }
    serial2_puts("[READ ROM] ");for(int i=0;i<8;i++)print_hex8_sp(rom[i]);serial2_puts("\r\n");
}
static void run_test_read_pwr_once(void){
    uint8_t vdd=READ_POWER_SUPPLY_BIT();
    led_set(vdd?1:0);
    serial2_puts("[READ POWER SUPPLY] ");
    serial2_puts(vdd?"VDD_EXTERNAL (1)\r\n":"PARASITIC_POWER (0)\r\n");
    delay_us(OW_TREC_US);
}

/* ---------- SCOPE: 0x33,0xCC,0xBE ---------- */
static void pulse_mark_byte_start(void){
    TIM3->CCMR1=(TIM3->CCMR1&~(7u<<12))|(5u<<12);TIM3->EGR=TIM_EGR_UG;delay_us(5);
    TIM3->CCMR1=(TIM3->CCMR1&~(7u<<12))|(4u<<12);TIM3->EGR=TIM_EGR_UG;delay_us(5);
}
static void run_test_scope_cmds_once(void){
    RESET_ONEWIRE();delay_us(OW_TREC_US);
    pulse_mark_byte_start();ENVOI_OCTET_ONEWIRE(0x33);serial2_puts("[SCOPE] 0x33\r\n");
    delay_ms(3);

    RESET_ONEWIRE();delay_us(OW_TREC_US);
    pulse_mark_byte_start();ENVOI_OCTET_ONEWIRE(0xCC);serial2_puts("[SCOPE] 0xCC\r\n");
    delay_ms(3);

    RESET_ONEWIRE();delay_us(OW_TREC_US);
    pulse_mark_byte_start();ENVOI_OCTET_ONEWIRE(0xCC);delay_us(OW_TREC_US);
    pulse_mark_byte_start();ENVOI_OCTET_ONEWIRE(0xBE);
    serial2_puts("[SCOPE] 0xCC 0xBE\r\n");
}

/* ---------- READ SCRATCHPAD ---------- */
static void run_test_read_scratchpad_once(void){
    uint8_t sp[9];
    RESET_ONEWIRE(); delay_us(OW_TREC_US);
    SKIP_ROM(); delay_us(OW_TREC_US);
    READ_SCRATCHPAD(); delay_us(OW_TREC_US);
    for(int i=0;i<9;i++){ sp[i]=LECTURE_OCTET_ONEWIRE(); delay_us(OW_TREC_US); }
    uint8_t crc=crc8_maxim(sp,9);
    serial2_puts("[READ SCRATCHPAD] ");for(int i=0;i<9;i++)print_hex8_sp(sp[i]);
    serial2_puts("\r\nCRC = ");serial2_puts(crc==0?"OK\r\n":"FAIL\r\n");
    led_set(crc==0?1:0);delay_ms(60);led_off();
}

/* ---------- CONVERT T + READ SCRATCHPAD ---------- */
static int16_t ds18b20_raw_temp_to_q4(uint8_t lsb,uint8_t msb){
    return (int16_t)((msb<<8)|lsb);
}
static void run_test_convert_t_once(void){
    RESET_ONEWIRE();delay_us(OW_TREC_US);
    SKIP_ROM();delay_us(OW_TREC_US);
    ENVOI_OCTET_ONEWIRE(0x44);delay_ms(800); /* t_conv */

    uint8_t sp[9];
    RESET_ONEWIRE();delay_us(OW_TREC_US);
    SKIP_ROM();delay_us(OW_TREC_US);
    READ_SCRATCHPAD();delay_us(OW_TREC_US);
    for(int i=0;i<9;i++){ sp[i]=LECTURE_OCTET_ONEWIRE(); delay_us(OW_TREC_US); }

    uint8_t crc=crc8_maxim(sp,9);
    int16_t raw=ds18b20_raw_temp_to_q4(sp[0],sp[1]);
    float temp_c=(float)raw*0.0625f;

    serial2_puts("[CONVERT T + READ SP] ");
    for(int i=0;i<9;i++)print_hex8_sp(sp[i]);
    serial2_puts("\r\nCRC=");serial2_puts(crc==0?"OK":"FAIL");
    serial2_puts(" | Temp=");char buf[16];sprintf(buf,"%+.2f C\r\n",temp_c);
    serial2_puts(buf);
    led_set(crc==0?1:0);delay_ms(80);led_off();
}

/* ================================ main ================================ */
int main(void){
    led_init();serial2_init();serial2_puts("\r\n=== TP5 OneWire Test ===\r\n");
    init_pins_onewire();init_timer();
#if USE_SCOPE_MARKERS
    scope_mark_low_both_us(2000);scope_mark_high_both_us(2000);
#endif
    while(1){
    #if   (TEST_FUNC==TEST_RESET)
        run_test_reset_once();
    #elif (TEST_FUNC==TEST_WRITE1)
        run_test_write1_once();
    #elif (TEST_FUNC==TEST_WRITE0)
        run_test_write0_once();
    #elif (TEST_FUNC==TEST_READ)
        run_test_read_once();
    #elif (TEST_FUNC==TEST_SEND_BYTE_A4)
        run_test_send_byte_a4_once();
    #elif (TEST_FUNC==TEST_READ_ROM)
        run_test_read_rom_once();
    #elif (TEST_FUNC==TEST_READ_SCRATCHPAD)
        run_test_read_scratchpad_once();
    #elif (TEST_FUNC==TEST_READ_PWR_SUPPLY)
        run_test_read_pwr_once();
    #elif (TEST_FUNC==TEST_SCOPE_CMDS)
        run_test_scope_cmds_once();
    #elif (TEST_FUNC==TEST_CONVERT_T)
        run_test_convert_t_once();
    #else
      #error "Selecione um TEST_FUNC válido"
    #endif
        delay_ms(20);
    }
}
