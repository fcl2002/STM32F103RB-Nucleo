#include "io.h"

/* -------------------------------------------------------------------------- */
/* Helpers internos                                                           */
/* -------------------------------------------------------------------------- */

/* Mapeia GPIOx -> bit de enable no RCC->APB2ENR (IOPA..IOPE = bits 2..6) */
static uint32_t io__rcc_gpio_en_bit(GPIO_TypeDef *port)
{
    if (port == GPIOA) return (1u << 2);
    if (port == GPIOB) return (1u << 3);
    if (port == GPIOC) return (1u << 4);
    if (port == GPIOD) return (1u << 5);
    if (port == GPIOE) return (1u << 6);
    return 0u; /* porta inválida para F103RB */
}

/* Seleciona ponteiro para CRL/CRH e deslocamento do nibble */
static volatile uint32_t* io__cr_reg(GPIO_TypeDef *port, uint8_t pin, uint8_t *shift4)
{
    if (pin < 8u) {
        *shift4 = (uint8_t)(pin * 4u);
        return &port->CRL;
    } else {
        *shift4 = (uint8_t)((pin - 8u) * 4u);
        return &port->CRH;
    }
}

/* Constrói cfg para saídas/AF a partir de func+speed (ignora entradas) */
uint8_t io_build_out_cfg(io_func_t func, io_speed_t speed)
{
    uint8_t mode;
    switch (speed) {
        case IO_SPEED_10M: mode = 0x1; break; /* MODE=01 */
        case IO_SPEED_2M : mode = 0x2; break; /* MODE=10 */
        default          : mode = 0x3; break; /* IO_SPEED_50M: MODE=11 */
    }

    switch (func) {
        case IO_FUNC_OUTPUT_PP: return (0x0 | mode);     /* PP  */
        case IO_FUNC_OUTPUT_OD: return (0x4 | mode);     /* OD  */
        case IO_FUNC_AF_PP    : return (0x8 | mode);     /* AF-PP */
        case IO_FUNC_AF_OD    : return (0xC | mode);     /* AF-OD */
        default               : return 0x0;              /* entradas não aqui */
    }
}

/* -------------------------------------------------------------------------- */
/* API bruta                                                                  */
/* -------------------------------------------------------------------------- */

void io_enable_port_clock(GPIO_TypeDef *port)
{
    uint32_t bit = io__rcc_gpio_en_bit(port);
    if (bit) {
        RCC->APB2ENR |= bit;
        (void)RCC->APB2ENR; /* dummy read para evitar reordenação */
    }
}

void io_config_raw(GPIO_TypeDef *port, uint8_t pin, uint8_t cfg4bits)
{
    if (!port || !IO_IS_VALID_PIN(pin)) return;

    uint8_t shift;
    volatile uint32_t *cr = io__cr_reg(port, pin, &shift);

    uint32_t tmp = *cr;
    tmp &= ~(0xFu << shift);
    tmp |=  ((uint32_t)(cfg4bits & 0xFu) << shift);
    *cr = tmp;
}

void io_config_ex_raw(GPIO_TypeDef *port, uint8_t pin, uint8_t cfg4bits, uint8_t extra)
{
    if (!port || !IO_IS_VALID_PIN(pin)) return;

    /* Programa CRL/CRH */
    io_config_raw(port, pin, cfg4bits);

    /* Se entrada com PUPD: aplica UP/DOWN via ODR */
    if ((cfg4bits & 0xC) == IO_IN_PUPD) {
        if (extra == IO_PULL_UP) {
            port->ODR |= IO_BIT(pin);
        } else if (extra == IO_PULL_DOWN) {
            port->ODR &= ~IO_BIT(pin);
        }
        return;
    }

    /* Para saída/AF: define nível inicial se solicitado */
    if (extra == IO_LEVEL_HIGH) {
        port->BSRR = IO_BIT(pin);                 /* set */
    } else if (extra == IO_LEVEL_LOW) {
        port->BSRR = (uint32_t)IO_BIT(pin) << 16; /* reset */
    }
}

/* -------------------------------------------------------------------------- */
/* API ergonômica                                                              */
/* -------------------------------------------------------------------------- */

void io_init_simple(GPIO_TypeDef *port, uint8_t pin,
                    io_func_t func, io_speed_t speed,
                    io_pull_t pull, uint8_t init_level)
{
    if (!port || !IO_IS_VALID_PIN(pin)) return;

    io_enable_port_clock(port);

    switch (func) {
    case IO_FUNC_INPUT_ANALOG:
        io_config_ex_raw(port, pin, IO_IN_ANALOG, IO_EXTRA_NONE);
        break;

    case IO_FUNC_INPUT_FLOATING:
        io_config_ex_raw(port, pin, IO_IN_FLOATING, IO_EXTRA_NONE);
        break;

    case IO_FUNC_INPUT_PUPD:
        io_config_ex_raw(port, pin, IO_IN_PUPD,
                         (pull == IO_PULLUP) ? IO_PULL_UP :
                         (pull == IO_PULLDOWN ? IO_PULL_DOWN : IO_PULL_DOWN));
        break;

    case IO_FUNC_OUTPUT_PP:
    case IO_FUNC_OUTPUT_OD:
    case IO_FUNC_AF_PP:
    case IO_FUNC_AF_OD: {
        uint8_t cfg = io_build_out_cfg(func, speed);
        io_config_ex_raw(port, pin, cfg,
                         (init_level ? IO_LEVEL_HIGH : IO_LEVEL_LOW));
        break;
    }

    default:
        /* inválido: não faz nada */
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* Operações de runtime                                                        */
/* -------------------------------------------------------------------------- */

void io_write(GPIO_TypeDef *port, uint8_t pin, uint8_t level)
{
    if (!port || !IO_IS_VALID_PIN(pin)) return;

    if (level) {
        port->BSRR = IO_BIT(pin);                 /* set */
    } else {
        port->BSRR = (uint32_t)IO_BIT(pin) << 16; /* reset */
    }
}

uint8_t io_read(GPIO_TypeDef *port, uint8_t pin)
{
    if (!port || !IO_IS_VALID_PIN(pin)) return 0u;
    return (uint8_t)((port->IDR & IO_BIT(pin)) ? 1u : 0u);
}

void io_toggle(GPIO_TypeDef *port, uint8_t pin)
{
    if (!port || !IO_IS_VALID_PIN(pin)) return;

    /* Método simples: XOR em ODR (RMW) */
    port->ODR ^= IO_BIT(pin);

    /* Alternativa (quase atômica):
       if (port->ODR & IO_BIT(pin)) port->BSRR = IO_BIT(pin) << 16;
       else                         port->BSRR = IO_BIT(pin);
    */
}
