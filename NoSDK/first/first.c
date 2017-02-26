/* ESP8266
 * try running a program without the SDK
 * call only routines in the bootrom.
 * Tom Trebisky  2-5-2016
 */

/* Look, no include files.
 * The clock really is running at 52 Mhz when we
 * come out of the boot rom
 */
// #define UART_CLK_FREQ	(80*1000000)
#define UART_CLK_FREQ	(52*1000000)

unsigned long xthal_get_ccount ( void );

#define TENSEC (UART_CLK_FREQ * 10)

// void user_init()
void call_user_start()
{
    int i;
    int x;
    unsigned long cc1, cc2;
    unsigned long *cp;

    uart_div_modify(0, UART_CLK_FREQ / 115200);
    ets_delay_us ( 1000 * 500 );

    ets_printf("\n");
    ets_printf("Hello World\n");

    x = ets_get_cpu_frequency ();
    ets_printf ( "CPU %d\n", x );

    cp = (unsigned long *) 0x3ff00014;

    ets_printf ( "CPU control byte: %08x\n", *cp );
    ets_printf ( "test: %08x\n", 0x1234 );

    /* Setting this to zero changes nothing.
     * Setting it to one makes the delay loop run
     * in 5 seconds instead of 10, BUT the baud
     * rate stays OK without redoing the divisor.
     */
    *cp = 1;

    /* 10 second delay */
    cc1 = xthal_get_ccount ();
    for ( ;; ) {
	cc2 = xthal_get_ccount ();
	if ( cc2 - cc1 > TENSEC )
	    break;
    }

    ets_printf ( "Done!\n" );

    // ets_update_cpu_frequency ( 80 );

    for ( i=0; i< 50; i++ ) {
	// ets_write_char ( 'A' );
	ets_putc ( 'X' );
	ets_delay_us ( 1000 * 4  );
    }

    ets_printf("\n");
}

/* THE END */
