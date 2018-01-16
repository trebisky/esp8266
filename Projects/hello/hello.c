/* ESP8266 sdk experiments
 *
 * This is a simple TCP server that handles requests to
 *  ring a bell via a GPIO bit that controls an SSR.
 *
 * This runs on a NodeMCU devel board.
 * It also controls the two on-board LEDs on the NodeMCU board.
 * One is on GPIO-2,
 * The other is on GPIO-16 (which actually isn't a GPIO).
 *
 * This is derived from my led_server project.
 *
 * The idea is to have the ESP8266 run a server that
 * listens for commands to ring a bell.
 * I plan to use this to announce events in my workshop,
 * such as visitors pushing the doorbell in the main house,
 * arrival of USPS mail, and other such things.
 *
 * The IP address gets set in set_ip_static ();
 *  This is esp_bell - 192.168.0.41
 *
 * Tom Trebisky  8-7-2017
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"
// #include "espconn.h"
// #include "mem.h"

void
user_init ( void )
{
    // This is used to setup the serial communication
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    os_printf ("\n");
    os_printf ( "Hello world\n" );

}

/* THE END */
