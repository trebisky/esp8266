/* ESP8266 "bare metal" IO
 *
 * This uses timer interrupts to generate pules on
 *  the unique and special "RTC" gpio pin.
 *  (otherwise known as GPIO16 to most folks).
 *
 * We also experiment here to see how fast we can handle
 *  timer interrupts.  The basic conclusion is that we
 *  can do 200 khz (with a trivial ISR), asking the
 *  processor to handle interrupts at 400 khz is right
 *  on the hairy edge, at least using the somewhat
 *  complex interrupt registration scheme provided by
 *  the bootrom routine:  ets_isr_attach()
 *
 * See: sdk/include/eagle_soc.h
 * Also: sdk/examples/driver_lib/driver/gpio16.c
 *
 * This compiles to 784 bytes.
 *
 * Tom Trebisky  3-14-2016
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

/* ----------------------------------------- */

#define RTC_BASE 0x60000700

struct rtc_gpio {
	volatile unsigned int	out;		/* 68 */
	int		_pad0[2];
	volatile unsigned int	enable;		/* 74 */
	int		_pad1[5];
	volatile unsigned int	in;		/* 8C */
	volatile unsigned int	conf;		/* 90 */
	volatile unsigned int	config[6];	/* 94 */
};

#define	dcdc_conf	config[3]		/* A0 */

#define RTC_GPIO_BASE (struct rtc_gpio *) (RTC_BASE + 0x68 )

/* ----------------------------------------- */

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

/* Here is a short library of code for fiddling with
 * the rtc GPIO.
 * These are proof of concept "demo" routines that might
 *  better be handled via compiler inline code.
 */

inline void
rtc_gpio_set ( void )
{
	struct rtc_gpio *rg = RTC_GPIO_BASE;

	rg->out |= 1;
}

inline void
rtc_gpio_clear ( void )
{
	struct rtc_gpio *rg = RTC_GPIO_BASE;

	rg->out &= ~1;
}

inline int
rtc_gpio_read ( void )
{
	struct rtc_gpio *rg = RTC_GPIO_BASE;

	return rg->in & 1;
}

void
rtc_gpio_output ( void )
{
	struct rtc_gpio *rg = RTC_GPIO_BASE;
	int mux;

	/* set mux for rtc_gpio0 */
	mux = rg->dcdc_conf;
	rg->dcdc_conf = (mux & 0xffffffbc) | 1;
	rg->conf &= ~1;

	/* enable output */
	rg->enable |= 1;
}

void
rtc_gpio_input ( void )
{
	struct rtc_gpio *rg = RTC_GPIO_BASE;
	int mux;

	/* set mux for rtc_gpio0 */
	mux = rg->dcdc_conf;
	rg->dcdc_conf = (mux & 0xffffffbc) | 1;
	rg->conf &= ~1;

	/* disable output */
	rg->enable &= ~1;
}

/* --------------------------------- */

/* Initializing this to zero here does not work.
 * My code would have to include some code to clear
 * the bss area.  Setting a non-zero initial value
 * works just fine.  No big surprise here.
 */
int state = 1;	/* 1 is high is off */

/* Here we do gpio output "the right way".
 * No calling tiny routines, although the compiler
 * would probably optimize the inline code just fine.
 */
void
timer_isr ( char *msg )
{
	struct rtc_gpio *rg = RTC_GPIO_BASE;

	if ( state )
	    rg->out &= ~1;
	else
	    rg->out |= 1;

	state = 1 - state;
}

/* Call with rate in units of CLK.
 *  (we set to prescaler to divide by 1).
 * We are clocking the timer with the full 52 Mhz,
 *  so if we want a 10 us period,
 *   call this with 10 * CLK = 520 for a 52 Mhz clock.
 */
void
timer_load ( int val )
{
	struct timer *tp = TIMER_BASE;
	struct dport *dp = DPORT_BASE;

	tp->ctrl = TC_ENABLE | TC_AUTO_LOAD | TC_DIV_1 | TC_EDGE;
	ets_isr_attach ( TIMER_INUM, timer_isr, 0 );

	dp->edge |= 0x02;
	ets_isr_unmask ( 1 << TIMER_INUM );

	tp->load = val;
}

/* Call with rate in units of CLK/16
 *  i.e. with a 52 Mhz clock we are clocking the
 *   timer at 3.25 Mhz
 */
void
timer_load_16 ( int val )
{
	struct timer *tp = TIMER_BASE;
	struct dport *dp = DPORT_BASE;

	tp->ctrl = TC_ENABLE | TC_AUTO_LOAD | TC_DIV_16 | TC_EDGE;
	ets_isr_attach ( TIMER_INUM, timer_isr, 0 );

	dp->edge |= 0x02;
	ets_isr_unmask ( 1 << TIMER_INUM );

	tp->load = val;
}

/* Call with rate in microseconds */
void
timer_setup ( int rate )
{
	timer_load_16 ( (rate * TIMER_TICKER) / 100 );
}

/* 100 microseconds is the fastest we can get exact rates
 * the way timer_setup() is currently coded.
 * This should give a 5 kHz waveform. (And it does!)
 */
#define TIMER_RATE	100

void
call_user_start ( void )
{
    uart_div_modify(0, CLK_FREQ / 115200);
    ets_delay_us ( 1000 * 500 );

    rtc_gpio_output ();
    // timer_setup ( TIMER_RATE );	/* see 5 nice khz */
    timer_load ( 260 );		/* see nice 100 khz */

    /* This interrupts at 400 khz (every 2.5 microseconds).
     * It almost works.
     */
    // timer_load ( 130 );	/* see 200 khz, with rare jitter */

    /* For the following we see a nice stable 500 khz.
     * It is not surprising we cannot get the expected 1 Mhz
     *  since we are interrupting the processor at 2 Mhz.
     * What is surprising is that we get a nice clean 500 khz,
     *  which I view as some kind of accident.
     * Even with the CPU running at 80 Mhz, this gives only
     *  40 cycles to get in and out of the ISR.
     */
    // timer_load ( 26 );

    ets_printf("\n");
    ets_printf("Starting\n");

#ifdef notdef
    /* We can spin here, avoiding returning to the boot rom,
     *  but it works just fine to simply return.
     * We get the message "user code done" and the rom is
     *  conveniently waiting for a download while interrupts
     *  are happily ticking away.  This is actually wonderful
     *  and ideal for this kind of testing.
     */
     for ( ;; )
	;
#endif
}

/* THE END */
