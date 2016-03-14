/* dht.c
 * Standalone version for the ESP8266
 * Tom Trebisky  1-1-2016
 *
 * This is a driver for the DHT-22, AD2302 (from Adsong),
 * or the RHT03 (from MaxDetect).
 * They all use their own unique one wire protocol
 *  (not to be confused with the Maxim one-wire protocol).
 *
 * This began long ago as dht.cpp by Rob Tillaart
 *  who apparently wrote it for the Arduino ecosystem.
 * I found it in the NodeMCU sources, and hacked it heavily.
 *
 * It came with the notations that follow:
 *
 * DHT Temperature & Humidity Sensor library for Arduino
 * URL: http://arduino.cc/playground/Main/DHTLib
 *  by Rob Tillaart (01/04/2011)
 *
 *  inspired by DHT11 library
 *
 * Released to the public domain
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"
#include "c_types.h"

#ifdef notdef
/* from sdk/include/eagle_soc.h */
#define PIN_PULLUP_DIS(PIN_NAME)                 CLEAR_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLUP)
#define PIN_PULLUP_EN(PIN_NAME)                  SET_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLUP)

/* Strangely, the following is missing in SDK 1.5.0 */
/* Taken from older versions of eagle_soc.h */
/* Not that it seems to make any difference */
/* The old PULLDWN bits are now renamed PULLUP2 and never used */
#define PERIPHS_IO_MUX_PULLUP           BIT7
#define PERIPHS_IO_MUX_PULLDWN          BIT6

#define PIN_PULLDWN_DIS(PIN_NAME)             CLEAR_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLDWN)
#define PIN_PULLDWN_EN(PIN_NAME)              SET_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLDWN)
#endif

/* place holder for unused channels */
#define Z       0

#define os_intr_lock	ets_intr_lock
#define os_intr_unlock	ets_intr_unlock

#define DHTLIB_OK                0
#define DHTLIB_ERROR_CHECKSUM   -1
#define DHTLIB_ERROR_TIMEOUT    -2

#define DHTLIB_INVALID_VALUE    -999

/* This is the length of the first pulse sent to the device
 * in units of milliseconds
 */
#define DHTLIB_DHT11_WAKEUP     18
#define DHTLIB_DHT_WAKEUP       1
#define DHTLIB_DHT_UNI_WAKEUP   18

#define DHT_DEBUG

// max timeout is 100 usec.
// For a 16 Mhz proc 100 usec is 1600 clock cycles
// loops using DHTLIB_TIMEOUT use at least 4 clock cycles
// so 100 us takes max 400 loops
// so by dividing F_CPU by 40000 we "fail" as fast as possible
// ESP8266 uses delay_us get 1us time

#define DHTLIB_TIMEOUT 100

int dht_read_universal(uint8_t pin);
int dht_read11(uint8_t pin);
int dht_read(uint8_t pin);

int dht_getHumidity(void);
int dht_getTemperature(void);

#define COMBINE_HIGH_AND_LOW_BYTE(byte_high, byte_low)  (((byte_high) << 8) | (byte_low))

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

void
pin_setup ( int gpio )
{
	PIN_FUNC_SELECT ( mux[gpio], func[gpio] );
}

/* Put gpio into output mode */
void
pin_output ( int gpio )
{
	// pin_setup ( gpio );

	PIN_FUNC_SELECT ( mux[gpio], func[gpio] );
	PIN_PULLUP_EN ( mux[gpio] );
}

/* Put gpio into input mode */
void
pin_input ( int gpio )
{
#ifdef notdef
	pin_setup ( gpio );

	PIN_PULLUP_DIS ( mux[gpio] );
	// PIN_PULLDWN_DIS ( mux[gpio] );

	gpio_register_set( GPIO_PIN_ADDR(gpio),
                       GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)  |
                       GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE) |
                       GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));

	gpio_output_set(0,0,0, 1<<gpio);
#endif
	GPIO_DIS_OUTPUT (gpio);
}

/* Read input value */
int
pin_read ( int gpio )
{
	// return (gpio_input_get()>>gpio) & 1;
	// return gpio_input_get() & (1<<gpio);

	return GPIO_INPUT_GET ( gpio );
}

/* Set output state high */
void
pin_output_high ( int gpio )
{
	int mask = 1 << gpio;

	gpio_output_set(0, mask, mask, 0);
}

/* Set output state low */
void
pin_output_low ( int gpio )
{
	int mask = 1 << gpio;

	gpio_output_set ( mask, 0, mask, 0);
}

static int dht_humidity;
static int dht_temperature;

int
dht_getHumidity(void)
{
    return dht_humidity;
}

int
dht_getTemperature(void)
{
    return dht_temperature;
}

static uint8_t dht_bytes[5];  // buffer to receive data
static int dht_readSensor(uint8_t pin, uint8_t wakeupDelay);

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int
dht_read_universal(uint8_t pin)
{
    // READ VALUES
    int rv = dht_readSensor(pin, DHTLIB_DHT_UNI_WAKEUP);

    if (rv != DHTLIB_OK) {
        dht_humidity    = DHTLIB_INVALID_VALUE;  // invalid value, or is NaN prefered?
        dht_temperature = DHTLIB_INVALID_VALUE;  // invalid value
        return rv; // propagate error value
    }

#if defined(DHT_DEBUG_BYTES)
    int i;
    for (i = 0; i < 5; i++) {
        DHT_DEBUG("%02X\n", dht_bytes[i]);
    }
#endif // defined(DHT_DEBUG_BYTES)

    // Assume it is DHT11
    // If it is DHT11, both bit[1] and bit[3] is 0
    if ((dht_bytes[1] == 0) && (dht_bytes[3] == 0)) {
        // It may DHT11
        // CONVERT AND STORE

        DHT_DEBUG("DHT11 method\n");
        dht_humidity    = dht_bytes[0];  // dht_bytes[1] == 0;
        dht_temperature = dht_bytes[2];  // dht_bytes[3] == 0;

        // TEST CHECKSUM
        // dht_bytes[1] && dht_bytes[3] both 0
        uint8_t sum = dht_bytes[0] + dht_bytes[2];
        if (dht_bytes[4] != sum) {
            // It may not DHT11
            dht_humidity    = DHTLIB_INVALID_VALUE; // invalid value, or is NaN prefered?
            dht_temperature = DHTLIB_INVALID_VALUE; // invalid value
            // Do nothing
        } else {
            return DHTLIB_OK;
        }
    }

    // Assume it is not DHT11
    // CONVERT AND STORE
    DHT_DEBUG("DHTxx method\n");

    dht_humidity    = COMBINE_HIGH_AND_LOW_BYTE(dht_bytes[0], dht_bytes[1]);
    dht_temperature = COMBINE_HIGH_AND_LOW_BYTE(dht_bytes[2] & 0x7F, dht_bytes[3]);

    if (dht_bytes[2] & 0x80)  // negative dht_temperature
        dht_temperature = -dht_temperature;

    // TEST CHECKSUM
    uint8_t sum = dht_bytes[0] + dht_bytes[1] + dht_bytes[2] + dht_bytes[3];
    if (dht_bytes[4] != sum)
        return DHTLIB_ERROR_CHECKSUM;

    return DHTLIB_OK;
}

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int
dht_read11(uint8_t pin)
{
    // READ VALUES
    int rv = dht_readSensor(pin, DHTLIB_DHT11_WAKEUP);
    if (rv != DHTLIB_OK) {
        dht_humidity    = DHTLIB_INVALID_VALUE; // invalid value, or is NaN prefered?
        dht_temperature = DHTLIB_INVALID_VALUE; // invalid value
        return rv;
    }

    // CONVERT AND STORE
    dht_humidity    = dht_bytes[0];  // dht_bytes[1] == 0;
    dht_temperature = dht_bytes[2];  // dht_bytes[3] == 0;

    // TEST CHECKSUM
    // dht_bytes[1] && dht_bytes[3] both 0
    uint8_t sum = dht_bytes[0] + dht_bytes[2];
    if (dht_bytes[4] != sum)
	return DHTLIB_ERROR_CHECKSUM;

    return DHTLIB_OK;
}


// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int
dht_read(uint8_t pin)
{
    // READ VALUES
    int rv = dht_readSensor(pin, DHTLIB_DHT_WAKEUP);
    if (rv != DHTLIB_OK) {
        dht_humidity    = DHTLIB_INVALID_VALUE;  // invalid value, or is NaN prefered?
        dht_temperature = DHTLIB_INVALID_VALUE;  // invalid value
        return rv; // propagate error value
    }

    // CONVERT AND STORE
    dht_humidity    = COMBINE_HIGH_AND_LOW_BYTE(dht_bytes[0], dht_bytes[1]);
    dht_temperature = COMBINE_HIGH_AND_LOW_BYTE(dht_bytes[2] & 0x7F, dht_bytes[3]);
    if (dht_bytes[2] & 0x80)  // negative dht_temperature
        dht_temperature = -dht_temperature;

    // TEST CHECKSUM
    uint8_t sum = dht_bytes[0] + dht_bytes[1] + dht_bytes[2] + dht_bytes[3];
    if (dht_bytes[4] != sum)
        return DHTLIB_ERROR_CHECKSUM;

    return DHTLIB_OK;
}

/////////////////////////////////////////////////////
//
// PRIVATE
//

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_TIMEOUT
int
dht_readSensor(uint8_t pin, uint8_t wakeupDelay)
{
    // INIT BUFFERVAR TO RECEIVE DATA
    uint8_t mask = 128;
    uint8_t idx = 0;
    uint8_t i = 0;

    for (i = 0; i < 5; i++)
	dht_bytes[i] = 0;

    // REQUEST SAMPLE
    // pinMode(pin, OUTPUT);
    // platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP);
    // DIRECT_MODE_OUTPUT(pin);
    pin_output ( pin );

    // digitalWrite(pin, LOW); // T-be
    // DIRECT_WRITE_LOW(pin);
    pin_output_low ( pin );

    // delay(wakeupDelay);
    for (i = 0; i < wakeupDelay; i++)
	os_delay_us(1000);

    /* XXX - note that if this takes and error exit,
     * it leaves interrupts locked, which is not good.
     */
    os_intr_lock();

    // digitalWrite(pin, HIGH);   // T-go
    // DIRECT_WRITE_HIGH(pin);
    pin_output_high ( pin );

    os_delay_us(40);

    // pinMode(pin, INPUT);
    // DIRECT_MODE_INPUT(pin);
    pin_input ( pin );

    // GET ACKNOWLEDGE or TIMEOUT

    uint16_t loopCntLOW = DHTLIB_TIMEOUT;
    while ( ! pin_read(pin) ) {
        os_delay_us(1);
        if (--loopCntLOW == 0)
	    return -2;
	    // return DHTLIB_ERROR_TIMEOUT;
    }

    uint16_t loopCntHIGH = DHTLIB_TIMEOUT;
    while ( pin_read(pin) ) {
        os_delay_us(1);
        if (--loopCntHIGH == 0)
	    return -3;
	    // return DHTLIB_ERROR_TIMEOUT;
    }

    // READ THE OUTPUT - 40 BITS => 5 BYTES
    for (i = 40; i > 0; i--) {
        loopCntLOW = DHTLIB_TIMEOUT;
        // while (pin_read(pin) == LOW ) {
        while ( ! pin_read(pin) ) {
            os_delay_us(1);
            if (--loopCntLOW == 0)
		return -4;
		// return DHTLIB_ERROR_TIMEOUT;
        }

        uint32_t t = system_get_time();

        loopCntHIGH = DHTLIB_TIMEOUT;
        // while (pin_read(pin) != LOW ) {
        while ( pin_read(pin) ) {
            os_delay_us(1);
            if (--loopCntHIGH == 0)
		return -5;
		// return DHTLIB_ERROR_TIMEOUT;
        }

        if ((system_get_time() - t) > 40) {
            dht_bytes[idx] |= mask;
        }

        mask >>= 1;
        if (mask == 0) {   // next byte?
            mask = 128;
            idx++;
        }
    }

    // Enable interrupts
    os_intr_unlock();

#ifdef notdef
    // pinMode(pin, OUTPUT);
    // DIRECT_MODE_OUTPUT(pin);
    pin_output ( pin );

    // digitalWrite(pin, HIGH);
    //DIRECT_WRITE_HIGH(pin);
    pin_output_high ( pin );
#endif

    return DHTLIB_OK;
}

int w[200];

int
dht_watch ( uint8_t pin, uint8_t wakeupDelay )
{
    // INIT BUFFERVAR TO RECEIVE DATA
    uint8_t mask = 128;
    uint8_t idx = 0;
    uint8_t i = 0;
    int xx, yy;
    int count;
    int run;
    int nw = 0;

    for (i = 0; i < 5; i++)
	dht_bytes[i] = 0;

    // pin_output ( pin );
    PIN_FUNC_SELECT ( mux[pin], func[pin] );
    PIN_PULLUP_EN ( mux[pin] );

    GPIO_OUTPUT_SET(pin, 1);
    os_delay_us(250000);

    GPIO_OUTPUT_SET(pin, 0);
    os_delay_us(20000);

    GPIO_OUTPUT_SET(pin, 1);
    os_delay_us(40);

#ifdef notdef
    pin_output_low ( pin );

    // delay for N milliseconds
    for (i = 0; i < wakeupDelay; i++)
	os_delay_us(1000);

    /* XXX - note that if this takes an error exit,
     * it leaves interrupts locked, which is not good.
     */

    pin_output_high ( pin );
    os_delay_us(40);
#endif

    // pin_input ( pin );
    GPIO_DIS_OUTPUT ( pin );

    // os_intr_lock();

#ifdef notdef
    /* This is hopeless, the os_printf ruins the timing */
    xx = pin_read ( pin );
    // os_printf ( "Pin began with %08x\n", xx );

    /* Transitions hi to low after 272 iterations
     * -- prior to os_delay
     * After os_delay this is 69 iterations
     */
    run = 1;
    while ( run ) {
	count = 0;
	for ( ;; ) {
	    yy = pin_read ( pin );
	    if ( xx != yy ) {
		os_printf ( "Pin changed after %d to %08x\n", count, yy );
		xx = yy;
		break;
	    }
	    ++count;
	    if ( count > 100000 ) {
		os_printf ( "Watch abandoned\n" );
		run = 0;
		break;
	    }
	    // os_delay_us(1);
	}
    }
#endif

#define BIAS 10000

/* In the following, we get counts as follows
 * without the delay for zeros we get 99 count.
 * with the delay:
 *	0 counts always 25 +- 1
 *	1 counts are 13 or 35
 */
    xx = pin_read ( pin );
    run = 1;
    while ( run ) {
	count = 0;
	for ( ;; ) {
	    yy = pin_read ( pin );
	    if ( xx != yy ) {
		if ( xx )
		    w[nw++] = count;
		else
		    w[nw++] = BIAS + count;
		xx = yy;
		break;
	    }
	    ++count;
	    if ( count > 100000 ) {
		// os_printf ( "Watch terminated\n" );
		run = 0;
		break;
	    }
	    os_delay_us(1);
	}
    }



    for ( i=0; i<nw; i++ ) {
	if ( w[i] > BIAS )
	    os_printf ( "run %d of 0 = %d\n", i, w[i] - BIAS );
	else
	    os_printf ( "run %d of 1 = %d\n", i, w[i] );
    }

    // os_intr_unlock();

#ifdef notdef
    uint16_t loopCntLOW = DHTLIB_TIMEOUT;
    while ( ! pin_read(pin) ) {
        os_delay_us(1);
        if (--loopCntLOW == 0)
	    return -2;
	    // return DHTLIB_ERROR_TIMEOUT;
    }

    uint16_t loopCntHIGH = DHTLIB_TIMEOUT;
    while ( pin_read(pin) ) {
        os_delay_us(1);
        if (--loopCntHIGH == 0)
	    return -3;
	    // return DHTLIB_ERROR_TIMEOUT;
    }

    // READ THE OUTPUT - 40 BITS => 5 BYTES
    for (i = 40; i > 0; i--) {
        loopCntLOW = DHTLIB_TIMEOUT;
        // while (pin_read(pin) == LOW ) {
        while ( ! pin_read(pin) ) {
            os_delay_us(1);
            if (--loopCntLOW == 0)
		return -4;
		// return DHTLIB_ERROR_TIMEOUT;
        }

        uint32_t t = system_get_time();

        loopCntHIGH = DHTLIB_TIMEOUT;
        // while (pin_read(pin) != LOW ) {
        while ( pin_read(pin) ) {
            os_delay_us(1);
            if (--loopCntHIGH == 0)
		return -5;
		// return DHTLIB_ERROR_TIMEOUT;
        }

        if ((system_get_time() - t) > 40) {
            dht_bytes[idx] |= mask;
        }

        mask >>= 1;
        if (mask == 0) {   // next byte?
            mask = 128;
            idx++;
        }
    }

    // Enable interrupts
    os_intr_unlock();

    // pinMode(pin, OUTPUT);
    // DIRECT_MODE_OUTPUT(pin);
    pin_output ( pin );

    // digitalWrite(pin, HIGH);
    //DIRECT_WRITE_HIGH(pin);
    pin_output_high ( pin );
#endif

    return DHTLIB_OK;
}

/* --------------------------------------------- */
/* --------------------------------------------- */

/* --------------------------------------------- */
/* --------------------------------------------- */

// #define DELAY	100
// #define DELAY	2000
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
    int val;

#ifdef notdef
    // Quick check to see what we read
    // val = pin_read ( pin );
    val = gpio_input_get();
    os_printf ( "Val = %08x\n", val );

    // Verify timing
    val = system_get_time();
    os_printf ( "System time 1: %d\n", val );
	os_delay_us((uint16)1000);
    val = system_get_time();
    os_printf ( "System time 2: %d\n", val );
#endif

    (void) dht_watch ( pin, 20 );
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
