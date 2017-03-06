/* Try out timers on the ESP8266
 * crank out a waveform to try my new dso Nano
 * Tom Trebisky  1-14-2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"

static os_timer_t timer1;

// #define PIN	2	/* D4 */	
// #define PIN	14	/* D5 */	
#define PIN	5	/* D1 */	
// #define PIN	4	/* D2 */	
// #define PIN	12	/* D6 */	

static int state;
static int count;

#define LOW_COUNT	9
#define HIGH_COUNT	1

void
timer_func1 ( void *arg )
{
    if ( --count > 0 )
	return;

    if ( state ) {
	pin_low ( PIN );
	state = 0;
	count = LOW_COUNT;
    } else {
	pin_high ( PIN );
	state = 1;
	count = HIGH_COUNT;
    }
}

#define DELAY	1

void user_init()
{
    os_timer_disarm ( &timer1 );
    os_timer_setfn ( &timer1, timer_func1, NULL );
    os_timer_arm ( &timer1, DELAY, 1 );

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    // os_printf("SDK version:%s\n", system_get_sdk_version());

    pin_output ( PIN );
    pin_low ( PIN );
    state = 0;
    count = LOW_COUNT;

    os_printf("running !!\n" );
}

/* THE END */
