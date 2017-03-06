/* Try out timers on the ESP8266
 * get an LED blinking
 * Tom Trebisky  12-27-2015
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"

static os_timer_t timer1;

/* BIT2 blinks the little LED on my NodeMCU board
 * (which is labelled "D4" on the silkscreen)
 * This is the LED on the ESP-12E board.
 * This code also works with the plain ESP-12 board I have.
 */
#define LED_BIT		BIT2
#define FUNC_MUX	PERIPHS_IO_MUX_GPIO2_U
#define FUNC_F		FUNC_GPIO2

void
timer_func1 ( void *arg )
{
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & LED_BIT) {
	gpio_output_set(0, LED_BIT, LED_BIT, 0);
	os_printf("set high\n");
    } else {
	gpio_output_set(LED_BIT, 0, LED_BIT, 0);
	os_printf("set low\n");
    }
}

void user_init()
{
    os_timer_disarm ( &timer1 );
    os_timer_setfn ( &timer1, timer_func1, NULL );
    os_timer_arm ( &timer1, 1000, 1 );

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());

    /*
    gpio_init ();
    */

    PIN_FUNC_SELECT ( FUNC_MUX, FUNC_F );

    /* low */
    gpio_output_set(0, LED_BIT, LED_BIT, 0);

    /* high */
    // gpio_output_set(LED_BIT, 0, LED_BIT, 0);
}

/* THE END */
