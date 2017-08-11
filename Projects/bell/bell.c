/* ESP8266 sdk experiments
 *
 * This is a simple TCP server that handles requests to
 *  ring a bell via a GPIO bit that controls an SSR.
 *
 * This runs on a NodeMCU devel board.
 * It also controls the two on-board LEDs on the NodeMCU board.
 * One is on GPIO-2,
 * The other is on GPIO-16 (which actually isn't a GPIO).
 *
 * This is derived from my led_server project.
 *
 * The idea is to have the ESP8266 run a server that
 * listens for commands to ring a bell.
 * I plan to use this to announce events in my workshop,
 * such as visitors pushing the doorbell in the main house,
 * arrival of USPS mail, and other such things.
 *
 * The IP address gets set in set_ip_static ();
 *  This is esp_bell - 192.168.0.41
 *
 * Tom Trebisky  8-7-2017
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

#define SERVER_PORT	1013

#ifdef notdef
static char *ssid = "polecat";
static char *pass = "Your ad here";
#endif

/* My ssid and password are in here */
#include "secret.h"

/* ------------------------------- */
/* ------------------------------- */

static char blue_state;
static char red_state;
static char bell_state;

static os_timer_t timer1;

/* BIT2 blinks the little blue LED on my NodeMCU board
 * (which is labelled "D4" on the silkscreen)
 * This is the blue LED on the ESP-12E board.
 */
#define BLUE_LED_BIT		BIT2

/* Some care is required to pick the bit to control
 *    the SSR that rings the bell.
 * GPIO-0 is a bad choice because it controls BOOT behavior.
 * GPIO-2 and GPIO-15 are also involved in BOOT
 * GPIO-2 and GPIO-16 have LED's hung on them.
 * GPIO-1 and GPIO-3 are used for the console uart.
 * GPIO 6 through 11 connect to the SD (flash) chip.
 * It turns out there are only 5 bits without issues:
 *    4, 5, 12, 13, 14
 */

#define BELL_BIT		BIT4	/* NodeMCU D2 */
// #define BELL_BIT		BIT5	/* NodeMCU D1 */
// #define BELL_BIT		BIT12	/* NodeMCU D6 */
// #define BELL_BIT		BIT13	/* NodeMCU D7 */
// #define BELL_BIT		BIT14	/* NodeMCU D5 */
// #define BELL_BIT		BIT0	/* NodeMCU D3 -- BAD, tangles with boot */

static void
blue_on ( void )
{
    gpio_output_set(0, BLUE_LED_BIT, BLUE_LED_BIT, 0);
    blue_state = 1;
}

static void
blue_off ( void )
{
    gpio_output_set(BLUE_LED_BIT, 0, BLUE_LED_BIT, 0);
    blue_state = 0;
}

/* This pulls low (I see 0.15 volts on DVM) */
static void
bell_off ( void )
{
    gpio_output_set(0, BELL_BIT, BELL_BIT, 0);
    bell_state = 1;
}

/* This pulls high (I see 3.3 volts on DVM) */
/* This drives my Opto-22 240D45 SSR just fine */
static void
bell_on ( void )
{
    gpio_output_set(BELL_BIT, 0, BELL_BIT, 0);
    bell_state = 0;
}

void
timer_func1 ( void *arg )
{
    os_printf("timer\n");
    bell_off ();
}

#define TIMER_REPEAT    1
#define TIMER_ONCE      0

static void
ring_bell ( void )
{
    bell_on ();
    /* This gets called with a delay in milliseconds.
     * The final argument is a repeat flag
     */
    os_timer_arm ( &timer1, 500, TIMER_ONCE );
}

#ifdef notdef
static void
flip_blue ( void )
{
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & BLUE_LED_BIT) {
	gpio_output_set(0, BLUE_LED_BIT, BLUE_LED_BIT, 0);
	// os_printf("set high\n");
    } else {
	gpio_output_set(BLUE_LED_BIT, 0, BLUE_LED_BIT, 0);
	// os_printf("set low\n");
    }
}
#endif

static void
red_on ( void )
{
    // WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | 0 );
    WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & ~1));
    red_state = 1;
}

static void
red_off ( void )
{
    // WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | 1 );
    WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) | 1 ));
    red_state = 0;
}


static void
led_init ( void )
{
    /* Setup "bit 2" for output (this is the LED) */
#define FUNC_MUX	PERIPHS_IO_MUX_GPIO2_U
#define FUNC_F		FUNC_GPIO2
    // PIN_FUNC_SELECT ( FUNC_MUX, FUNC_F );
    PIN_FUNC_SELECT ( PERIPHS_IO_MUX_GPIO2_U, 0 );
    blue_off ();

    // Also set up bit 0 (GPIO) for bell output */
    PIN_FUNC_SELECT ( PERIPHS_IO_MUX_GPIO0_U, 0 );
    bell_off ();

    /* The red LED is on GPIO 16, which is not really a GPIO, but the rtc unit */
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1);      // mux configuration for XPD_DCDC to output rtc_gpio0

    WRITE_PERI_REG(RTC_GPIO_CONF,
                   (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);  //mux configuration for out enable

    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   (READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe) | (uint32)0x1);        //out enable

    red_off ();
}

/* ------------------------------- */
/* ------------------------------- */

void
show_mac ( void )
{
    unsigned char mac[6];

    wifi_get_macaddr ( STATION_IF, mac );
    os_printf ( "MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
	mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
}

/* Could require up to 16 bytes */
char *
ip2str ( char *buf, unsigned char *p )
{
    os_sprintf ( buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3] );
    return buf;
}

void
show_ip ( void )
{
    struct ip_info info;
    char buf[16];

    wifi_get_ip_info ( STATION_IF, &info );
    // os_printf ( "IP: %08x\n", info.ip );
    // os_printf ( "IP: %s\n", ip2str_i ( buf, info.ip.addr ) );
    os_printf ( "IP: %s\n", ip2str ( buf, (char *) &info.ip.addr ) );
}

static void
my_reply ( void *arg, char *reply )
{
    char buf[16];

    strcpy ( buf, reply );
    strcat ( buf, "\n" );
    espconn_send ( arg, buf, strlen(buf) );
}

static void
my_status ( void *arg )
{
    char buf[24];

    os_sprintf ( buf, "blue-%d red-%d bell-%d\n", blue_state, red_state, bell_state );
    espconn_send ( arg, buf, strlen(buf) );
}

// static int x_count = 0;
// static int x_len;
// static char x_msg[64];

static void
do_cmd ( void *arg, char *buf )
{
    if ( strcmp ( buf, "status" ) == 0 ) {
	my_status ( arg );
    } else if ( strcmp ( buf, "b_on" ) == 0 ) {
	blue_on ();
	my_reply ( arg, "OK" );
    } else if ( strcmp ( buf, "b_off" ) == 0 ) {
	blue_off ();
	my_reply ( arg, "OK" );
    } else if ( strcmp ( buf, "r_on" ) == 0 ) {
	red_on ();
	my_reply ( arg, "OK" );
    } else if ( strcmp ( buf, "r_off" ) == 0 ) {
	red_off ();
	my_reply ( arg, "OK" );
    } else if ( strcmp ( buf, "bell_on" ) == 0 ) {
	bell_on ();
	my_reply ( arg, "OK" );
    } else if ( strcmp ( buf, "bell_off" ) == 0 ) {
	bell_off ();
	my_reply ( arg, "OK" );
    } else if ( strcmp ( buf, "bell" ) == 0 ) {
        ring_bell ();
        my_reply ( arg, "OK" );
    } else
	my_reply ( arg, "ERR" );
	    
}

/* Here when we receive a message.
 * Messages from telnet end in \r\n
 * Messages from our ruby script end in \n
 */
void ICACHE_FLASH_ATTR
tcp_receive_data ( void *arg, char *buf, unsigned short len )
{
    int i;

    os_printf ( "TCP receive data: %d bytes\n", len );

    if ( len < 2 ) {
	os_printf ( "runt\n" );
	my_reply ( arg, "ERR" );
	return;
    }

    if ( buf[len-1] == '\n' )
	buf[len-1] = '\0';
    if ( buf[len-2] == '\r' )
	buf[len-2] = '\0';

    for ( i=0; i<=len; i++ ) {
	os_printf ( "msg-%d = %x\n", i, buf[i] );
    }

    do_cmd ( arg, buf );

    // espconn_send ( arg, buf, len );

    // os_memcpy ( x_msg, buf, len );
    // x_len = len;
    // x_count = 1;
}

/* Here when we finish sending our reply */
void ICACHE_FLASH_ATTR
tcp_send_data ( void *arg )
{
    os_printf ( "TCP send data\n" );

    /*
    if ( x_count > 0 ) {
	espconn_send ( arg, x_msg, x_len );
	x_count--;
    }
    */
}
 
/* After a connect there appears to be a 40 second timeout
 * and then this server will do a disconnect.
 * Activity seems to reset this timer, but it will go off
 * 40 seconds after the last receive/send action.
 */
void ICACHE_FLASH_ATTR
tcp_connect_cb ( void *arg )
{
    struct espconn *conn = (struct espconn *)arg;
    char buf[16];

    os_printf ( "TCP connect\n" );
    os_printf ( "  remote ip: %s\n", ip2str ( buf, conn->proto.tcp->remote_ip ) );
    os_printf ( "  remote port: %d\n", conn->proto.tcp->remote_port );

    espconn_regist_recvcb( conn, tcp_receive_data );
    espconn_regist_sentcb( conn, tcp_send_data );

    // os_printf ( "TCP sending %d bytes\n", dht_count );
    // espconn_send ( arg, dht_msg, dht_count );
}

/* rare, and I don't care */
void ICACHE_FLASH_ATTR
tcp_reconnect_cb ( void *arg, sint8 err )
{
    os_printf ( "TCP reconnect\n" );
}

/* Called when a client connection gets closed by the other end */
void ICACHE_FLASH_ATTR
tcp_disconnect_cb ( void *arg )
{
    os_printf ( "TCP disconnect\n" );
}

static struct espconn server_conn;
static esp_tcp my_tcp_conn;

void
setup_server ( void )
{
    register struct espconn *c = &server_conn;
    char *x;

    c->type = ESPCONN_TCP;
    c->state = ESPCONN_NONE;
    my_tcp_conn.local_port = SERVER_PORT;
    c->proto.tcp = &my_tcp_conn;

    espconn_regist_reconcb ( c, tcp_reconnect_cb);
    espconn_regist_connectcb ( c, tcp_connect_cb);
    espconn_regist_disconcb ( c, tcp_disconnect_cb);

    if ( espconn_accept(c) != ESPCONN_OK ) {
	os_printf("Error starting server %d\n", 0);
	return;
    }

    /* Interval in seconds to timeout inactive connections */
    espconn_regist_time(c, 20, 0);

    // x = (char *) os_zalloc ( 4 );
    // os_printf ( "Got mem: %08x\n", x );

    os_printf ( "Server ready\n" );
}

void
wifi_event ( System_Event_t *e )
{
    int event = e->event;

    if ( event == EVENT_STAMODE_GOT_IP ) {
	os_printf ( "WIFI Event, got IP\n" );
	show_ip ();
	setup_server ();
    } else if ( event == EVENT_STAMODE_CONNECTED ) {
	os_printf ( "WIFI Event, connected\n" );
    } else if ( event == EVENT_STAMODE_DISCONNECTED ) {
	os_printf ( "WIFI Event, disconnected\n" );
    } else {
	os_printf ( "Unknown event %d !\n", event );
    }
}

void
set_ip_static ( void )
{
    struct ip_info info;

    wifi_station_dhcpc_stop ();
    wifi_softap_dhcps_stop();

    /* This is esp_temp - 192.168.0.9 */
    /* This is esp_bell - 192.168.0.41 */
    IP4_ADDR(&info.ip, 192, 168, 0, 41);
    IP4_ADDR(&info.gw, 192, 168, 0, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    wifi_set_ip_info(STATION_IF, &info);
}

void
user_init ( void )
{
    struct station_config conf;

    // This is used to setup the serial communication
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    wifi_set_opmode(STATION_MODE);
    set_ip_static ();

    // os_bzero ( &conf, sizeof(struct station_config) );
    os_memset ( &conf, 0, sizeof(struct station_config) );
    os_memcpy (&conf.ssid, ssid, 32);
    os_memcpy (&conf.password, pass, 64 );
    wifi_station_set_config (&conf);

    os_printf("\n");

    led_init ();

    // os_printf("SDK version:%s\n", system_get_sdk_version());
    // system_print_meminfo();

    os_delay_us ( 1 );
    // os_printf ( "CPU Hz = %d\n", system_get_cpu_freq() );

    show_mac ();
    // show_ip ();

    /* set a callback for wifi events */
    wifi_set_event_handler_cb ( wifi_event );

    os_timer_disarm ( &timer1 );
    os_timer_setfn ( &timer1, timer_func1, NULL );

    /* once we fall off the end, it is all about event handlers */
}

/* THE END */
