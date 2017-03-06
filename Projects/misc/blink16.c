/* Try out things on the ESP8266
 * get an LED blinking on GPIO 16
 * Tom Trebisky  12-27-2015
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"

static int use_16 = 1;

static os_timer_t timer;

/* From SDK examples/driver_lib/driver/gpio16.c
 */
void
gpio16_setup ( void )
{
   // mux configuration for XPD_DCDC to output rtc_gpio0
    WRITE_PERI_REG ( PAD_XPD_DCDC_CONF,
	(READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1);

    //mux configuration for out enable
    WRITE_PERI_REG ( RTC_GPIO_CONF,
	(READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);

    //out enable
    WRITE_PERI_REG ( RTC_GPIO_ENABLE,
	(READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe) | (uint32)0x1);

}

void
gpio16_write ( int val )
{
    WRITE_PERI_REG ( RTC_GPIO_OUT,
	(READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | (uint32)(val & 1));
}

int
gpio16_read ( void )
{
	return READ_PERI_REG(RTC_GPIO_OUT) & 1;
}

/* BIT2 blinks the little LED on my NodeMCU board
 * (which is labelled "D4" on the silkscreen)
 */
#define LED_BIT		BIT2
#define FUNC_MUX	PERIPHS_IO_MUX_GPIO2_U
#define FUNC_F		FUNC_GPIO2

void
gpio_setup ( void )
{
    if ( use_16 )
	gpio16_setup ();
    else
	PIN_FUNC_SELECT ( FUNC_MUX, FUNC_F );
}

void
gpio_write ( int val )
{
    if ( use_16 ) {
	gpio16_write ( val );
	return;
    }

    if ( val )
	gpio_output_set(0, LED_BIT, LED_BIT, 0);
    else
	gpio_output_set(LED_BIT, 0, LED_BIT, 0);
}

int
gpio_read ( void )
{
    if ( use_16 )
	return gpio16_read ();
    else
	return ! (GPIO_REG_READ(GPIO_OUT_ADDRESS) & LED_BIT);
}

void
timer_func ( void *arg )
{
    if ( gpio_read () ) {
	os_printf("found high\n");
	gpio_write ( 0 );
    } else {
	os_printf("found low\n");
	gpio_write ( 1 );
    }
}

void user_init()
{
    os_timer_disarm ( &timer );
    os_timer_setfn ( &timer, timer_func, NULL );
    os_timer_arm ( &timer, 1000, 1 );

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());

    gpio_setup ();
    gpio_write ( 0 );
}

/* THE END */
