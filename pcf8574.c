#include "pcf8574.h"
#include "i2c_bb.h"

uint8_t pcf8574_write(uint8_t addr7, uint8_t value)
{
    /* START + (addr<<1 | W=0) + data + STOP
     * i2cbb_write_byte retorna 0=ACK, 1=NACK
     */
    uint8_t ack1, ack2;

    i2cbb_start();
    ack1 = i2cbb_write_byte((uint8_t)((addr7 << 1) | 0u)); /* bit R/W = 0 (write) */
    ack2 = i2cbb_write_byte(value);
    i2cbb_stop();

    /* Sucesso se ambos deram ACK (0) */
    return (ack1 == 0u && ack2 == 0u) ? PCF8574_OK : PCF8574_FAIL;
}

uint8_t pcf8574_read(uint8_t addr7, uint8_t *out)
{
    uint8_t ack_addr;
    uint8_t data = 0u;

    i2cbb_start();
    ack_addr = i2cbb_write_byte((uint8_t)((addr7 << 1) | 1u)); /* bit R/W = 1 (read) */
    if (ack_addr != 0u) {
        i2cbb_stop();
        return PCF8574_FAIL;  /* NACK no endereço */
    }

    /* Lê 1 byte e envia NACK para encerrar (último byte) */
    data = i2cbb_read_byte(I2CBB_NACK);
    i2cbb_stop();

    if (out) {
        *out = data;
    }

    return PCF8574_OK;
}
