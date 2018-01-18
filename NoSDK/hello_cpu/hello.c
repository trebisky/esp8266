/* ESP8266
 *
 * This is almost the simplest possible
 * program we can run without the SDK.
 *
 * It compiles to 240 bytes, all of which goes
 * into the first flash segments.
 * About 18 assembly language statements.
 *
 * All of the routines called are in the boot rom.
 *
 * Also since it exits, it drops back into the
 * boot rom which is then ready to run new code.
 *
 * Tom Trebisky  3-9-2016  original code
 * Tom Trebisky  1-17-2018 add cpu clock fiddling
 */

/* Look, no include files! */

/* The clock really is running at 52 Mhz when we
 * come out of the boot rom
 */
#define UART_CLK_FREQ2	(80*1000000)
#define UART_CLK_FREQ	(52*1000000)

void
set_cpu_clock ( void )
{
    rom_i2c_writeReg(103,4,1,136);
    rom_i2c_writeReg(103,4,2,145);

#ifdef notdef
    if(rom_i2c_readReg(103,4,1) != 136) { // 8: 40MHz, 136: 26MHz
        //if(get_sys_const(sys_const_crystal_26m_en) == 1) { // soc_param0: 0: 40MHz, 1: 26MHz, 2: 24MHz
            // set 80MHz PLL CPU
            rom_i2c_writeReg(103,4,1,136);
            rom_i2c_writeReg(103,4,2,145);
        //}
    }
#endif
}

void
call_user_start ( void )
{
    // uart_div_modify(0, UART_CLK_FREQ / 115200);

    set_cpu_clock ();
    uart_div_modify(0, UART_CLK_FREQ2 / 115200);

    ets_delay_us ( 1000 * 500 );

    ets_printf("\n");
    ets_printf("Eat donuts faster!\n");
}

/* THE END */
