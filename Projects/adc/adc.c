/* 
 * This was derived from my old blink3 demo.
 *
 * We trigger a number of activities using the ESP8266
 *  hardware timer.
 *
 *  - read and print the ADC reading from the tout pin
 *  - we blink the onboard LED on GPIO2 at 1 Hz
 *  - we generate a 100 Khz waveform on GPIO5
 * 
 * The idea here is to learn how to use
 *  a hardware timer on the ESP8266
 *
 * Here we do 2 things with it.
 *
 * Works great, os timer has 1 ms resolution.
 *
 * Tom Trebisky  2-18-2017  5-13-2021
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"
#include "user_interface.h"

/* BIT2 blinks the little LED on my NodeMCU board
 * (which is labelled "D4" on the silkscreen)
 * This is the LED on the ESP-12E board.
 * This code also works with the plain ESP-12 board I have.
 */
#define LED_BIT		BIT2
#define PULSE_BIT	BIT5

/* Timer clock */
#define DIV_BY_1     0
#define DIV_BY_16    4
#define DIV_BY_256   8

#define TM_LEVEL_INT  1   // level interrupt
#define TM_EDGE_INT   0   // edge interrupt

static void
flip_led ( void )
{
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & LED_BIT) {
	gpio_output_set(0, LED_BIT, LED_BIT, 0);
	// os_printf("set high\n");
    } else {
	gpio_output_set(LED_BIT, 0, LED_BIT, 0);
	// os_printf("set low\n");
    }
}

static void
pulse ( void )
{
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & PULSE_BIT) {
	gpio_output_set(0, PULSE_BIT, PULSE_BIT, 0);
    } else {
	gpio_output_set(PULSE_BIT, 0, PULSE_BIT, 0);
    }
}

static void
adc_handler ( void )
{
    int val;

    val = system_adc_read ();
    os_printf ( "ADC = %04x (%d)\n", val, val );
    val = system_get_vdd33 ();
    os_printf ( "V33 = %04x (%d)\n", val, val );
}

#define LED_RATE	100*1000

static long count = 0;

void
hw_timer_isr ( void )
{
    if ( ++count >= LED_RATE ) {
	flip_led ();
	count = 0;
	adc_handler ();
    }

    pulse ();
}

/* After reading in the API guide for the SDK,
 * one would think that the 3 calls would be
 * provided in some library, but they are not.
 * A look at the recommended example shows
 * that these routines are provided as source in
 * the example itself:
 *  sdk/examples/driver_lib/driver/hw_timer.c
 * I began with that code and worked up the code
 *  below.
 */

#ifdef notdef
static void
hw_timer_setup_sdk ( void )
{
    hw_timer_init ( FRC1_SOURCE, 1 );
    hw_timer_set_func ( timer_func1 );
    hw_timer_arm ( 500 * 1000 );
}
#endif

#define US_TO_RTC_TIMER_TICKS(t)          \
    ((t) ?                                   \
     (((t) > 0x35A) ?                   \
      (((t)>>2) * ((APB_CLK_FREQ>>4)/250000) + ((t)&0x3) * ((APB_CLK_FREQ>>4)/1000000))  :    \
      (((t) *(APB_CLK_FREQ>>4)) / 1000000)) :    \
     0)

#define FRC1_ENABLE_TIMER  BIT7
#define FRC1_AUTO_LOAD  BIT6

/* Argument is interval in microseconds !! */
static void
hw_timer_setup ( unsigned int val )
{
    /* hw_timer_init */
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS, FRC1_AUTO_LOAD | DIV_BY_16 | FRC1_ENABLE_TIMER | TM_EDGE_INT);
    // RTC_REG_WRITE(FRC1_CTRL_ADDRESS, DIV_BY_16 | FRC1_ENABLE_TIMER | TM_EDGE_INT);

    // ETS_FRC_TIMER1_NMI_INTR_ATTACH(hw_timer_isr);
    ETS_FRC_TIMER1_INTR_ATTACH(hw_timer_isr, NULL);

    TM1_EDGE_INT_ENABLE();
    ETS_FRC1_INTR_ENABLE();

    /* hw_timer_arm */
    RTC_REG_WRITE(FRC1_LOAD_ADDRESS, US_TO_RTC_TIMER_TICKS(val));
}

void user_init()
{

    // hw_timer_setup ( 500 * 1000 );
    hw_timer_setup ( 5 );
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());

    /* remember GPIO 1 and 3 are uart */

/* Setup "bit 2" for output (this is the LED) */
#define FUNC_MUX	PERIPHS_IO_MUX_GPIO2_U
#define FUNC_F		FUNC_GPIO2
    // PIN_FUNC_SELECT ( FUNC_MUX, FUNC_F );
    PIN_FUNC_SELECT ( PERIPHS_IO_MUX_GPIO2_U, 0 );

/* Setup GPIO-5 for output (for fast pulses) */
    PIN_FUNC_SELECT ( PERIPHS_IO_MUX_GPIO5_U, 0 );

    gpio_output_set(0, LED_BIT, LED_BIT, 0);
    gpio_output_set(0, PULSE_BIT, PULSE_BIT, 0);
}

/* THE END */
