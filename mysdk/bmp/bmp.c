/*
The MIT License (MIT)

Copyright (c) 2016 Tom Trebisky  tom@mmto.org

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

// #include "iic.h"

#define  BMP_ADDR 0x77

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

/* These will move to iic library */

#define IIC_WADDR(a)	(a << 1)
#define IIC_RADDR(a)	((a << 1) | 1)

/* raw write an array of bytes (8 bit objects)
 * for a device without registers (like the MCP4725)
 */
static int ICACHE_FLASH_ATTR
iic_write_raw ( int addr, unsigned char *buf, int n )
{
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	for ( i = 0; i < n; i++ ) {
		if ( iic_send_byte_m ( buf[i], "reg" ) ) return 1;
	}
	iic_stop();

	return 0;
}

/* raw read an array of bytes (8 bit objects)
 * for a device without registers (like the MCP4725)
 */
static int ICACHE_FLASH_ATTR
iic_read_raw ( int addr, unsigned char *buf, int n )
{
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return 1;

	for ( i=0; i < n; i++ ) {
		*buf++ = iic_recv_byte ( i == n - 1 ? 1 : 0 );
	}

	iic_stop();

	return 0;
}

/* raw read an array of shorts (16 bit objects)
 */
static int ICACHE_FLASH_ATTR
iic_read_16raw ( int addr, unsigned short *buf, int n )
{
	unsigned int val;
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return 1;

	for ( i=0; i < n; i++ ) {
		val =  iic_recv_byte ( 0 ) << 8;
		val |= iic_recv_byte ( i == n - 1 ? 1 : 0 );
		*buf++ = val;
	}

	iic_stop();

	return 0;
}

/* 8 bit read */
static int ICACHE_FLASH_ATTR
iic_read ( int addr, int reg )
{
	int rv;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return;
	if ( iic_send_byte_m ( reg, "reg" ) ) return;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return;
	rv = iic_recv_byte ( 1 );
	iic_stop();

	return rv;
}

/* read a 2 byte (short) object from
 * two consecutive i2c registers.
 * (or in some devices, a single 16 bit register)
 */
static int ICACHE_FLASH_ATTR
iic_read_16 ( int addr, int reg )
{
	int rv;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return;
	if ( iic_send_byte_m ( reg, "reg" ) ) return;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return;
	rv =  iic_recv_byte ( 0 ) << 8;
	rv |= iic_recv_byte ( 1 );
	iic_stop();

	return rv;
}


/* read an array of bytes (8 bit objects) from
 * consecutive i2c registers.
 */
static int ICACHE_FLASH_ATTR
iic_read_n ( int addr, int reg, unsigned char *buf, int n )
{
	unsigned int val;
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return 1;
	for ( i=0; i < n; i++ ) {
		*buf++ = iic_recv_byte ( i == n - 1 ? 1 : 0 );
	}
	iic_stop();

	return 0;
}

/* read an array of 2 byte (short) objects from
 * consecutive i2c registers.
 *  - note that i2c devices just read out consecutive registers
 *  until you send a nack
 */
static int ICACHE_FLASH_ATTR
iic_read_16n ( int addr, int reg, unsigned short *buf, int n )
{
	int val;
	int i;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return 1;
	for ( i=0; i < n; i++ ) {
		val =  iic_recv_byte ( 0 ) << 8;
		val |= iic_recv_byte ( i == n - 1 ? 1 : 0 );
		*buf++ = val;
	}
	iic_stop();

	return 0;
}

/* read a 2 byte (short) object from
 * two consecutive i2c registers.
 */
static int ICACHE_FLASH_ATTR
iic_read_24 ( int addr, int reg )
{
	int rv;

	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return;
	if ( iic_send_byte_m ( reg, "reg" ) ) return;
	iic_stop();

	iic_start();
	if ( iic_send_byte_m ( IIC_RADDR(addr), "R address" ) ) return;
	rv =  iic_recv_byte ( 0 ) << 16;
	rv |=  iic_recv_byte ( 0 ) << 8;
	rv |= iic_recv_byte ( 1 );
	iic_stop();

	return rv;
}

static int ICACHE_FLASH_ATTR
iic_write ( int addr, int reg, int val )
{
	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	if ( iic_send_byte_m ( val, "val" ) ) return 1;
	iic_stop();

	return 0;
}

static int ICACHE_FLASH_ATTR
iic_write16 ( int addr, int reg, int val )
{
	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	if ( iic_send_byte_m ( val >> 8, "val_h" ) ) return 1;
	if ( iic_send_byte_m ( val & 0xff, "val_l" ) ) return 1;
	iic_stop();

	return 0;
}

/* write a register address with no data
 * (this could be a call to write_raw with a
 *   count of zero)
 */
static int ICACHE_FLASH_ATTR
iic_write_nada ( int addr, int reg )
{
	iic_start();
	if ( iic_send_byte_m ( IIC_WADDR(addr), "W address" ) ) return 1;
	if ( iic_send_byte_m ( reg, "reg" ) ) return 1;
	iic_stop();

	return 0;
}

/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* BMP180 specific routines --- */

/* ID register -- should always yield 0x55
 */
static int ICACHE_FLASH_ATTR
read_id ( void )
{
	int id;

	id = iic_read ( BMP_ADDR, REG_ID );

	return id;
}

static int ICACHE_FLASH_ATTR
read_temp ( void )
{
	int rv;

	(void) iic_write ( BMP_ADDR, REG_CONTROL, CMD_TEMP );

    os_delay_us ( TDELAY );

	rv = iic_read_16 ( BMP_ADDR, REG_RESULT );

	return rv;
}

/* Pressure reads 3 bytes
 */

static int ICACHE_FLASH_ATTR
read_pressure ( void )
{
	int rv;

	(void) iic_write ( BMP_ADDR, REG_CONTROL, CMD_PRESS );

    os_delay_us ( PDELAY );

	rv = iic_read_24 ( BMP_ADDR, REG_RESULT );

	return rv >> PSHIFT;
}

static void ICACHE_FLASH_ATTR
read_cals ( unsigned short *buf )
{
	/* Note that on the ESP8266 just placing the bytes into
	 * memory in the order read out does not yield an array of shorts.
	 * This is because the BMP180 gives the MSB first, but the
	 * ESP8266 is little endian.  So we use our routine that gives
	 * us an array of shorts and we are happy.
	 */
	// iic_read_n ( BMP_ADDR, REG_CALS, (unsigned char *) buf, 2*NCALS );
	iic_read_16n ( BMP_ADDR, REG_CALS, buf, NCALS );
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

/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* MCP23008 driver follows
 * The MCP23017 expands from 8 to 16 bits
 * each register has the "extension" at addr + 0x10
 * (so MCP_DIR is at 0x00 and 0x10)
 */

#define MCP_ADDR	0x20

#define MCP_DIR		0x00
#define MCP_GPIO	0x09
#define MCP_OLAT	0x0a

static int ICACHE_FLASH_ATTR
read_gpio ( void )
{
	int val;

	val = iic_read ( MCP_ADDR, MCP_GPIO );

	return val;
}

/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* MCP4725 driver follows
 * This device is peculiar in that it does not have registers
 * You either read or write.
 * read always returns 3 bytes.
 * write has 4 flavors.
 */
#define DAC_ADDR	0x60

static void ICACHE_FLASH_ATTR
dac_write ( unsigned int val )
{
	unsigned char iobuf[2];

	iobuf[0] = (val >> 8) & 0xf;
	iobuf[1] = val & 0xff;
	iic_write_raw ( DAC_ADDR, iobuf, 2 );
}

static void ICACHE_FLASH_ATTR
dac_write_ee ( unsigned int val )
{
	unsigned char iobuf[3];

	iobuf[0] = 0x60;
	iobuf[1] = val >> 4;
	// iobuf[2] = (val & 0xf) << 4;
	iobuf[2] = val << 4;
	iic_write_raw ( DAC_ADDR, iobuf, 3 );
}

/* returns up to 5 bytes.
 * "status" byte
 * DAC value (2 bytes)
 * EEPROM value (2 bytes)
 */
static void ICACHE_FLASH_ATTR
dac_read ( unsigned char *buf, int n )
{
	iic_read_raw ( DAC_ADDR, buf, n );
}

static unsigned int ICACHE_FLASH_ATTR
dac_read_val ( void )
{
	unsigned char iobuf[3];

	iic_read_raw ( DAC_ADDR, iobuf, 3 );
	return (iobuf[1] << 4) | (iobuf[2] >> 4);
}
/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* HDC1008 driver
 */
#define HDC_ADDR	0x40

#define HDC_TEMP	0x00
#define HDC_HUM		0x01
#define HDC_CON		0x02
#define HDC_SN1		0xFB
#define HDC_SN2		0xFC
#define HDC_SN3		0xFD

/* bits/fields in config register */
#define HDC_RESET	0x8000
#define HDC_HEAT	0x2000
#define HDC_BOTH	0x1000
#define HDC_BSTAT	0x0800

#define HDC_TRES14	0x0000
#define HDC_TRES11	0x0400

#define HDC_HRES14	0x0000
#define HDC_HRES11	0x0100
#define HDC_HRES8	0x0200

/* Delays in microseconds */
#define CONV_8		2500
#define CONV_11		3650
#define CONV_14		6350

#define CONV_BOTH	12700

/* Read gets both T then H */
static void ICACHE_FLASH_ATTR
hdc_con_both ( void )
{
	(void) iic_write16 ( HDC_ADDR, HDC_CON, HDC_BOTH );
}

/* Read gets either T or H */
static void ICACHE_FLASH_ATTR
hdc_con_single ( void )
{
	(void) iic_write16 ( HDC_ADDR, HDC_CON, 0 );
}

static void ICACHE_FLASH_ATTR
hdc_read_both ( unsigned short *buf )
{
	iic_write_nada ( HDC_ADDR, HDC_TEMP );
	os_delay_us ( CONV_BOTH );

	iic_read_16raw ( HDC_ADDR, buf, 2 );
}

/* ---------------------------------------------- */
/* ---------------------------------------------- */

static void ICACHE_FLASH_ATTR
upd_mcp ( void )
{
	int val;
	static int phase = 0;

	// val = read_gpio ();
	// os_printf ( "MCP gpio = %02x\n", val );

	if ( phase ) {
		iic_write ( MCP_ADDR, MCP_OLAT, 0 );
		phase = 0;
	} else {
		iic_write ( MCP_ADDR, MCP_OLAT, 0xff );
		phase = 1;
	}

}

static void ICACHE_FLASH_ATTR
read_bmp ( void )
{
	int t, p;

	t = read_temp ();
	p = read_pressure ();
	//os_printf ( "temp = %d\n", val );
	//os_printf ( "pressure = %d\n", val );
	//os_printf ( "raw T, P = %d  %d\n", t, p );

	convert ( t, p );
}

static void ICACHE_FLASH_ATTR
dac_show ( void )
{
	unsigned char io[5];

	dac_read ( io, 5 );

	os_printf ( "DAC status = %02x\n", io[0] );
	os_printf ( "DAC val = %02x %02x\n", io[1], io[2] );
	os_printf ( "DAC ee = %02x %02x\n", io[3], io[4] );
}

static unsigned int dac_val = 0;

static void ICACHE_FLASH_ATTR
dac_doodle ( void )
{
	dac_val += 16;
	if ( dac_val > 4096 ) dac_val = 0;
	dac_write ( dac_val);
}

static void ICACHE_FLASH_ATTR
hdc_test ( void )
{
	int val;

	/* This does not work.
	 * device does not autoincrement address
	 */
	// unsigned short iobuf[3];
	// (void) iic_read_16n ( HDC_ADDR, HDC_SN1, iobuf, 3 );

	val = iic_read_16 ( HDC_ADDR, HDC_SN1 );
	os_printf ( "HDC sn = %04x\n", val );
	val = iic_read_16 ( HDC_ADDR, HDC_SN2 );
	os_printf ( "HDC sn = %04x\n", val );
	val = iic_read_16 ( HDC_ADDR, HDC_SN3 );
	os_printf ( "HDC sn = %04x\n", val );

	val = iic_read_16 ( HDC_ADDR, HDC_CON );
	os_printf ( "HDC con = %04x\n", val );

	hdc_con_both ();
}

static void ICACHE_FLASH_ATTR
hdc_read ( void )
{
	unsigned short iobuf[2];
	int t, h;
	int tf;

	hdc_read_both ( iobuf );

	/*
	t = 99;
	h = 23;
	tf = 0;
	os_printf ( " Bogus t, tf, h = %d  %d %d\n", t, tf, h );
	*/
	// os_printf ( " HDC raw t,h = %04x  %04x\n", iobuf[0], iobuf[1] );

	t = ((iobuf[0] * 165) / 65536)- 40;
	tf = t * 18 / 10 + 32;
	os_printf ( " -- traw, t, tf = %04x %d   %d %d\n", iobuf[0], iobuf[0], t, tf );

	//h = ((iobuf[1] * 100) / 65536);
	//os_printf ( "HDC t, tf, h = %d  %d %d\n", t, tf, h );

	h = ((iobuf[1] * 100) / 65536);
	os_printf ( " -- hraw, h = %04x %d    %d\n", iobuf[1], iobuf[1], h );
}

/* ---------------------------------------- */

static os_timer_t timer;

static void ICACHE_FLASH_ATTR
ticker ( void *arg )
{
	static int first = 1;
	static int count = 0;

	if ( first ) {
		int val = read_id ();
		os_printf ( "BMP180 id = %02x\n", val );

		read_cals ( (unsigned short *) &bmp_cal );
		// show_cals ( (void *) &bmp_cal );

		// iic_write ( MCP_ADDR, MCP_DIR, 0 );		/* outputs */

		// dac_show ();

		hdc_test ();

		first = 0;
	}

	// read_bmp ();
	// upd_mcp ();
	// dac_doodle ();
	hdc_read ();

	/*
	if ( ++count > 10 ) {
		os_printf ( "Finished\n" );
		os_timer_disarm(&timer);
	}
	*/
}

#define IIC_SDA		4
#define IIC_SCL		5

/* Reading the BMP180 sensor takes 4.5 + 7.5 milliseconds */
#define DELAY 1000 /* milliseconds */
// #define DELAY 2 /* 2 milliseconds - for DAC test */

void user_init(void)
{
	// Configure the UART
	// uart_init(BIT_RATE_9600,0);
	uart_div_modify(0, UART_CLK_FREQ / 115200);
	os_printf ( "\n" );

	iic_init ( IIC_SDA, IIC_SCL );

	// os_printf ( "BMP180 address: %02x\n", BMP_ADDR );
	// os_printf ( "OSS = %d\n", OSS );

	// Set up a timer to tick continually
    os_timer_disarm(&timer);
	os_timer_setfn(&timer, ticker, (void *)0);
	os_timer_arm(&timer, DELAY, 1);
}

// vim: ts=4 sw=4 
