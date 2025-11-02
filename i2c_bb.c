#include "i2c_bb.h"

/* ============================================================================
 * Implementação I2C bit-bang (PB6=SCL, PB7=SDA)
 * - Saída open-drain 2 MHz
 * - “1” = liberar linha (release)   -> io_write(..., 1)
 * - “0” = forçar nível baixo (drive)-> io_write(..., 0)
 * - Leitura via io_read() (IDR reflete o nível da linha)
 * - Temporização definida em i2cbb.h (I2CBB_T_*_US)
 * - Clock stretching: espera SCL subir com timeout
 * ============================================================================ */

/* Timeout máximo esperando SCL=1 após liberar a linha (em microssegundos) */
#ifndef I2CBB_STRETCH_TIMEOUT_US
#define I2CBB_STRETCH_TIMEOUT_US   1000u
#endif

/* ----------------------- Helpers de pino ----------------------- */
static inline void scl_low(void)      { io_write(I2CBB_SCL_PORT, I2CBB_SCL_PIN, 0u); }
static inline void scl_release(void)  { io_write(I2CBB_SCL_PORT, I2CBB_SCL_PIN, 1u); }
static inline void sda_low(void)      { io_write(I2CBB_SDA_PORT, I2CBB_SDA_PIN, 0u); }
static inline void sda_release(void)  { io_write(I2CBB_SDA_PORT, I2CBB_SDA_PIN, 1u); }

static inline uint8_t read_scl(void)  { return io_read(I2CBB_SCL_PORT, I2CBB_SCL_PIN); }
static inline uint8_t read_sda(void)  { return io_read(I2CBB_SDA_PORT, I2CBB_SDA_PIN); }

/* Espera SCL subir (clock stretching) ou estourar timeout. Retorna 1 se SCL==1. */
static uint8_t wait_scl_high_with_timeout(void)
{
    uint32_t t = I2CBB_STRETCH_TIMEOUT_US;
    while (t--) {
        if (read_scl()) return 1u;
        delay_us(1u);
    }
    return 0u; /* timeout */
}

/* ========================================================================== */
/* API                                                                         */
/* ========================================================================== */

void i2cbb_init(void)
{
    /* Configura SCL e SDA como saída open-drain 2 MHz, nível inicial “alto” (release).
     * Observação: idealmente usar pull-ups externos (2.2k–10k). */
    io_init_simple(I2CBB_SCL_PORT, I2CBB_SCL_PIN, IO_FUNC_OUTPUT_OD, IO_SPEED_2M, IO_PULLUP, IO_LEVEL_HIGH);
    io_init_simple(I2CBB_SDA_PORT, I2CBB_SDA_PIN, IO_FUNC_OUTPUT_OD, IO_SPEED_2M, IO_PULLUP, IO_LEVEL_HIGH);

    /* Garante barramento livre: SCL=1, SDA=1 */
    scl_release();
    sda_release();

    /* Pequena pausa para estabilizar */
    delay_us(I2CBB_T_SU_STA_US);
}

void i2cbb_start(void)
{
    /* START: SDA 1->0 enquanto SCL=1, depois derruba SCL */
    sda_release();
    scl_release();
    delay_us(I2CBB_T_SU_STA_US);
    
    sda_low();
    delay_us(I2CBB_T_HD_STA_US);
    
    scl_low();
    delay_us(I2CBB_T_LOW_US);
}

void i2cbb_stop(void)
{
    /* STOP: SDA 0->1 enquanto SCL=1 */
    sda_low();
    delay_us(I2CBB_T_SU_STO_US);
    
    scl_release();
    (void)wait_scl_high_with_timeout();
    delay_us(I2CBB_T_HIGH_US);
    
    sda_release();
    delay_us(I2CBB_T_SU_STO_US);
}

/* Escreve 1 bit (0/1) em SDA com pulso de clock em SCL */
void i2cbb_write_bit(uint8_t bit)
{
    if (bit) sda_release();
    else     sda_low();

    delay_us(I2CBB_T_SU_DAT_US);

    scl_release();
    (void)wait_scl_high_with_timeout();
    delay_us(I2CBB_T_HIGH_US);

    scl_low();
    delay_us(I2CBB_T_LOW_US);
}

/* Lê 1 bit de SDA durante o pulso de clock em SCL */
uint8_t i2cbb_read_bit(void)
{
    uint8_t bit;

    sda_release(); /* libera SDA para o slave dirigir */
    delay_us(I2CBB_T_SU_DAT_US);
    delay_ms(2000);
    
    scl_release();
    (void)wait_scl_high_with_timeout();
    delay_us(I2CBB_T_HIGH_US / 2u); /* meio período antes da amostragem */
    delay_ms(2000);
    
    bit = read_sda();
    
    delay_ms(2000);
    delay_us((I2CBB_T_HIGH_US + 1u) / 2u); /* completa o high */
    scl_low();
    delay_us(I2CBB_T_LOW_US);
    delay_ms(2000);

    return bit;
}

/* Escreve 1 byte (MSB primeiro) e lê ACK do slave. Retorna 0=ACK, 1=NACK. */
uint8_t i2cbb_write_byte(uint8_t byte)
{
    for (uint8_t m = 0x80u; m != 0u; m >>= 1) {
        i2cbb_write_bit((byte & m) ? 1u : 0u);
    }

    /* Bit de ACK do slave (slave puxa para 0). */
    return i2cbb_read_bit(); /* 0 = ACK, 1 = NACK */
}

/* Lê 1 byte (MSB primeiro) e envia ACK/NACK ao final. */
uint8_t i2cbb_read_byte(uint8_t ack)
{
    uint8_t byte = 0u;

    for (uint8_t i = 0; i < 8; ++i) {
        byte = (uint8_t)((byte << 1) | i2cbb_read_bit());
    }

    /* ACK: master envia 0; NACK: master envia 1 */
    i2cbb_write_bit(ack ? 1u : 0u);

    return byte;
}
