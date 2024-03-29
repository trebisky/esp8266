/* dht.c
 * Standalone version for the ESP8266
 * Tom Trebisky  1-1-2016
 *
 * This began as dht.cpp by Rob Tillaart
 * I found it in the NodeMCU sources with the
 * notations that follow:
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

/* place holder for unused channels */
#define Z       0

#define os_intr_lock	ets_intr_lock
#define os_intr_unlock	ets_intr_unlock

#include "user_interface.h"
#include "c_types.h"

#define DHTLIB_OK                0
#define DHTLIB_ERROR_CHECKSUM   -1
#define DHTLIB_ERROR_TIMEOUT    -2

#define DHTLIB_INVALID_VALUE    -999

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
#define DHTLIB_TIMEOUT (100)

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
	pin_setup ( gpio );
}

/* Put gpio into input mode */
void
pin_input ( int gpio )
{
	pin_setup ( gpio );
	PIN_PULLUP_DIS ( mux[gpio] );
	PIN_PULLDWN_DIS ( mux[gpio] );

	gpio_output_set(0,0,0, 1<<gpio);
}

/* Read input value */
int
pin_input_read ( int gpio )
{
	// return (gpio_input_get()>>gpio) & 1;
	return gpio_input_get() & (1<<gpio);
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
    while ( ! pin_input_read(pin) ) {
        os_delay_us(1);
        if (--loopCntLOW == 0)
	    return -2;
	    // return DHTLIB_ERROR_TIMEOUT;
    }

    uint16_t loopCntHIGH = DHTLIB_TIMEOUT;
    while ( pin_input_read(pin) ) {
        os_delay_us(1);
        if (--loopCntHIGH == 0)
	    return -3;
	    // return DHTLIB_ERROR_TIMEOUT;
    }

    // READ THE OUTPUT - 40 BITS => 5 BYTES
    for (i = 40; i > 0; i--) {
        loopCntLOW = DHTLIB_TIMEOUT;
        // while (pin_input_read(pin) == LOW ) {
        while ( ! pin_input_read(pin) ) {
            os_delay_us(1);
            if (--loopCntLOW == 0)
		return -4;
		// return DHTLIB_ERROR_TIMEOUT;
        }

        uint32_t t = system_get_time();

        loopCntHIGH = DHTLIB_TIMEOUT;
        // while (pin_input_read(pin) != LOW ) {
        while ( pin_input_read(pin) ) {
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

    return DHTLIB_OK;
}

/* --------------------------------------------- */
/* --------------------------------------------- */

#define DELAY	2000
// #define GPIO	14	/* D5 */	
// #define GPIO	5	/* D1 */	
#define GPIO	4	/* D2 */	

static os_timer_t timer1;

void
timer_func1 ( void *arg )
{
    int t;
    int h;
    int s;

    s = dht_read ( GPIO );
    h = dht_getHumidity ();
    t = dht_getTemperature ();

    os_printf ( "Status: %d\n", s );
    os_printf ( " Temp:  %d\n", t );
    os_printf ( " Humid: %d\n", h );
}

void
user_init( void )
{
    os_timer_disarm ( &timer1 );
    os_timer_setfn ( &timer1, timer_func1, NULL );
    os_timer_arm ( &timer1, 2000, 1 );

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());
}

/* THE END */
