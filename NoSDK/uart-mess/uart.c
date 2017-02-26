/* ESP8266 bare metal experiments
 * Tom Trebisky  12-28-2015
 */

unsigned int strlen ( const char * );

#define FULL	1000 * 1000
#define HALF	1000 * 500
#define TENTH	1000 * 100

char stuff[] = "0123456789abcdABCD ";

void uart_putc ( unsigned char );
void tiny_start ( void );

/* The "normal" sdk link file specifies
 * call_user_start() as the first bit of code
 * executed.  This eventually calls user_init()
 */
void user_init ( void )
{
    tiny_start ();
}

// void user_init ( void ) {} /* stub */

// void user_init()
// void call_user_start ( void )
void
tiny_start ( void )
{
    char *p;
    int n;
    int i;

    n = strlen ( stuff );

    uart_div_modify ( 0, (80*1000*1000) / 115200 );
    os_printf_plus ( "\n\n" );
    os_printf_plus ( "Hello 1\n" );

    ets_delay_us ( 10 * 1000 );

    uart_init ();
    uart_baud ( 115200 );

    os_printf_plus ( "Hello 2\n" );

    /* The real SDK runs a watchdog and
     * if you don't return from user_init in about
     * 3.2 seconds, you get a reset.
     */
    /*
    i = 0;
    for ( ;; ) {
	//uart_putc ( stuff[i] );
	//if ( ++i >= n ) i = 0;

	// uart_putc ( i );
	// if ( ++i >= 256 ) i = 0;

	uart_putc ( '0' );
	

	// ets_delay_us ( HALF );
	// ets_delay_us ( TENTH );
	ets_delay_us ( FULL );
    }
    */
}

/* THE END */
