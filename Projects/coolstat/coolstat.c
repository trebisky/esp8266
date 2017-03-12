/* 
 * This is the code for my coolstat mod.
 *
 * It runs on an ESP8266 on an ESP-12 card.
 *
 * We listen on GPIO4 for commands.
 * We relay these (with possible changes) to GPIO5.
 *
 * We run the hardware timer at 10,000 Hz so we
 *  can sample each bit 10 times.
 *  (the actual serial data rate is 1000 Hz.)
 *
 * Tom Trebisky  2-20-2017
 *     finished  3-11-2017
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"
#include "user_interface.h"

/* Scope shows that these are right */
#define bit_low(x)	gpio_output_set(0, x, x, 0)
#define bit_high(x)	gpio_output_set(x, 0, x, 0)

#define ACTIVE		1
#define INACTIVE	0

/* We need to flip both input and output for the real thing.
 * We did initial development and test using the signals available outside
 * the coolstat (which has an inverting line driver transistor), but when
 * we patch the board into the system, we will do that ahead of this inverting
 * transistor and be dealing with "active low" signals
 *   (well inverted signals anyhow).
 */
#define FLIP

#ifdef FLIP
/* for the real thing */
#define bit_inactive(x)	bit_high(x)
#define bit_active(x)	bit_low(x)
#else
/* for test */
#define bit_inactive(x)	bit_low(x)
#define bit_active(x)	bit_high(x)
#endif

/* BIT2 controls the LED on the ESP-12 board
 */
#define LED_BIT		BIT2

#define IN_BIT		BIT4
#define OUT_BIT		BIT5

/* Timer clock */
#define DIV_BY_1     0
#define DIV_BY_16    4
#define DIV_BY_256   8

#define TM_LEVEL_INT  1   // level interrupt
#define TM_EDGE_INT   0   // edge interrupt

/*
 * Certain eagle_soc.h files have this
#define PERIPHS_IO_MUX_PULLUP           BIT7
#define PIN_PULLUP_DIS(PIN_NAME)                 CLEAR_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLUP)
#define PIN_PULLUP_EN(PIN_NAME)                  SET_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLUP)
*/

#define PERIPHS_IO_MUX_PULLDWN          BIT6
#define PIN_PULLDWN_DIS(PIN_NAME)             CLEAR_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLDWN)
#define PIN_PULLDWN_EN(PIN_NAME)              SET_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLDWN)

#define bit_input(x)	gpio_output_set(0, 0, 0, x)

#define bit_test(x)	(gpio_input_get() & x)

#ifdef BLINK_LED
static void
flip_led ( void )
{
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & LED_BIT) {
	bit_low ( LED_BIT );
    } else {
	bit_high ( LED_BIT );
    }
}

#define LED_RATE	5*1000
static int led_clock = 0;
#endif

#define IDLE	0
#define LEAD	1
#define LCOUNT	2
#define HCOUNT	3

#define START	4	/* for output */

#define MAX_CLOCK	110
#define MAX_BITS	8

static int state = IDLE;
static int clock_count;
static int bit_count;
static int hcount;
static int lcount;

void new_data ( void );

#ifdef FLIP
int
read_input ( void )
{
    if ( gpio_input_get() & IN_BIT )
	return INACTIVE;
    else
	return ACTIVE;
}
#else
int
read_input ( void )
{
    if ( gpio_input_get() & IN_BIT )
	return ACTIVE;
    else
	return INACTIVE;
}
#endif

static int inbits;
static int outbits;

/* Input state machine */
int
process_input ( void )
{
    int pin;

    pin = read_input ();

    /* Common case */
    if ( state == IDLE ) {
	if ( pin == ACTIVE ) {
	    state = LEAD;
	    clock_count = 1;
	    bit_count = 0;
	    inbits = 0;
	}
	return;
    }

    if ( ++clock_count > MAX_CLOCK ) {
	state = IDLE;
	os_printf ( "Oops\n" );
	return;
    }

    if ( state == LEAD ) {
	if ( pin == INACTIVE ) {
	    state = LCOUNT;
	    hcount = 0;
	    lcount = 0;
	}
	return;
    }

    if ( state == LCOUNT ) {
	if ( pin == ACTIVE )
	    state = HCOUNT;
	else
	    ++lcount;
	return;
    }

    /* HCOUNT */
    if ( pin == ACTIVE ) {
	++hcount;
	return;
    }

    /* SHORT */
    if ( lcount > hcount )
	inbits |= 0x80 >> bit_count;

    if ( ++bit_count >= MAX_BITS ) {
	state = IDLE;
	/* Finish -- full byte received */
	new_data ();
    } else {
	state = LCOUNT;
	hcount = 0;
	lcount = 0;
    }
}

#define LEAD_TIME	20
#define LONG_TIME	7
#define SHORT_TIME	3

static int out_state = IDLE;
static int outbit_count;
static int out_count;

/* Output state machine */
void
process_output ( void )
{
    if ( out_state == IDLE )
	return;

    if ( out_state == START ) {
	out_count = LEAD_TIME;
	outbit_count = 0;
	out_state = HCOUNT;
	bit_active ( OUT_BIT );
	return;
    }

    if ( out_state == LCOUNT ) {
	if ( --out_count > 0 )
	    return;

	if ( outbits & (0x80 >> outbit_count) )
	    out_count = SHORT_TIME;
	else
	    out_count = LONG_TIME;
	/*
	if ( outbits[outbit_count] == BIT_LONG )
	    out_count = LONG_TIME;
	else
	    out_count = SHORT_TIME;
	    */
	outbit_count++;
	bit_active ( OUT_BIT );
	out_state = HCOUNT;
	return;
    }

    /* HCOUNT */
    if ( --out_count > 0 )
	return;

    if ( outbit_count >= MAX_BITS ) {
	bit_inactive ( OUT_BIT );
	out_state = IDLE;
	return;
    }

    if ( outbits & (0x80 >> outbit_count) )
	out_count = LONG_TIME;
    else
	out_count = SHORT_TIME;
    /*
    if ( outbits[outbit_count] == BIT_LONG )
	out_count = SHORT_TIME;
    else
	out_count = LONG_TIME;
    */
    bit_inactive ( OUT_BIT );
    out_state = LCOUNT;
}

#define KEEP_MASK	0xf1
#define FAN_MASK	0x0c
#define PUMP_MASK	0x02
#define PURGE_MASK	0x01

#define FAN_BOTH	0x0c
#define FAN_HIGH	0x08
#define FAN_LOW		0x04

void
show_bits ( char *msg, int bits )
{
	char ss[5];
	char *p;

	// os_printf ( "Got -- %d %d %d %d - %d %d %d %d\n",
	//     bits[0], bits[1], bits[2], bits[3],
	//     bits[4], bits[5], bits[6], bits[7] );

	p = ss;
	*p++ = bits & FAN_HIGH ? 'H' : ' ';
	*p++ = bits & FAN_LOW ? 'L' : ' ';
	*p++ = bits & PUMP_MASK ? 'P' : ' ';
	*p++ = bits & PURGE_MASK ? 'X' : ' ';
	*p = '\0';

	os_printf ( "%s -- %02x %s\n", msg, bits, ss );
}

#define HOLD_TIME	80	/* 20 seconds */

/* In truth, these could all be char */
static int first = 1;
static int pump_last;
static int fan_last;
static int pump_hold;
static int fan_hold;

/* Called whenever new data is received.
 * This is where the dead band logic is applied.
 */
void
new_data ( void )
{
    int pump, fan;
    char fs, ps;

    // show_bits ( "in ", inbits );

    pump = inbits & PUMP_MASK;
    fan = inbits & FAN_MASK;

    /* Never allow this */
    if ( fan == FAN_BOTH )
	fan = FAN_HIGH;

    if ( first ) {
	// os_printf ( "Setting first values\n" );
	first = 0;
	pump_hold = 0;
	fan_hold = 0;
	pump_last = pump;
	fan_last = fan;
    }

    // fs = fan_hold ? 'F' : 'f';
    // ps = pump_hold ? 'P' : 'p';
    // os_printf ( " %c %x %x -- %c %x %x\n", fs, fan, fan_last, ps, pump, pump_last );

    // os_printf ( " %2d %x %x -- %2d %x %x\n", fan_hold, fan, fan_last, pump_hold, pump, pump_last );

    if ( pump_hold ) {
	pump = pump_last;
	pump_hold--;
	// if ( pump_hold == 0 ) os_printf ( "Stop pump hold\n" );
    }

    if ( fan_hold ) {
	fan = fan_last;
	fan_hold--;
	// if ( fan_hold == 0 ) os_printf ( "Stop fan hold\n" );
    }

    if ( pump != pump_last ) {
	// os_printf ( "Start pump hold\n" );
	pump_last = pump;
	pump_hold = HOLD_TIME;
    }

    if ( fan != fan_last ) {
	// os_printf ( "Start fan hold\n" );
	fan_last = fan;
	fan_hold = HOLD_TIME;
    }

    // outbits = inbits;
    outbits = (inbits & KEEP_MASK) | pump | fan;
    // show_bits ( "out", outbits );

    out_state = START;
}

/* Everything is driven by this interrupt ticking */
void
hw_timer_isr ( void )
{
#ifdef BLINK_LED
    ++led_clock;
    if ( led_clock >= LED_RATE ) {
	flip_led ();
	led_clock = 0;
    }
#endif

    process_input ();
    process_output ();
}

#define US_TO_RTC_TIMER_TICKS(t)          \
    ((t) ?                                   \
     (((t) > 0x35A) ?                   \
      (((t)>>2) * ((APB_CLK_FREQ>>4)/250000) + ((t)&0x3) * ((APB_CLK_FREQ>>4)/1000000))  :    \
      (((t) *(APB_CLK_FREQ>>4)) / 1000000)) :    \
     0)

#define FRC1_ENABLE_TIMER	BIT7
#define FRC1_AUTO_LOAD		BIT6

/* Argument is interval in microseconds !! */
static void
hw_timer_setup ( unsigned int val )
{
    /* hw_timer_init */
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS, FRC1_AUTO_LOAD | DIV_BY_16 | FRC1_ENABLE_TIMER | TM_EDGE_INT);
    // RTC_REG_WRITE(FRC1_CTRL_ADDRESS, DIV_BY_16 | FRC1_ENABLE_TIMER | TM_EDGE_INT);

    // ETS_FRC_TIMER1_NMI_INTR_ATTACH(hw_timer_isr);
    ETS_FRC_TIMER1_INTR_ATTACH(hw_timer_isr, NULL);

    TM1_EDGE_INT_ENABLE();
    ETS_FRC1_INTR_ENABLE();

    /* hw_timer_arm */
    RTC_REG_WRITE(FRC1_LOAD_ADDRESS, US_TO_RTC_TIMER_TICKS(val));
}

void user_init ( void )
{
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    /* Don't need no stinkin' Wifi */
    wifi_set_opmode(NULL_MODE);

    // We want to sample 10 times per millisecond
    hw_timer_setup ( 100 );

    os_printf("\n");
    os_printf("Coolstat filter !!\n");

    // os_printf("SDK version:%s\n", system_get_sdk_version());

    /* remember GPIO 1 and 3 are uart */
    /* GPIO 2 is the boot control pin */

/* Setup "bit 2" for output (this is the LED)
 * and ensure the LED is turned off.
 * Note that pulling the bit low turns the LED on !!
 */
    PIN_FUNC_SELECT ( PERIPHS_IO_MUX_GPIO2_U, 0 );
    bit_high ( LED_BIT );

/* Set up GPIO-4 for input */
    PIN_FUNC_SELECT ( PERIPHS_IO_MUX_GPIO4_U, 0 );
    // PIN_PULLDWN_DIS ( PERIPHS_IO_MUX_GPIO4_U );
    // PIN_PULLDWN_EN ( PERIPHS_IO_MUX_GPIO4_U );
    // PIN_PULLUP_DIS ( PERIPHS_IO_MUX_GPIO4_U );
    PIN_PULLUP_EN ( PERIPHS_IO_MUX_GPIO4_U );
    bit_input ( IN_BIT );

/* Set up GPIO-5 for output  */
    PIN_FUNC_SELECT ( PERIPHS_IO_MUX_GPIO5_U, 0 );
    bit_inactive ( OUT_BIT );
}

/* THE END */
