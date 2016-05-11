/*
The MIT License (MIT)

Copyright (c) 2014 Matt Callow

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>

// #include "driver/uart.h"
#include "i2c_master.h"

extern void wdt_feed(void);

#define  BMP_ADDR 0x77
#define  BMP_ADDR_R ((BMP_ADDR << 1) | 1)
#define  BMP_ADDR_W (BMP_ADDR << 1)

#define  REG_CALS		0xAA
#define  REG_CONTROL	0xF4
#define  REG_RESULT		0xF6
#define  REG_ID			0xD0

#define  CMD_TEMP 0x2E
#define  TDELAY 4500

/* OSS can have values 0, 1, 2, 3
 * representing internal sampling of 1, 2, 4, or 8 values
 * note that sampling more will use more power.
 *  (since each conversion takes about the same dose of power)
 */

#define  OSS_ULP	0			/* 16 bit - Ultra Low Power */
#define  CMD_P_ULP  0x34
#define  DELAY_ULP  4500
#define  SHIFT_ULP  8

#define  OSS_STD	1			/* 17 bit - Standard */
#define  CMD_P_STD  0x74
#define  DELAY_STD  7500
#define  SHIFT_STD	7

#define  OSS_HR		2			/* 18 bit - High Resolution */
#define  CMD_P_HR   0xB4
#define  DELAY_HR   13500
#define  SHIFT_HR	6

#define  OSS_UHR	3			/* 19 bit - Ultra High Resolution */
#define  CMD_P_UHR  0xF4
#define  DELAY_UHR  25500
#define  SHIFT_UHR	5

/* This chooses from the above */
#define  OSS 		OSS_STD
#define  CMD_PRESS	CMD_P_STD
#define  PDELAY 	DELAY_STD
#define  PSHIFT 	SHIFT_STD
// #define  PSHIFT 	(8-OSS)

#define NCALS	11

struct bmp_cals {
	short ac1;
	short ac2;
	short ac3;
	unsigned short ac4;
	unsigned short ac5;
	unsigned short ac6;
	short b1;
	short b2;
	short mb;
	short mc;
	short md;
} bmp_cal;

#define	AC1	bmp_cal.ac1
#define	AC2	bmp_cal.ac2
#define	AC3	bmp_cal.ac3
#define	AC4	bmp_cal.ac4
#define	AC5	bmp_cal.ac5
#define	AC6	bmp_cal.ac6
#define	B1	bmp_cal.b1
#define	B2	bmp_cal.b2
#define	MB	bmp_cal.mb	/* never used */
#define	MC	bmp_cal.mc
#define	MD	bmp_cal.md

LOCAL int ICACHE_FLASH_ATTR
send_byte ( int byte, char *msg )
{
	int ack;

	i2c_master_writeByte ( byte );
	ack = i2c_master_getAck();
	if ( ack ) {
		os_printf("I2C: No ack after sending %s\n", msg);
		i2c_master_stop();
		return 1;
	}
	return 0;
}

LOCAL int ICACHE_FLASH_ATTR
recv_byte ( int ack )
{
	int rv;

	rv = i2c_master_readByte();
	i2c_master_setAck ( ack );
	return rv;
}

/* ID register should always yield 0x55 */
LOCAL int ICACHE_FLASH_ATTR
read_id ( void )
{
	int id;

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_W, "W address" ) ) return;
	if ( send_byte ( REG_ID, "reg" ) ) return;
	i2c_master_stop();

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_R, "R address" ) ) return;
	id = recv_byte ( 1 );
	i2c_master_stop();

	// os_printf("I2C: ID = 0x%02x\n", id );
	return id;
}

LOCAL int ICACHE_FLASH_ATTR
read_temp ( void )
{
	int rv;

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_W, "W address" ) ) return;
	if ( send_byte ( REG_CONTROL, "reg" ) ) return;
	if ( send_byte ( CMD_TEMP, "cmd" ) ) return;
	i2c_master_stop();

    os_delay_us ( TDELAY );

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_W, "W address" ) ) return;
	if ( send_byte ( REG_RESULT, "reg" ) ) return;
	i2c_master_stop();

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_R, "R address" ) ) return;
	rv =  recv_byte ( 0 ) << 8;
	rv |= recv_byte ( 1 );
	i2c_master_stop();

	// os_printf("I2C: read(MSB/LSB)=(0x%02x/0x%02x)\n", msb, lsb);
	return rv;
}

/* Pressure reads 3 bytes
 *  - note that i2c devices just read out consecutive registers
 *  until you send a nack
 */

LOCAL int ICACHE_FLASH_ATTR
read_pressure ( void )
{
	int rv;

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_W, "W address" ) ) return;
	if ( send_byte ( REG_CONTROL, "reg" ) ) return;
	if ( send_byte ( CMD_PRESS, "cmd" ) ) return;
	i2c_master_stop();

    os_delay_us ( PDELAY );

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_W, "W address" ) ) return;
	if ( send_byte ( REG_RESULT, "reg" ) ) return;
	i2c_master_stop();

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_R, "R address" ) ) return;
	rv =  recv_byte ( 0 ) << 16;
	rv |=  recv_byte ( 0 ) << 8;
	rv |=  recv_byte ( 1 );
	i2c_master_stop();

	rv >>= PSHIFT;

	// os_printf("I2C: pressure = %08x\n", rv );
	return rv;
}

LOCAL void ICACHE_FLASH_ATTR
read_cals ( unsigned short *buf )
{
	int val;
	int i;

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_W, "W address" ) ) return;
	if ( send_byte ( REG_CALS, "reg" ) ) return;
	i2c_master_stop();

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_R, "R address" ) ) return;
	for ( i=0; i < NCALS; i++ ) {
		val = recv_byte ( 0 ) << 8;
		val |= recv_byte ( i == NCALS - 1 ? 1 : 0 );
		*buf++ = val;
	}
	i2c_master_stop();
}

/* ---------------------------------------------- */
/* ---------------------------------------------- */

#ifdef notdef
/* This raw pressure value is already shifted by 8-oss */
int rawt = 25179;
int rawp = 77455;

int ac1 = 8240;
int ac2 = -1196;
int ac3 = -14709;
int ac4 = 32912;	/* U */
int ac5 = 24959;	/* U */
int ac6 = 16487;	/* U */
int b1 = 6515;
int b2 = 48;
int mb = -32768;
int mc = -11786;
int md = 2845;
#endif

int b5;

/* Yields temperature in 0.1 degrees C */
int
conv_temp ( int raw )
{
	int x1, x2;

	x1 = ((raw - AC6) * AC5) >> 15;
	x2 = MC * 2048 / (x1 + MD);
	b5 = x1 + x2;
	return (b5 + 8) >> 4;
}

/* Yields pressure in Pa */
int
conv_pressure ( int raw )
{
	int b3, b6;
	unsigned long b4, b7;
	int x1, x2, x3;
	int p;

	b6 = b5 - 4000;
	x1 = B2 * (b6 * b6 / 4096) / 2048;
	x2 = AC2 * b6 / 2048;
	x3 = x1 + x2;
	b3 = ( ((AC1 * 4 + x3) << OSS) + 2) / 4;
	x1 = AC3 * b6 / 8192;
	x2 = (B1 * (b6 * b6 / 4096)) / 65536;
	x3 = (x1 + x2 + 2) / 4;

	b4 = AC4 * (x3 + 32768) / 32768;
	b7 = (raw - b3) * (50000 >> OSS);

	p = (b7 / b4) * 2;
	x1 = (p / 256) * (p / 256);
	x1 = (x1 * 3038) / 65536;
	x2 = (-7357 * p) / 65536;
	p += (x1 + x2 + 3791) / 16;

	return p;
}

#ifdef notdef
int
conv_tempX ( int raw )
{
	int x1, x2;

	x1 = (raw - ac6) * ac5 / 32768;
	x2 = mc * 2048 / (x1 + md);
	b5 = x1 + x2;
	return (b5+8) / 16;
}

int
conv_pressureX ( int raw, int oss )
{
	int b3, b6;
	unsigned long b4, b7;
	int x1, x2, x3;
	int p;

	b6 = b5 - 4000;
	x1 = b2 * (b6 * b6 / 4096) / 2048;
	x2 = ac2 * b6 / 2048;
	x3 = x1 + x2;
	b3 = ( ((ac1 * 4 + x3) << oss) + 2) / 4;
	x1 = ac3 * b6 / 8192;
	x2 = (b1 * (b6 * b6 / 4096)) / 65536;
	x3 = (x1 + x2 + 2) / 4;

	b4 = ac4 * (x3 + 32768) / 32768;
	b7 = (raw - b3) * (50000 >> oss);

	p = (b7 / b4) * 2;
	x1 = (p / 256) * (p / 256);
	x1 = (x1 * 3038) / 65536;
	x2 = (-7357 * p) / 65536;
	p += (x1 + x2 + 3791) / 16;

	return p;
}
#endif

// #define MB_TUCSON	84.0
#define MB_TUCSON	8400

int
convert ( int rawt, int rawp )
{
	int t;
	int p;
	//int t2;
	//int p2;
	int tf;
	int pmb_sea;

	// double tc;
	// double tf;
	// double pmb;
	// double pmb_sea;

	t = conv_temp ( rawt );
	//t2 = conv_tempX ( rawt );
	// tc = t / 10.0;
	// tf = 32.0 + tc * 1.8;
	tf = t * 18;
	tf = 320 + tf / 10;

	// os_printf ( "Temp = %d\n", t );
	// os_printf ( "Temp (C) = %.3f\n", tc );
	// os_printf ( "Temp (F) = %.3f\n", tf );
	// os_printf ( "Temp = %d (%d)\n", t, tf );

	p = conv_pressure ( rawp );
	// p2 = conv_pressureX ( rawp, OSS );
	// pmb = p / 100.0;
	// pmb_sea = pmb + MB_TUCSON;
	pmb_sea = p + MB_TUCSON;

	// os_printf ( "Pressure (Pa) = %d\n", p );
	// os_printf ( "Pressure (mb) = %.2f\n", pmb );
	// os_printf ( "Pressure (mb, sea level) = %.2f\n", pmb_sea );
	// os_printf ( "Pressure (mb*100, sea level) = %d\n", pmb_sea );

	os_printf ( "Temp = %d -- Pressure (mb*100, sea level) = %d\n", tf, pmb_sea );
}

#ifdef notdef
void
show_cals ( void *cals )
{
		short *calbuf = (short *) cals;
		unsigned short *calbuf_u = (unsigned short *) cals;

		os_printf ( "cal 1 = %d\n", calbuf[0] );
		os_printf ( "cal 2 = %d\n", calbuf[1] );
		os_printf ( "cal 3 = %d\n", calbuf[2] );
		os_printf ( "cal 4 = %d\n", calbuf_u[3] );
		os_printf ( "cal 5 = %d\n", calbuf_u[4] );
		os_printf ( "cal 6 = %d\n", calbuf_u[5] );
		os_printf ( "cal 7 = %d\n", calbuf[6] );
		os_printf ( "cal 8 = %d\n", calbuf[7] );
		os_printf ( "cal 9 = %d\n", calbuf[8] );
		os_printf ( "cal 10 = %d\n", calbuf[9] );
		os_printf ( "cal 11 = %d\n", calbuf[10] );
}
#endif

/* ---------------------------------------------- */
/* ---------------------------------------------- */

LOCAL os_timer_t read_timer;

LOCAL void ICACHE_FLASH_ATTR
read_cb ( void )
{
	static int first = 1;
	static int count = 0;
	int t, p;

	wdt_feed();
	// os_printf("Time=%ld\n", system_get_time());

	if ( first ) {
		int val = read_id ();
		os_printf ( "id = %02x\n", val );
		os_printf ( "OSS = %d\n", OSS );
		read_cals ( (unsigned short *) &bmp_cal );
		// show_cals ( (void *) &bmp_cal );
		first = 0;
	}

	t = read_temp ();
	p = read_pressure ();
	//os_printf ( "temp = %d\n", val );
	//os_printf ( "pressure = %d\n", val );
	//os_printf ( "raw T, P = %d  %d\n", t, p );

	convert ( t, p );

	/*
	if ( ++count > 10 ) {
		os_printf ( "Finished\n" );
		os_timer_disarm(&read_timer);
	}
	*/
}

/* Reading the sensor takes 4.5 + 7.5 milliseconds */
#define DELAY 1000 /* milliseconds */

void user_init(void)
{
	// Configure the UART
	// uart_init(BIT_RATE_9600,0);
	uart_div_modify(0, UART_CLK_FREQ / 115200);
	os_printf ( "\n" );

	i2c_master_gpio_init();
	os_printf ( "BMP180 address: %02x\n", BMP_ADDR );

	// Set up a timer to tick continually
    os_timer_disarm(&read_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&read_timer, read_cb, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&read_timer, DELAY, 1);
}

// vim: ts=4 sw=4 
