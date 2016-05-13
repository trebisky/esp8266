/* Tom Trebisky
 *    i2c "library"
 * 5-11-2016
 *
 * A key idea is that the sda and scl pins can be
 * specified as arguments to the initializer function.
 *
 * A fairly high level interface is presented as an API.
 * patterned after i2c_master.c
 */

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"

#include "iic.h"

static void iic_setdc ( int, int );
static void iic_dc ( int, int );
static void iic_dc_wait ( int, int, int );
static void iic_writeb ( int );
static int iic_readb ( void );
static void iic_setAck ( int );
static int iic_getAck ( void );

/* The following functions are all that a person
 * should ever need to use.
 */
void iic_init ( int, int );
void iic_start ( void );
void iic_stop ( void );
int iic_recv_byte ( int );
int iic_send_byte ( int );

/* -------------------------------------------------- */

/* Part of a general GPIO facility. */

/* place holder for unused channels */
#define Z       0

/* This is an array of pin control register addresses.
 */
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

/* These are the mux values that put a pin into GPIO mode
 */
static const uint8 func[] = { 0, 3, 0, 3,   0, 0, Z, Z,   Z, 3, 3, Z,   3, 3, 3, 3 };

static void ICACHE_FLASH_ATTR
gpio_iic_setup ( int gpio )
{
    int reg;

    PIN_FUNC_SELECT ( mux[gpio], func[gpio] );

    /* make this open drain */
    reg = GPIO_PIN_ADDR ( gpio );
    GPIO_REG_WRITE ( reg, GPIO_REG_READ( reg ) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE) );

    GPIO_REG_WRITE (GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << gpio) );
}

/* -------------------------------------------------- */

static uint8 sda_pin;
static uint8 scl_pin;	/* never used */

static uint8 cur_sda;
static uint8 cur_scl;

static uint8 sda_mask;
static uint8 scl_mask;
static uint8 all_mask;

#define MAX_BITS	28

static void ICACHE_FLASH_ATTR
iic_bus_init ( void )
{
    int i;

    iic_dc ( 1, 0 );

    iic_dc ( 0, 0 );
    iic_dc ( 1, 0 );

    for (i = 0; i < MAX_BITS; i++) {
	iic_dc ( 1, 0 );
	iic_dc ( 1, 1 );
    }

    iic_stop();
}

static void ICACHE_FLASH_ATTR
iic_gpio_init ( int sda, int scl )
{
    sda_pin = sda;
    scl_pin = scl;

    sda_mask = 1 << sda;
    scl_mask = 1 << scl;
    all_mask = sda_mask | scl_mask;

    ETS_GPIO_INTR_DISABLE() ;

    gpio_iic_setup ( sda );
    gpio_iic_setup ( scl );

    iic_setdc ( 1, 1 );

    ETS_GPIO_INTR_ENABLE() ;
}

/* Arguments are gpio numbers, 0-15 */
void ICACHE_FLASH_ATTR
iic_init ( int sda_pin, int scl_pin )
{
    iic_gpio_init ( sda_pin, scl_pin );
    iic_bus_init ();
}

/* -------------------------------------------------- */

/* Could be a macro */
static int ICACHE_FLASH_ATTR
iic_getbit ( void )
{
    // return GPIO_INPUT_GET ( SDA_GPIO );
    return ( gpio_input_get() >> sda_pin) & 1;
}

static void ICACHE_FLASH_ATTR
iic_setdc ( int sda, int scl )
{
    int high_mask;
    int low_mask;

    cur_sda = sda;
    cur_scl = scl;

    if ( sda ) {
        high_mask = sda_mask;
        low_mask = 0;
    } else {
        high_mask = 0;
        low_mask = sda_mask;
    }

    if ( scl )
        high_mask |= scl_mask;
    else
        low_mask |= scl_mask;

    gpio_output_set( high_mask, low_mask, all_mask, 0);
}

static void ICACHE_FLASH_ATTR
iic_dc ( int data, int clock )
{
    iic_setdc ( data, clock );
    os_delay_us ( 5 );
}

static void ICACHE_FLASH_ATTR
iic_dc_wait ( int data, int clock, int wait )
{
    iic_setdc ( data, clock );
    os_delay_us ( wait );
}

void ICACHE_FLASH_ATTR
iic_start(void)
{
    iic_dc ( 1, cur_scl );
    iic_dc ( 1, 1 );
    iic_dc ( 0, 1 );
}

void ICACHE_FLASH_ATTR
iic_stop(void)
{
    os_delay_us (5);

    iic_dc ( 0, cur_scl );
    iic_dc ( 0, 1 );
    iic_dc ( 1, 1 );
}

static void ICACHE_FLASH_ATTR
iic_setAck ( int level )
{
    iic_dc ( cur_sda, 0 );
    iic_dc ( level, 0 );
    iic_dc ( level, 1 );
    iic_dc ( level, 0 );
    iic_dc ( 1, 0 );
}

static int ICACHE_FLASH_ATTR
iic_getAck ( void )
{
    int rv;

    iic_dc ( cur_sda, 0 );
    iic_dc ( 1, 0 );
    iic_dc ( 1, 1 );

    rv = iic_getbit ();
    os_delay_us (5);

    iic_dc ( 1, 0 );

    return rv;
}

static int ICACHE_FLASH_ATTR
iic_readb ( void )
{
    int rv = 0;
    int i;

    os_delay_us (5);

    iic_dc ( cur_sda, 0 );

    for (i = 0; i < 8; i++) {
        os_delay_us (5);
	iic_dc ( 1, 0 );
	iic_dc ( 1, 1 );

        rv |= iic_getbit () << (7-i);

	iic_dc_wait ( 1, 1, i == 7 ? 8 : 5 );
    }

    iic_dc ( 1, 0 );

    return rv;
}

static void ICACHE_FLASH_ATTR
iic_writeb ( int data )
{
    int bit;
    int i;

    os_delay_us (5);

    iic_dc ( cur_sda, 0 );

    for (i = 7; i >= 0; i--) {
        // bit = data >> i;
        bit = (data >> i) & 1;
	iic_dc ( bit, 0 );
	iic_dc_wait ( bit, 1, i == 0 ? 8 : 5 );
	iic_dc ( bit, 0 );
    }
}

int ICACHE_FLASH_ATTR
iic_send_byte ( int byte )
{
	int ack;

	iic_writeb ( byte );
	ack = iic_getAck();
	if ( ack ) {
	    iic_stop();
	    return 1;
	}
	return 0;
}

/* XXX - useful when debugging
 *  but not what I would want when in production.
 */
int ICACHE_FLASH_ATTR
iic_send_byte_m ( int byte, char *msg )
{
	int ack;

	iic_writeb ( byte );
	ack = iic_getAck();
	if ( ack ) {
	    os_printf("I2C: No ack after sending %s\n", msg);
	    iic_stop();
	    return 1;
	}
	return 0;
}

int ICACHE_FLASH_ATTR
iic_recv_byte ( int ack )
{
	int rv;

	rv = iic_readb();
	iic_setAck ( ack );
	return rv;
}

/* THE END */
