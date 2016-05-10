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

#define  REG_CALIBRATION 0xAA
#define  REG_CONTROL 0xF4
#define  REG_RESULT  0xF6
#define  REG_ID  0xD0

#define  CMD_TEMP 0x2E
#define  TDELAY 4500

/* This can have values 0, 1, 2, 3
 * representing internal sampling of 1, 2, 4, or 8 values
 * The 24 bit raw pressure value should be divided by:
 *  val = val / 2^(8-OSS)
 * This is a shift, as shown below
 */

#define  OSS_ULP	0
#define  CMD_P_ULP  0x34
#define  DELAY_ULP  4500
#define  SHIFT_ULP  8

#define  OSS_STD	1
#define  CMD_P_STD  0x74
#define  DELAY_STD  7500
#define  SHIFT_STD	7

#define  OSS_HR	2
#define  CMD_P_HR   0xB4
#define  DELAY_HR   13500
#define  SHIFT_HR	6

#define  OSS_UHR	3
#define  CMD_P_UHR  0xF4
#define  DELAY_UHR  25500
#define  SHIFT_UHR	5

/* This chooses from the above */
#define  OSS 		OSS_STD
#define  CMD_PRESS	CMD_P_STD
#define  PDELAY 	DELAY_STD
#define  PSHIFT 	SHIFT_STD

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

    i2c_master_wait ( TDELAY );

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

/* Note that we can read a third byte for pressure */

LOCAL int ICACHE_FLASH_ATTR
read_pressure ( void )
{
	int rv;

	i2c_master_start();
	if ( send_byte ( BMP_ADDR_W, "W address" ) ) return;
	if ( send_byte ( REG_CONTROL, "reg" ) ) return;
	if ( send_byte ( CMD_PRESS, "cmd" ) ) return;
	i2c_master_stop();

    i2c_master_wait ( PDELAY );

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

LOCAL os_timer_t read_timer;

LOCAL void ICACHE_FLASH_ATTR
read_cb ( void )
{
	static int first = 1;
	// static int count = 0;
	int t, p;

	wdt_feed();
	// os_printf("Time=%ld\n", system_get_time());

	if ( first ) {
		int val = read_id ();
		os_printf ( "id = %02x\n", val );
		first = 0;
	}

	t = read_temp ();
	p = read_pressure ();
	//os_printf ( "temp = %d\n", val );
	//os_printf ( "pressure = %d\n", val );
	os_printf ( "T, P = %d  %d\n", t, p );

#ifdef notdef
	if ( ++count > 10 ) {
		os_printf ( "Finished\n" );
		os_timer_disarm(&read_timer);
	}
#endif
}

/* Reading the sensor takes 4.5 + 7.5 milliseconds */
#define DELAY 2000 /* milliseconds */

void user_init(void)
{
	// Configure the UART
	// uart_init(BIT_RATE_9600,0);
	uart_div_modify(0, UART_CLK_FREQ / 115200);
	os_printf ( "\n" );

	i2c_master_gpio_init();
	os_printf ( "SDA/SCL = %d/%d\n",
		I2C_MASTER_SDA_GPIO, I2C_MASTER_SCL_GPIO );
	os_printf ( "BMP180 address: %02x\n", BMP_ADDR );

	// Set up a timer to tick continually
    os_timer_disarm(&read_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&read_timer, read_cb, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&read_timer, DELAY, 1);
}

// vim: ts=4 sw=4 
