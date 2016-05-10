/* gpio.c
 * Tom Trebisky  5-1-2016
 *
 * This is a generic GPIO driver for the ESP8266
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

/* This is an array of pin control register addresses.
 */
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

/* These are the mux values that put a pin into GPIO mode
 */
static const int func[] = { 0, 3, 0, 3,   0, 0, Z, Z,   Z, 3, 3, Z,   3, 3, 3, 3 };

/* Some notes on basic GPIO control registers.
 * Most people fiddle with these using the gpio_output_set() routine.
 *
 * gpio_output_set ( set_mask, clear_mask, out_mask, in_mask )
 *
 * You could expose some basic registers by adding these lines to the eagle.app.v6.ld file.
PROVIDE(PIN_OUT = 0x60000300);
PROVIDE(PIN_OUT_SET = 0x60000304);
PROVIDE(PIN_OUT_CLEAR = 0x60000308);
PROVIDE(PIN_DIR = 0x6000030C);
PROVIDE(PIN_DIR_OUTPUT = 0x60000310);
PROVIDE(PIN_DIR_INPUT = 0x60000314);
PROVIDE(PIN_IN = 0x60000318);
PROVIDE(PIN_0 = 0x60000328);
PROVIDE(PIN_2 = 0x60000330);
 *
 * Then add lines to C code like so:
extern volatile uint32_t PIN_OUT;
 *
 * The routine gpio_output_set writes to four of these
 *  ( 304, 308, 310, 314 )
 * So, lets look at these.
 *  304 is a write only register that takes a mask of bits to be forced to one.
 *  308 is a write only register that takes a mask of bits to be forced to zero.
 *    The nice part about these registers is that any bits not included in the
 *    mask are unchanged.
 *  310 is a write only register that takes a mask of bits to be made outputs.
 *  314 is a write only register that takes a mask of bits to be made inputs..
 *    These are an exact analogy to the set/clear registers,
 *     any bits not in the mask are unchanged.
 *
 * There are registers that change the state of all pins.
 *  300 sets the state of all 16 gpio all at once, everything gets changed.
 *   You can read this register, which makes things sane.
 *  30C is the same thing for input/output direction.
 *   writing a one makes a bit an output.  It can also be read.
 *
 * The PIN_0 and PIN_2 registers are still mysteries.
 *
 * All this makes clear why gpio_output_set() is the way it is.  It accepts a list
 *  of masks indicating changes to be made.  
 */

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

/* i2c wants input mode with pullup enabled */
void
pin_iic ( int gpio )
{
	PIN_PULLUP_EN ( mux[gpio] );

	// GPIO_DIS_OUTPUT (gpio);
	gpio_output_set(0,0,0, 1<<gpio);
}

/* gpio_output_set() is a routine in the bootrom.
 * it has four arguments.
 * gpio_output_set ( set_mask, clear_mask, out_mask, in_mask )
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

/* Set multiple pins (used by i2c) */
void
pin_mask ( int hmask, int lmask )
{
	int all_mask = hmask | lmask;

	gpio_output_set( hmask, lmask, all_mask, 0);
}

/* THE END */
