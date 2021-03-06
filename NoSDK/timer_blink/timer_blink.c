/* ESP8266 "bare metal" interrupts.
 *
 * This uses timer interrupts to blink an LED
 *
 * For fun we do direct hardware access for the gpio
 * as well as the pin mux.
 *
 * It compiles to 576 bytes.
 *
 * Tom Trebisky  3-12-2016
 */

/* Look, no include files! */

/* The clock really is running at 52 Mhz when we
 * come out of the boot rom
 */
#ifdef notdef
/* Note that 80/1 = 80, 80/16 = 5, and 80/256 = 0.3125
 * So, in the case when the prescaler is set to divide by 16,
 *  we can just calculate the load value for the timer as
 *  load = P*5 where P is the desired timer period in microseconds.
 * Note that this overflows when a period longer
 *  than 858 seconds is requested.
 * We set "TIMER_TICKER" to this value times 100 to allow
 *  for the 52 Mhz case below (this limits us to 8.58 second periods)
 */
#define  CLK_FREQ   80*1000000
#define TIMER_TICKER	500
#endif

/* Note that 52/1 = 52, 52/16 = 3.25, and 52/256 = 0.203125
 * So, in the case when the prescaler is set to divide by 16,
 * if we are given a clock period in microsecons (call it P)
 * We can calculate the load value for the timer as
 * load = P*325/100
 * This calculation will overflow a 32 bit unsigned when
 *  P*325 = 2^32, i.e. when P = 13,215,284, i.e.
 *  when a period of over 13 seconds is requested.
 */
#define  CLK_FREQ       (52*1000000)
#define TIMER_TICKER	325

struct timer {
	volatile unsigned int	load;
	volatile unsigned int	count;
	volatile unsigned int	ctrl;
	volatile unsigned int	intack;
};

#define TIMER1_BASE (struct timer *) 0x60000600;
#define TIMER2_BASE (struct timer *) 0x60000620;

#define TIMER_BASE 	TIMER1_BASE

#define TIMER_INUM 9

/* bits in the timer control register */
#define	TC_ENABLE	0x80
#define	TC_AUTO_LOAD	0x40
#define	TC_DIV_1	0x00
#define	TC_DIV_16	0x04
#define	TC_DIV_256	0x08
#define TC_LEVEL	0x01
#define TC_EDGE		0x00

/* It looks like we have 2 bits assigned to a prescaler.
 *  probably 0x0c -- setting both bits to one sill gives
 *  a divide by 256, so my experiments have found no hidden values.
 *
 * note that 80,000,000 / 16 = 5,000,000
 *      also 52,000,000 / 16 = 3,250,000
 */

struct dport {
	volatile unsigned int	_unk1;
	volatile unsigned int	edge;
};

#define DPORT_BASE (struct dport *) 0x3ff00000;

/* --------------------------------- */

struct gpio {
	volatile unsigned int	out;		/* 00 */
	volatile unsigned int	out_set;	/* 04 */
	volatile unsigned int	out_clear;	/* 08 */
	volatile unsigned int	enable;		/* 0c */
	volatile unsigned int	ena_set;	/* 10 */
	volatile unsigned int	ena_clear;	/* 14 */
	volatile unsigned int	in;		/* 18 */
	volatile unsigned int	status;		/* 1c */
	volatile unsigned int	status_set;	/* 20 */
	volatile unsigned int	status_clear;	/* 24 */
};

#define GPIO_BASE (struct gpio *) 0x60000300;

struct mux {
	volatile unsigned int	conf;		/* 00 */
	volatile unsigned int	gpio12;		/* 04 */
	volatile unsigned int	gpio13;		/* 08 */
	volatile unsigned int	gpio14;		/* 0C */
	volatile unsigned int	gpio15;		/* 10 */
	volatile unsigned int	gpio3;		/* 14 */
	volatile unsigned int	txd;		/* 18 */
	volatile unsigned int	sd_clk;		/* 1C */
	volatile unsigned int	sd_d0;		/* 20 */
	volatile unsigned int	sd_d1;		/* 24 */
	volatile unsigned int	sd_d2;		/* 28 */
	volatile unsigned int	sd_d3;		/* 2C */
	volatile unsigned int	sd_cmd;		/* 30 */
	volatile unsigned int	gpio0;		/* 34 */
	volatile unsigned int	gpio2;		/* 38 */
	volatile unsigned int	gpio4;		/* 3c */
	volatile unsigned int	gpio5;		/* 40 */
};

/* The function mask is shifted over 4 bits and has a 2 bit "hole"
 * in the middle (those bits are used to pullup control).
 */
#define FUNC_MASK	0x130

/* I would rather not use this, but just generate constants with the
 * necessary hole.
 */
#define funk(x)		(((x&4)<<2|(x&0x3))<<4)

/* set to enable pullup */
#define PULLUP_MASK	0x080

#define MUX_BASE (struct mux *) 0x60000800;


/* ---------- */

#define LED_BIT		2	/* LED is on GPIO 2 */
#define LED_MASK	0x04	/* 1 << 2 */

/* --------------------------------- */

/* Initializing this to zero here does not work.
 * My code would have to include some code to clear
 * the bss area.  Setting a non-zero initial value
 * works just fine.  No big surprise here.
 */
int state = 1;	/* 1 is high is off */

/* Here we do gpio output "the right way".
 * None of this nonsense of setting all the enable and
 * disable registers every time we do anything.
 */
void
timer_isr ( char *msg )
{
	struct gpio *gp = GPIO_BASE;

	if ( state )
	    gp->out_clear = LED_MASK;
	else
	    gp->out_set = LED_MASK;

	state = 1 - state;
}

/* Call with rate in microseconds */
void
timer_setup ( int rate )
{
	struct timer *tp = TIMER_BASE;
	struct dport *dp = DPORT_BASE;

	tp->ctrl = TC_ENABLE | TC_AUTO_LOAD | TC_DIV_16 | TC_EDGE;
	ets_isr_attach ( TIMER_INUM, timer_isr, 0 );

	dp->edge |= 0x02;
	ets_isr_unmask ( 1 << TIMER_INUM );

	tp->load = (rate * TIMER_TICKER) / 100;

#ifdef notdef
	/* Here is the odd logic used in the SDK examples.
	 * IT avoids overflow with a 32 bit unsigned int.
	 * The magic number 858 is based on a 80 Mhz clock.
	 */
	if ( rate <= 858 )
	    tp->load = (rate * (CLK_FREQ>>4)) / 1000000;
	else
	    tp->load = (rate>>2) * ((CLK_FREQ>>4)/250000) + (rate&0x3) * ((CLK_FREQ>>4)/1000000);
#endif
}

/* clearing the LED bit will turn the LED on,
 * so we set it to turn the LED off.
 */
void
led_setup ( void )
{
	struct gpio *gp = GPIO_BASE;
	struct mux *mp = MUX_BASE;

	// Just clear func bits to set zero
	mp->gpio2 &= ~FUNC_MASK;

	// MODE is 0, so this does nothing.
	// mp->gpio2 |= funk(MODE);

	gp->ena_set = LED_MASK;

	gp->out_set = LED_MASK;
}

/* 1 second In microseconds */
// #define TIMER_RATE	1000000

/* half second */
#define TIMER_RATE	500000

void
call_user_start ( void )
{
    uart_div_modify(0, CLK_FREQ / 115200);
    ets_delay_us ( 1000 * 500 );

    led_setup ();
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
