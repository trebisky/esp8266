/* Print hello world once a second .
 * Extra dead wood in here.
 * Tom Trebisky 12=26-2015
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"

static os_timer_t timer;

void
timer_func ( void *arg )
{
    os_printf("Hello, world!\n");
}
 
void user_init()
{
    struct station_config conf;

    os_timer_disarm ( &timer );
    os_timer_setfn ( &timer, timer_func, NULL );
    os_timer_arm ( &timer, 1000, 1 );

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());

    system_print_meminfo();
}

/* THE END */
