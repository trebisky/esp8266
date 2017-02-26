/* ESP8266 bare metal experiments
 * Tom Trebisky  12-28-2015
 */

unsigned int strlen ( const char * );

#define FULL	1000 * 1000
#define HALF	1000 * 500
#define TENTH	1000 * 100

char stuff[] = "0123456789abcdABCD ";

void uart_putc ( unsigned char );

// void user_init ( void ) {} /* stub */

// void user_init()
// void call_user_start ( void )
void
call_user_start ( void )
{
    char *p;
    int n;
    int i;

    uart_init ();
    uart_baud ( 115200 );
    uart_puts ( "Hello World\n" );

    for ( ;; )
	uart_putc ( '0' );

    /* The real SDK runs a watchdog and
     * if you don't return from user_init in about
     * 3.2 seconds, you get a reset.
     */

    /*
    n = strlen ( stuff );
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
