/* Use a hardware timer on the ESP8266
 * to get an LED blinking
 *
 * Tom Trebisky  2-18-2017
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"

// static os_timer_t timer1;

/* BIT2 blinks the little LED on my NodeMCU board
 * (which is labelled "D4" on the silkscreen)
 * This is the LED on the ESP-12E board.
 * This code also works with the plain ESP-12 board I have.
 */
#define LED_BIT		BIT2
#define FUNC_MUX	PERIPHS_IO_MUX_GPIO2_U
#define FUNC_F		FUNC_GPIO2

//TIMER PREDIVED MODE
typedef enum {
    DIVDED_BY_1 = 0,            //timer clock
    DIVDED_BY_16 = 4,   //divided by 16
    DIVDED_BY_256 = 8,  //divided by 256
} TIMER_PREDIVED_MODE;

typedef enum {                  //timer interrupt mode
    TM_LEVEL_INT = 1,   // level interrupt
    TM_EDGE_INT   = 0,  //edge interrupt
} TIMER_INT_MODE;

void
hw_timer_isr ( void )
{
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & LED_BIT) {
	gpio_output_set(0, LED_BIT, LED_BIT, 0);
	os_printf("set high\n");
    } else {
	gpio_output_set(LED_BIT, 0, LED_BIT, 0);
	os_printf("set low\n");
    }
}

/* After reading in the API guide for the SDK,
 * one would think that the 3 calls would be
 * provided in some library, but they are not.
 * A look at the recommended example shows
 * that these routines are provided as source in
 * the example itself:
 *  sdk/examples/driver_lib/driver/hw_timer.c
 * I began with that code and worked up the code
 *  below.
 */

#ifdef notdef
static void
hw_timer_setup_sdk ( void )
{
    hw_timer_init ( FRC1_SOURCE, 1 );
    hw_timer_set_func ( timer_func1 );
    hw_timer_arm ( 500 * 1000 );
}
#endif

#define US_TO_RTC_TIMER_TICKS(t)          \
    ((t) ?                                   \
     (((t) > 0x35A) ?                   \
      (((t)>>2) * ((APB_CLK_FREQ>>4)/250000) + ((t)&0x3) * ((APB_CLK_FREQ>>4)/1000000))  :    \
      (((t) *(APB_CLK_FREQ>>4)) / 1000000)) :    \
     0)

#define FRC1_ENABLE_TIMER  BIT7
#define FRC1_AUTO_LOAD  BIT6

/* Argument is interval in microseconds !! */
static void
hw_timer_setup ( unsigned int val )
{
    /* hw_timer_init */
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS, FRC1_AUTO_LOAD | DIVDED_BY_16 | FRC1_ENABLE_TIMER | TM_EDGE_INT);
    // RTC_REG_WRITE(FRC1_CTRL_ADDRESS, DIVDED_BY_16 | FRC1_ENABLE_TIMER | TM_EDGE_INT);

    // ETS_FRC_TIMER1_NMI_INTR_ATTACH(hw_timer_isr);
    ETS_FRC_TIMER1_INTR_ATTACH(hw_timer_isr, NULL);

    TM1_EDGE_INT_ENABLE();
    ETS_FRC1_INTR_ENABLE();

    /* hw_timer_arm */
    RTC_REG_WRITE(FRC1_LOAD_ADDRESS, US_TO_RTC_TIMER_TICKS(val));
}

void user_init()
{

    hw_timer_setup ( 500 * 1000 );
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(NULL_MODE);

    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());

    PIN_FUNC_SELECT ( FUNC_MUX, FUNC_F );

    gpio_output_set(0, LED_BIT, LED_BIT, 0);
}

/* THE END */
