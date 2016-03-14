
/* --------------------------------------------- */
/* --------------------------------------------- */

#define DELAY	100
#define GPIO	14	/* D5 */	

static os_timer_t timer1;

void
timer_func1 ( void *arg )
{
    /*
    int t;
    int h;
    int s;

    s = dht_read ( GPIO );
    h = dht_getHumidity ();
    t = dht_getTemperature ();

    os_printf ( "Status: %d\n", s );
    os_printf ( " Temp:  %d\n", t );
    os_printf ( " Humid: %d\n", h );
    */
    int pin = GPIO;
    int val;

    // val = pin_input_read ( pin );
    val = gpio_input_get();
    os_printf ( "Val = %08x\n", val );

}

void
user_init( void )
{
    os_timer_disarm ( &timer1 );
    os_timer_setfn ( &timer1, timer_func1, NULL );
    os_timer_arm ( &timer1, DELAY, 1 );

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());

    pin_input ( GPIO );
    os_printf ( "Ready\n" );
}

/* THE END */
