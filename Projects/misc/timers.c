/* Try out timers on the ESP8266
 * get a pair of OS timers running
 * Tom Trebisky  12-27-2015
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"

static os_timer_t timer1;
static os_timer_t timer2;

void
timer_func1 ( void *arg )
{
    os_printf("dog\n");
}

void
timer_func2 ( void *arg )
{
    os_printf("cat\n");
}
 
void user_init()
{
    os_timer_disarm ( &timer1 );
    os_timer_setfn ( &timer1, timer_func1, NULL );
    os_timer_arm ( &timer1, 1000, 1 );

    os_timer_disarm ( &timer2 );
    os_timer_setfn ( &timer2, timer_func2, NULL );
    os_timer_arm ( &timer2, 5000, 1 );

    // This is used to setup the serial communication
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());
    // system_print_meminfo();
}

/* THE END */
