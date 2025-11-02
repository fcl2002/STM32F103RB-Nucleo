#include "stm32f10x.h"
#include "i2c_bb.h"
#include "pcf8574.h"
#include "delay.h"
#include "io.h"

// int main(void) {
	
// 		i2cbb_init();
// 		i2cbb_start();

// 		while (1) {
// 			i2cbb_write_bit(1);  // SDA liberado (alto) durante pulso de SCL
// 			delay_ms(2000);
// 		}
// }

static void led_init(void) {
    io_init_simple(GPIOA, 5, IO_FUNC_OUTPUT_PP, IO_SPEED_2M, IO_NOPULL, IO_LEVEL_LOW);
}

int main(void) {
    led_init();
    i2cbb_init();  // SCL/SDA como OD 2MHz e liberados em HIGH (idle)

    while (1) {
        uint8_t b = i2cbb_read_bit();   // gera 1 pulso de SCL e amostra SDA no alto
        if (b == 0) {
            // Leu '0' (SDA em LOW durante o pulso) -> LED ON
            io_write(GPIOA, 5, 0);
        } else {
            // Leu '1' (SDA em HIGH durante o pulso) -> LED OFF
            io_write(GPIOA, 5, 1);
        }
        delay_ms(2000);   // facilite a visualização/ação manual no botão

		io_write(I2CBB_SDA_PORT, I2CBB_SDA_PIN, 0u);
		delay_ms(2000);
    }
}
