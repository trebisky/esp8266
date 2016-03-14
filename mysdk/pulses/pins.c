/* pins.c
 * Tom Trebisky  1-7-2016
 *
 * Simple pin IO "library"
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"
#include "c_types.h"

/* --------------------------------------------- */
/* Pin routines */
/* --------------------------------------------- */

/* place holder for unused channels */
#define Z       0

static const int mux[] = {
    PERIPHS_IO_MUX_GPIO0_U,     /* 0 - D3 */
    PERIPHS_IO_MUX_U0TXD_U,     /* 1 - uart */
    PERIPHS_IO_MUX_GPIO2_U,     /* 2 - D4 */
    PERIPHS_IO_MUX_U0RXD_U,     /* 3 - uart */
    PERIPHS_IO_MUX_GPIO4_U,     /* 4 - D2 */
    PERIPHS_IO_MUX_GPIO5_U,     /* 5 - D1 */
    Z,  /* 6 */
    Z,  /* 7 */
    Z,  /* 8 */
    PERIPHS_IO_MUX_SD_DATA2_U,  /* 9   - D11 (SD2) */
    PERIPHS_IO_MUX_SD_DATA3_U,  /* 10  - D12 (SD3) */
    Z,  /* 11 */
    PERIPHS_IO_MUX_MTDI_U,      /* 12 - D6 */
    PERIPHS_IO_MUX_MTCK_U,      /* 13 - D7 */
    PERIPHS_IO_MUX_MTMS_U,      /* 14 - D5 */
    PERIPHS_IO_MUX_MTDO_U       /* 15 - D8 */
};

static const int func[] = { 0, 3, 0, 3,   0, 0, Z, Z,   Z, 3, 3, Z,   3, 3, 3, 3 };

/* Put gpio into input mode */
void
pin_input ( int gpio )
{
#ifdef notdef
	PIN_PULLUP_DIS ( mux[gpio] );

	gpio_register_set( GPIO_PIN_ADDR(gpio),
                       GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)  |
                       GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE) |
                       GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
#endif
	// GPIO_DIS_OUTPUT (gpio);
	gpio_output_set(0,0,0, 1<<gpio);
}

/* gpio_output_set() is a routine in the bootrom.
 * it has four arguments.
 * gpio_output_set ( set_mask, clear_mask, enable_mask, disable_mask )
 */

/* Read input value */
int
pin_read ( int gpio )
{
	// return (gpio_input_get()>>gpio) & 1;
	// return gpio_input_get() & (1<<gpio);
	return GPIO_INPUT_GET ( gpio );
}

/* Put gpio into output mode */
void
pin_output ( int gpio )
{
	PIN_FUNC_SELECT ( mux[gpio], func[gpio] );
	PIN_PULLUP_EN ( mux[gpio] );
}

/* Set output state high */
void
pin_high ( int gpio )
{
	int mask = 1 << gpio;

	// GPIO_OUTPUT_SET(gpio, 1);
	gpio_output_set ( mask, 0, mask, 0);
}

/* Set output state low */
void
pin_low ( int gpio )
{
	int mask = 1 << gpio;

	// GPIO_OUTPUT_SET(gpio, 0);
	gpio_output_set(0, mask, mask, 0);
}

/* THE END */
