/* ESP8266 sdk experiments
 * Do simple hello world for disassembly
 * Tom Trebisky  1-31-2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

void show_addr ( unsigned int addr )
{
    unsigned int *p;

    p = (unsigned int *) addr;
    os_printf ( "%08x: %08x\n", addr, *p );
}

void user_init()
{
    // wifi_set_opmode(NULL_MODE);

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    os_printf("\n");
    os_printf("Hello\n");

    show_addr ( 0x3fffde50 );
}

/* THE END */
