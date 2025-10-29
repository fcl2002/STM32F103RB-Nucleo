#include "stm32f10x.h"
#include "i2c_bb.h"
#include "pcf8574.h"
#include "delay.h"

int main(void)
{
    uint8_t addr = PCF8574_ADDR(0,0,0); // 0x20
    uint8_t val = 0xFF;
    uint8_t read_val = 0;

    i2cbb_init();

    while (1)
    {
        // Escreve todos 0 -> todos pinos em LOW
        pcf8574_write(addr, 0x00);
        delay_ms(100);

        // Escreve todos 1 -> todos pinos em HIGH
        pcf8574_write(addr, 0xFF);
        delay_ms(100);

        // Leitura (modo quasi-bidirectional)
        pcf8574_write(addr, 0xFF);  // libera linhas
        pcf8574_read(addr, &read_val);
        (void)read_val;             // coloque breakpoint se quiser examinar
        delay_ms(200);
    }
}
