/* dht_tt.c
 * Standalone version for the ESP8266
 * Tom Trebisky  1-7-2016
 *
 * This is a driver for the DHT-22, AD2302 (from Adsong),
 * or the RHT03 (from MaxDetect).
 * They all use their own unique one wire protocol
 *  (not to be confused with the Maxim one-wire protocol).
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"
#include "c_types.h"

unsigned long xthal_get_ccount ( void );

#define os_intr_lock	ets_intr_lock
#define os_intr_unlock	ets_intr_unlock

/* --------------------------------------------- */
/* Pin routines */
/* --------------------------------------------- */

/* place holder for unused channels */
#define Z       0

static const int mux[] = {
    PERIPHS_IO_MUX_GPIO0_U,     /* 0 - D3 */
    PERIPHS_IO_MUX_U0TXD_U,     /* 1 - uart */
    PERIPHS_IO_MUX_GPIO2_U,     /* 2 - D4 */
    PERIPHS_IO_MUX_U0RXD_U,     /* 3 - uart */
    PERIPHS_IO_MUX_GPIO4_U,     /* 4 - D2 */
    PERIPHS_IO_MUX_GPIO5_U,     /* 5 - D1 */
    Z,  /* 6 */
    Z,  /* 7 */
    Z,  /* 8 */
    PERIPHS_IO_MUX_SD_DATA2_U,  /* 9   - D11 (SD2) */
    PERIPHS_IO_MUX_SD_DATA3_U,  /* 10  - D12 (SD3) */
    Z,  /* 11 */
    PERIPHS_IO_MUX_MTDI_U,      /* 12 - D6 */
    PERIPHS_IO_MUX_MTCK_U,      /* 13 - D7 */
    PERIPHS_IO_MUX_MTMS_U,      /* 14 - D5 */
    PERIPHS_IO_MUX_MTDO_U       /* 15 - D8 */
};

static const int func[] = { 0, 3, 0, 3,   0, 0, Z, Z,   Z, 3, 3, Z,   3, 3, 3, 3 };

/* Put gpio into input mode */
void
pin_input ( int gpio )
{
#ifdef notdef
	PIN_PULLUP_DIS ( mux[gpio] );

	gpio_register_set( GPIO_PIN_ADDR(gpio),
                       GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)  |
                       GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE) |
                       GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
#endif
	// GPIO_DIS_OUTPUT (gpio);
	gpio_output_set(0,0,0, 1<<gpio);
}

/* gpio_output_set() is a routine in the bootrom.
 * it has four arguments.
 * gpio_output_set ( set_mask, clear_mask, enable_mask, disable_mask )
 */

/* Read input value */
int
pin_read ( int gpio )
{
	// return (gpio_input_get()>>gpio) & 1;
	// return gpio_input_get() & (1<<gpio);
	return GPIO_INPUT_GET ( gpio );
}

/* Put gpio into output mode */
void
pin_output ( int gpio )
{
	PIN_FUNC_SELECT ( mux[gpio], func[gpio] );
	PIN_PULLUP_EN ( mux[gpio] );
}

/* Set output state high */
void
pin_high ( int gpio )
{
	int mask = 1 << gpio;

	// GPIO_OUTPUT_SET(gpio, 1);
	gpio_output_set ( mask, 0, mask, 0);
}

/* Set output state low */
void
pin_low ( int gpio )
{
	int mask = 1 << gpio;

	// GPIO_OUTPUT_SET(gpio, 0);
	gpio_output_set(0, mask, mask, 0);
}

/* --------------------------------------------- */
/* --------------------------------------------- */

/* I have tested this only with a MaxDetect RHT03 device
 * It should work just fine with the more common DHT-22
 * or with the am2302 from Aosong.
 *
 * The DHT-11 is slightly different (it returns 8 bit
 *  values for temperature and humidity).
 * It is not supported by this driver, but it would
 *  be quite simple to make some changes to support it.
 */

/* To use this, set up a timer that calls this not
 * more often than every 2 seconds.  This is the spec
 * set by the manufacturer of the sensor, not this driver.
 *
 * Call dht_sensor as follows:
 *
 * int temp, humid;
 * status = dht_sensor ( gpio_pin, &temp, &humid );
 * 
 * status will be 1 if values have been read.
 * values are exactly as delivered by the sensor.
 * both temperature and humidity are scaled by 10.
 * temperatures are degrees centegrade.
 * humidities are in percent.
 */

/* I never see timings longer that about 150 us.
 * (and according to specs, you never should).
 */
#define BIT_TIMEOUT	500

/* Using os_delay_us(1) for timing the bits is problematic.
 * With an 80 Mhz clock, we only have 80 clocks in a
 * millisecond, so we spend considerable time just
 * processing the loop and the call to os_delay_us() itself.
 *
 * Using the ccount register is precise.
 * os_delay_us() just busy waits polling this register
 * anyway, so there is nothing wrong with tying up the
 * processor watching the sensor here.
 *
 * We find that we always enter the following loop looking
 * at the zero (part of the response from the sensor).
 * After watching that for about 60 us, the sensor pulls
 * the line high for about 80 us (we ignore this).
 * Then we start getting data.
 * Each bit holds low a constant time (about 52 us),
 * The length of a high time tells us 0 versus 1.
 * Highs are about 70 us, lows are about 25 us.
 *
 * The process ends with a low time of 45 us,
 * then the line goes high indefinitely.
 */

#define CLOCK_RATE	80	/* ESP8266 runs at 80 Mhz */
#define THRESH		50	/* microseconds, longer than this is a 1 bit */
#define THRESH_CC	(THRESH*CLOCK_RATE)

/* So how long does this whole thing take?
 *
 * You are not supposed to call it faster than every two seconds.
 * But when you do call it...
 *    There is a 20 millisecond start pulse
 *    Gathering 40 bits is 40 * 120 = 4800 microseconds (5 milliseconds).
 * So about 25 milliseconds will be spent in this routine.
 *
 * And what about the disabling of interrupts?
 *
 * This seems to work just fine without doing this, but under a
 *  different regime of other activity, who knows?
 * They are disabled for about 5 milliseconds during the
 *  transmission of data from the sensor.
 * Care is taken not to exit this routine without
 *  reenabling interrupts.
 */

// int w[50];
// int nw;

int
dht_sensor ( int pin, int *temp_p, int *hum_p )
{
    unsigned char dht_data[5];
    int xx, yy;
    int count;
    unsigned long cc1, cc2;
    int data_count, bit_count, data;
    //int sum;
    //int i;

    // put the GPIO into output mode
    pin_output ( pin );

    /* Huge wait time (well, 250 ms)
     * Rather than do this here, we do this at the tail
     * of this routine.  Since this should only be getting
     * called every 2 seconds, this will avoid hogging
     * 1/4 second here.
     */
    // GPIO_OUTPUT_SET(pin, 1);
    // os_delay_us(250000);

    /* Send 20 millisecond start pulse to the sensor */
    pin_low ( pin );
    os_delay_us(20000);

    pin_high ( pin );
    os_delay_us(40);

    pin_input ( pin );

    xx = pin_read ( pin );
    // nw = 0;
    data_count = -1;
    bit_count = 0;
    data = 0;

    os_intr_lock();

    cc1 = xthal_get_ccount ();

    /* Loop through all 40 bits */
    for ( ;; ) {
	cc1 = cc2;
	for ( count = 0; count < BIT_TIMEOUT; ++count ) {
	    yy = pin_read ( pin );
	    if ( xx == yy )
		continue;

	    cc2 = xthal_get_ccount ();
	    if ( xx == 0 ) {
		/* End of zero period, ignore */
		xx = yy;
		break;
	    }

	    /* End of one period */
	    xx = yy;

	    /* First bit is not data, it is the finish
	     * of the startup response from the sensor
	     */
	    if ( data_count < 0 ) {
		data_count++;
		break;
	    }

	    data <<= 1;
	    if ( cc2 - cc1 > THRESH_CC )
		data |= 1;
	    if ( ++bit_count > 7 ) {
		dht_data[data_count++] = data;
		data = 0;
		bit_count = 0;
	    }
	    // w[nw++] = (cc2 - cc1) / 80;
	    break;
	}
	if ( count >= BIT_TIMEOUT )
	    break;
    }

    os_intr_unlock();

    /* Done gathering bits, restore the gpio
     * back to an output and set the state high
     * in preparation for the next read.
     */
    pin_output ( pin );
    pin_high ( pin );

    if ( data_count < 5 )
	return 0;

#ifdef notdef
    for ( i=0; i<nw; i++ ) {
	if ( w[i] > 50 )
	    os_printf ( "run %d of 1 = %d -- 1\n", i, w[i] );
	else
	    os_printf ( "run %d of 1 = %d --   0\n", i, w[i] );
    }

    for ( i=0; i < data_count; i++ ) {
	os_printf ( "  data %d = %04x\n", i, dht_data[i] );
    }

    sum = dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3];
    os_printf ( "sum: %04x\n", sum );
#endif

    if ( ((dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3]) & 0xff) != dht_data[4] ) {
	// os_printf ( "Checksum Bad\n" );
	return 0;
    }

    *hum_p = dht_data[0] << 8 | dht_data[1];
    *temp_p = dht_data[2] << 8 | dht_data[3];

    return 1;
}

/* --------------------------------------------- */
/* --------------------------------------------- */

#define DELAY	2000
// #define GPIO	14	/* D5 */	
// #define GPIO	5	/* D1 */	
// #define GPIO	4	/* D2 */	
#define GPIO	12	/* D6 */	

static os_timer_t timer1;

void
timer_func1 ( void *arg )
{
    int pin = GPIO;
    int t, h;
    int tf;

    (void) dht_sensor ( pin, &t, &h );

    tf = 320 + (t * 9) / 5;

    os_printf ( "humidity = %d\n", h );
    os_printf ( "temperature (C) = %d\n", t );
    os_printf ( "temperature (F) = %d\n", tf );
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

    // pin_input ( GPIO );
    os_printf ( "Ready\n" );
}

/* THE END */
