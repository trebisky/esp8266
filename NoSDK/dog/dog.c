/* ESP8266
 *
 * This is almost the simplest possible
 * program we can run without the SDK.
 *
 * It compiles to 192 bytes, all of which goes
 * into the first flash segments.
 * About 18 assembly language statements.
 *
 * All 3 of the routines called are in the boot rom.
 *
 * Also since it exits, it drops back into the
 * boot rom which is then ready to run new code.
 *
 * Tom Trebisky  3-9-2016
 */

/* Look, no include files! */

/* The clock really is running at 52 Mhz when we
 * come out of the boot rom
 */
// #define UART_CLK_FREQ	(80*1000000)
#define UART_CLK_FREQ	(52*1000000)

void
call_user_start ( void )
{
    int n = 0;

    uart_div_modify(0, UART_CLK_FREQ / 115200);
    ets_delay_us ( 1000 * 500 );

    ets_printf("\n");
    ets_printf("Eat donuts!\n");

    /* Spin and see if watchdog gets cranky */
    for ( ;; ) {
	ets_delay_us ( 1000 * 1000 );
	ets_printf ( "%d\n", ++n );
    }
}

/* THE END */
