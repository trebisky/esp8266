/* sleep.c
 * Tom Trebisky  4-12-2016
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"
#include "c_types.h"

void
user_init( void )
{
    unsigned short val;

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf( "\n");
    os_printf( "running !!\n" );

    val = readvdd33 ();
    os_printf ( "Vdd33 = %d\n", val );

    val = system_adc_read ();
    os_printf ( "ADC = %d\n", val );

    os_printf( "sleeping\n" );

    system_deep_sleep ( 10 * 1000 * 1000 );
}

/* THE END */
