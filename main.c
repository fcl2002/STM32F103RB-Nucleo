#include "stm32f10x.h"
#include "io.h"
#include "delay.h"
#include "serial.h"

int main(void)
{
    /* Configura LED em PA5 (output push-pull 2MHz, inicial LOW) */
    io_init_simple(GPIOA, 5, IO_FUNC_OUTPUT_PP, IO_SPEED_2M, IO_NOPULL, IO_LEVEL_LOW);

    /* Inicializa USART2 em 9600 8N1 (PA2=TX, PA3=RX) */
    serial2_init();

    /* Mensagem inicial */
    serial2_puts("\r\n=== USART2 Teste Iniciado ===\r\n");
    serial2_puts("Digite algo e veja o eco!\r\n");

    while (1)
    {
        /* Se há dado recebido, leia e ecoe */
        if (serial2_readable())
        {
            char c = serial2_getc();
            serial2_putc(c);      // ecoa no terminal
            io_toggle(GPIOA, 5);  // pisca LED
        }
    }
}
