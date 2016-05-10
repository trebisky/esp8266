/* bmp.c
 * Tom Trebisky  5-1-2016
 *
 * My first stab at a bmp180 driver
 * as well as an i2c driver
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"
#include "c_types.h"

/* ---------------------------------- */
/* i2c library */
/* ---------------------------------- */

static unsigned char sda_pin;
static unsigned char scl_pin;

static unsigned char sda_mask;
static unsigned char scl_mask;

static unsigned char sda_val;
static unsigned char scl_val;

static void
iic_dc_set ( int data, int clock )
{
    int high_mask = 0;
    int low_mask = 0;

    sda_val = data;
    scl_val = clock;

    if ( data )
	high_mask |= sda_mask;
    else
	low_mask |= sda_mask;

    if ( clock )
	high_mask |= scl_mask;
    else
	low_mask |= scl_mask;

    pin_mask ( high_mask, low_mask );
}

void
iic_dc ( int data, int clock )
{
    iic_dc_set ( data, clock );
    os_delay_us ( 5 );
}

void
iic_dc_wait ( int data, int clock, int wait )
{
    iic_dc_set ( data, clock );
    os_delay_us ( wait );
}

int
iic_get_sda ( void )
{
    return pin_read ( sda_pin );
}

void
iic_start ( void )
{
    iic_dc ( 1, scl_val );
    iic_dc ( 1, 1 );
    iic_dc ( 0, 1 );
}

void
iic_stop ( void )
{
    os_delay_us ( 5 );
    iic_dc ( 0, scl_val );
    iic_dc ( 0, 1 );
    iic_dc ( 1, 1 );
}

static void
iic_setack ( int level )
{
    iic_dc ( sda_val, 0 );
    iic_dc ( level, 0 );
    iic_dc_wait ( level, 1, 8 );
    iic_dc ( level, 0 );
    iic_dc ( 1, 0 );
}

#ifdef notdef
void
iic_send_ack ( void )
{
    iic_setack ( 0 );
}

void
iic_send_nack ( void )
{
    iic_setack ( 1 );
}
#endif

int
iic_getack ( void )
{
    int rv;

    iic_dc ( sda_val, 0 );
    iic_dc ( 1, 0 );
    iic_dc ( 1, 1 );

    rv = iic_get_sda ();
    os_delay_us ( 5 );

    iic_dc ( 1, 0 );
    return rv;
}

// void ICACHE_FLASH_ATTR
static int
iic_send ( data )
{
    int bit;
    int i;

    os_delay_us ( 5 );
    iic_dc ( sda_val, 0 );

    for (i = 7; i >= 0; i--) {
        bit = data >> i;
	iic_dc ( bit, 0 );
	iic_dc ( bit, 1 );
	if ( i == 0 )
	    os_delay_us ( 3 );
	iic_dc ( bit, 0 );
    }
    return iic_getack ();
}

static int
iic_recv ( void )
{
    int rv = 0;
    int i;
    int bit;

    os_delay_us ( 5 );
    iic_dc ( sda_val, 0 );

    for (i = 0; i < 8; i++) {
	os_delay_us ( 5 );
	iic_dc ( 1, 0 );
	iic_dc ( 1, 1 );

	bit = iic_get_sda ();
	os_delay_us ( 5 );
	if ( i == 7 ) 
	    os_delay_us ( 3 );
	rv |= bit << (7-i);
    }

    iic_dc ( 1, 0 );
    return rv;
}

void
iic_read ( unsigned char *buf, int count )
{
    int i;

    for ( i=0; i < count-1; i++ ) {
	*buf++ = iic_recv ();
	iic_setack ( 1 );	/* ack */
    }
    *buf = iic_recv ();
    iic_setack ( 0 );	/* nack */
}

void
iic_address_w ( int addr )
{
    (void) iic_send ( addr << 1 );
}

void
iic_address_r ( int addr )
{
    (void) iic_send ( (addr << 1) | 1 );
}

void
iic_read_reg ( int dev, int reg, unsigned char *buf, int count )
{
    iic_start ();
    iic_address_w ( dev );
    (void) iic_send ( reg );
    iic_stop ();

    iic_start ();
    iic_address_r ( dev );
    iic_read ( buf, count );
    iic_stop ();
}

void
iic_write_reg ( int dev, int reg, int val )
{
    iic_start ();
    iic_address_w ( dev );
    (void) iic_send ( reg );
    (void) iic_send ( val );
    iic_stop ();
}

#define MAX_DATA	28

void
iic_init ( int sda, int scl )
{
    int i;

    sda_pin = sda;
    scl_pin = scl;

    sda_mask = 1 << sda;
    scl_mask = 1 << scl;

    /* perform GPIO initialization XXX */
    pin_iic ( sda );
    pin_iic ( scl );

    iic_dc ( 1, 0 );
    iic_dc ( 0, 0 );
    iic_dc ( 1, 0 );

    for ( i=0; i < MAX_DATA; i++ ) {
	iic_dc ( 1, 0 );
	iic_dc ( 1, 1 );
    }

    iic_stop ();
}

/* ---------------------------------- */
/* bmp180 library */
/* ---------------------------------- */

/* The data sheet says that the address is
 * 0xEE for writes and 0xEF for reads.
 * What is done is that the following is shifted left
 * one bit and the 0 or 1 added on the end.
 */
#define BMP_ADDR	0x77

#define BMP_REG_CAL	0xAA
#define BMP_REG_ID	0xD0	/* always yields 0x55 */
#define BMP_REG_CTL	0xF4
#define BMP_REG_DATA	0xF6	/* msb, lsb, xlsb */

/* calibration data consists of 11 16 bit words,
 * stored msb then lsb.
 * none will ever be all zeros or all ones
 */
#define NUM_CALS	22

#define CMD_TEMP	0x2E
#define CMD_PRESS	0x34

int
bmp_temp ( void )
{
    unsigned char buf[2];

    iic_write_reg ( BMP_ADDR, BMP_REG_CTL, CMD_TEMP );
    os_delay_us ( 4500 );
    iic_read_reg ( BMP_ADDR, BMP_REG_DATA, buf, 2 );

    return buf[0] << 8 | buf[1];
}

int
bmp_id ( void )
{
    unsigned char id = 0;

    iic_read_reg ( BMP_ADDR, BMP_REG_ID, &id, 1 );

    return id;
}

/* ---------------------------------- */
/* ---------------------------------- */

/* These are GPIO numbers, not NodeMCU pin numbers */

#define SDA_PIN		4	/* nodemcu D2 */
#define SCL_PIN		5	/* nodemcu D1 */

// #define SCOPE		13	/* D7 */
#define SCOPE		4

void
scope ( void )
{
    pin_output ( SCOPE );

    for ( ;; ) {
	system_soft_wdt_feed();

	os_delay_us ( 1000 );
	pin_high ( SCOPE );
	// (void) bmp_id ();
	os_delay_us ( 1000 );
	pin_low ( SCOPE );

    }
}

void
user_init( void )
{
    unsigned short val;

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf( "\n");
    os_printf( "running !!\n" );

    iic_init ( SDA_PIN, SCL_PIN );

    // os_printf ( "BMP180 id = %x\n", bmp_id () );

    /* Without the call to wdt_feed, a watchdog triggers
     * after 1 second and resets the device.
     */
    os_printf ( "looping ...\n" );
    scope ();
}

/* THE END */
