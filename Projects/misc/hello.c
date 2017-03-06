/* Print hello world once a second .
 * Tom Trebisky 2-3-2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"

void
my_puts ( char *s )
{
    while ( *s ) {
	ets_write_char ( *s++ );
    }
}

/* Apparently ets_putc does not map \n to \n\r */
void
my_puts2 ( char *s )
{
    while ( *s ) {
	ets_putc ( *s++ );
    }
}

void show ( unsigned int ad )
{
    unsigned int *adp = (unsigned int *) ad;

    os_printf_plus ( "%08x: %08x\n", ad, *adp );
}

void user_init ( void )
{
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());
    os_printf("Hello, world!\n");

    show ( 0x3fffdd48 );
    show ( 0x3fffdd4c );

    /* It does not like this.
     * with this, it goes into a boot loop on about
     * a 2 second cycle with the message:
     *  rst cause:2, boot mode:(3,6)
     * (actually some sources say you get 3.2 seconds)
     * 
     * The idea is that you should just be setting up event callbacks
     *  and not tying things up in user_init.
     */
#ifdef notdef
    for ( ;; ) {
	os_delay_us ( 1000 );
    }
#endif

    ets_printf ( "\nETS\n" );
    my_puts ( "0000\n" );
    my_puts ( "0000\n" );
    my_puts2 ( "XXXX\n" );
    my_puts2 ( "XXXX\n" );
}

/* THE END */
