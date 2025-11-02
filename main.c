#include "stm32f10x.h"
#include "i2c_bb.h"
#include "pcf8574.h"
#include "delay.h"
#include "io.h"

int main(void) {
	
		i2cbb_init();
		i2cbb_start();

		while (1) {
			i2cbb_write_bit(1);  // SDA liberado (alto) durante pulso de SCL
			delay_ms(2000);
		}
}
