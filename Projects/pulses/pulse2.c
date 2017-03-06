/* Try out timers on the ESP8266
 * crank out a waveform to try my new dso Nano
 *
 * The idea here is to crank out a burst of pulses that
 * we need to set up a trigger to capture.
 *
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
static int mode;
static int pulse_count;

#define M_PULSE		1
#define M_IDLE		0

#define LOW_COUNT	2
#define HIGH_COUNT	1
#define IDLE_COUNT	20

#define NUM_PULSES	2

void
pulse ( void )
{
    if ( --count > 0 )
	return;

    if ( state ) {
	pin_low ( PIN );
	state = 0;
	count = LOW_COUNT;
	if ( --pulse_count > 0 )
	    return;
	mode = M_IDLE;
	count = IDLE_COUNT;
    } else {
	pin_high ( PIN );
	state = 1;
	count = HIGH_COUNT;
    }
}

void
timer_func1 ( void *arg )
{
    if ( mode == M_PULSE ) {
	pulse ();
	return;
    }

    if ( -- count > 0 )
	return;
    mode = M_PULSE;
    pulse_count = NUM_PULSES;
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

    mode = M_PULSE;
    pulse_count = NUM_PULSES;

    os_printf("running !!\n" );
}

/* THE END */
