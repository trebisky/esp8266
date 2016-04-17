/* Print hello world once a second .
 * Tom Trebisky 2-3-2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"

void user_init ( void )
{
    unsigned short xx[8];
    unsigned short yy;

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());

    read_sar_dout ( xx );
    yy = system_adc_read ();
    yy = readvdd33 ();
}

/* THE END */
