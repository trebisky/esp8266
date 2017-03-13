/* ESP8266 "bare metal" interrupts.
 *
 * This uses timer interrupts to print messages every second.
 *
 * It compiles to 528 bytes.
 *
 * Tom Trebisky  3-12-2016
 */

/* Look, no include files! */

/* The clock really is running at 52 Mhz when we
 * come out of the boot rom
 */
// #define  CLK_FREQ   80*1000000
#define  CLK_FREQ       (52*1000000)

struct timer {
	volatile unsigned int	load;
	volatile unsigned int	count;
	volatile unsigned int	ctrl;
	volatile unsigned int	intack;
};

#define TIMER_BASE (struct timer *) 0x60000600;
#define TIMER_INUM 9

/* bits in the timer control register */
#define	TC_ENABLE	0x80
#define	TC_AUTO_LOAD	0x40
#define	TC_DIV_1	0x00
#define	TC_DIV_16	0x04
#define	TC_DIV_256	0x08
#define TC_LEVEL	0x01
#define TC_EDGE		0x00

/* It looks like we have 3 bits assigned to a prescaler.
 * My guess is that we get choices of 1, 4, 16, 64, 256.
 * maybe 1024, 4096 ... ?
 * Experimenting would be necessary.
 * note that 80,000,000 / 16 = 5,000,000
 *      also 52,000,000 / 16 = 3,250,000
 */

struct dport {
	volatile unsigned int	_unk1;
	volatile unsigned int	edge;
};

#define DPORT_BASE (struct dport *) 0x3ff00000;

/* --------------------------------- */

/* Initializing this to zero here does not work.
 * My code would have to include some code to clear
 * the bss area.  Setting a non-zero initial value
 * works just fine.  No big surprise here.
 */
int count = 13;

void
timer_isr ( char *msg )
{
	ets_printf ( "Tick %d: %s\n", count++, msg );
}

/* Just something to test passing a pointer to the isr attach routine */
char *bird = "duck";

/* Call with rate in microseconds */
void
timer_setup ( int rate )
{
	struct timer *tp = TIMER_BASE;
	struct dport *dp = DPORT_BASE;

	tp->ctrl = TC_ENABLE | TC_AUTO_LOAD | TC_DIV_16 | TC_EDGE;
	ets_isr_attach ( TIMER_INUM, timer_isr, bird );

	dp->edge |= 0x02;
	ets_isr_unmask ( 1 << TIMER_INUM );

	/* This avoids overflow with a 32 bit unsigned int.
	 * The magic number 858 is based on a 80 Mhz clock.
	 */
	if ( rate <= 858 )
	    tp->load = (rate * (CLK_FREQ>>4)) / 1000000;
	else
	    tp->load = (rate>>2) * ((CLK_FREQ>>4)/250000) + (rate&0x3) * ((CLK_FREQ>>4)/1000000);

	/* note in the above, if we ask for a 1 second delay (1000000 us),
	 * we set the load register to CLK_FREQ/16
	 * So with the prescaler set to /16, the timer counts at CPU_HZ / 16.
	 */
}

/* In microseconds */
#define TIMER_RATE	1000000

void
call_user_start ( void )
{
    uart_div_modify(0, CLK_FREQ / 115200);
    ets_delay_us ( 1000 * 500 );

    timer_setup ( TIMER_RATE );

    ets_printf("\n");
    ets_printf("Starting\n");

#ifdef notdef
    /* We can spin here, avoiding returning to the boot rom,
     *  but it works just fine to simply return.
     * We get the message "user code done" and the rom is
     *  conveniently waiting for a download while interrupts
     *  are happily ticking away.
     */
     for ( ;; )
	;
#endif
}

/* THE END */
