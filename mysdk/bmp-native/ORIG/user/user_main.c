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
/*
	Print some system info
*/
#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include "driver/uart.h"
#include "driver/i2c_master.h"

#define DELAY 1000 /* milliseconds */

#define BH1750_ADDR 0x23 /* I2C address of sensor */

extern void wdt_feed(void);

LOCAL os_timer_t read_timer;

LOCAL void ICACHE_FLASH_ATTR
read_cb(void)
{
	uint8_t ack, low, high;
	wdt_feed();
	os_printf("Time=%ld\n", system_get_time());
	i2c_master_start();
	i2c_master_writeByte(BH1750_ADDR);
	ack = i2c_master_getAck();
	if (ack)
	{
		os_printf("I2C:No ack after sending address\n");
		i2c_master_stop();
		return;
	}
	i2c_master_stop();
    i2c_master_wait(40000); // why?

    i2c_master_start();
    i2c_master_writeByte(BH1750_ADDR + 1);
    ack = i2c_master_getAck();
	if (ack)
	{
		os_printf("I2C:No ack after write command\n");
		i2c_master_stop();
		return;
	}
	low = i2c_master_readByte();
	high = i2c_master_readByte();
	i2c_master_setAck(1);
    i2c_master_stop();
	os_printf("I2C:read(L/H)=(0x%02x/0x%02x)\n", low, high);
}

/*
 * This is entry point for user code
 */
void user_init(void)
{
	// Configure the UART
	uart_init(BIT_RATE_9600,0);

	i2c_master_gpio_init();

	// Set up a timer to send the message
	// os_timer_disarm(ETSTimer *ptimer)
    os_timer_disarm(&read_timer);
	// os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
	os_timer_setfn(&read_timer, read_cb, (void *)0);
	// void os_timer_arm(ETSTimer *ptimer,uint32_t milliseconds, bool repeat_flag)
	os_timer_arm(&read_timer, DELAY, 1);
}

// vim: ts=4 sw=4 
