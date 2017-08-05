/* ESP8266 sdk experiments
 *
 * Bell project.
 *
 * The idea here is to have the ESP8266 run a server that
 * listens for commands to ring a bell.
 * I plan to use this to announce events in my workshop,
 * such as visitors pushing the doorbell in the main house,
 * arrival of mail, and other such things.
 *
 * IP address gets set in set_ip_static ();
 *  This is esp_bell - 192.168.0.41
 *
 * I am developing this on a NodeMCU board.
 *
 * A short telnet session talking to this looks like:
 *
 * telnet 192.168.0.41 1013
 * status
 * blue-0 red-0
 * b_on
 * OK
 * status
 * blue-1 red-0
 * b_off
 * OK
 * status
 * blue-0 red-0
 * 
 * Tom Trebisky  8-5-2017
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

#define SERVER
#define SERVER_PORT	1013

/* For Client mode stuff */
#define TEST_PORT 13	/* daytime */
#define DATA_PORT 2001	/* on trona */

#ifdef notdef
static char *ssid = "polecat";
static char *pass = "Your ad here";
#endif

/* My ssid and password are in here */
#include "secret.h"

/* Usually we finish in 0.2 seconds,
 * so allowing 0.5 seconds should be adequate
 * When we first power up, connecting to the
 * wireless can take quite a while, typically
 * about 5.3 seconds.
 */
#define WATCHDOG_INIT	10000
#define WATCHDOG_TCP	500

void show_ip ( void );
void next_time ( void );
void harvest_data ( void );
void set_watchdog ( int );

unsigned long xthal_get_ccount ( void );

/* ------------------------------- */
/* ------------------------------- */

static char blue_state;
static char red_state;

void ICACHE_FLASH_ATTR
gpio16_output_conf(void)
{
}

void ICACHE_FLASH_ATTR
gpio16_output_set(uint8 value)
{
}


/* BIT2 blinks the little blue LED on my NodeMCU board
 * (which is labelled "D4" on the silkscreen)
 * This is the blue LED on the ESP-12E board.
 */
#define BLUE_LED_BIT		BIT2

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

static void
red_on ( void )
{
    WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | 0 );
    red_state = 1;
}

static void
red_off ( void )
{
    WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | 1 );
    red_state = 0;
}


static void
led_init ( void )
{
    /* remember GPIO 1 and 3 are uart */

    /* Setup "bit 2" for output (this is the LED) */
#define FUNC_MUX	PERIPHS_IO_MUX_GPIO2_U
#define FUNC_F		FUNC_GPIO2
    // PIN_FUNC_SELECT ( FUNC_MUX, FUNC_F );
    PIN_FUNC_SELECT ( PERIPHS_IO_MUX_GPIO2_U, 0 );

    blue_off ();

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

#ifdef notdef
/* Could require up to 16 bytes */
char *
ip2str_i ( char *buf, unsigned int ip )
{
    int n1, n2, n3, n4;

    n1 = ip & 0xff;
    ip >>= 8;
    n2 = ip & 0xff;
    ip >>= 8;
    n3 = ip & 0xff;
    ip >>= 8;
    n4 = ip & 0xff;
    os_sprintf ( buf, "%d.%d.%d.%d", n1, n2, n3, n4 );
    return buf;
}
#endif

/* 192.168.0.5 is trona */
void
load_ip ( unsigned char *ip )
{
    ip[0] = 192;
    ip[1] = 168;
    ip[2] = 0;
    ip[3] = 5;
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

#ifdef DNS
static struct espconn dns_conn;
static ip_addr_t dns_ip;

static void ICACHE_FLASH_ATTR
dns_result ( const char *name, ip_addr_t *ip, void *arg )
{
    char buf[16];

    if ( ip == NULL ) {
	os_printf ( "Lookup failed\n" );
	return;
    }

    os_printf ( "Lookup: %s -- %s\n", name, ip2str ( buf, (char *) &ip->addr ));
}

void
dns_lookup ( char *name )
{
    espconn_gethostbyname ( &dns_conn, name, &dns_ip, dns_result );
}
#endif

#ifdef SERVER

/*
 * Sets up a TCP server on port 1013
 */

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
    char buf[16];

    os_sprintf ( buf, "blue-%d red-%d\n", blue_state, red_state );
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
#endif /* SERVER */

/* Not in use */
#ifdef CLIENT
/* ------------------------------- */
/* CLIENT stuff starts */
/* ------------------------------- */

int dht_hum;
int dht_tc;
int dht_status;

unsigned long start_time;

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

int dht_count;
char dht_msg[64];

/* Callback for when data is received */
/* Data received when using telnet ends with cr-lf pair */
/* Does nothing in this code */
void ICACHE_FLASH_ATTR
tcp_receive_data ( void *arg, char *buf, unsigned short len )
{
    os_printf ( "TCP receive data: %d bytes\n", len );

    // os_memcpy ( x_msg, buf, len );
    // x_len = len;

    // espconn_send ( arg, x_msg, x_len );
    os_printf ( "TCP sending %d bytes\n", dht_count );
    espconn_send ( arg, dht_msg, dht_count );
}

/* Call back for when data being sent is finished being sent */
/* not needed unless we are waiting to send more */
void ICACHE_FLASH_ATTR
tcp_send_data ( void *arg )
{
    os_printf ( "TCP send data (done)\n" );
    next_time ();
}

void ICACHE_FLASH_ATTR
send_temps ( void *arg )
{
    int dht_tf;
    int time;
    int div;

    // harvest_data ();

    /* Calculate elapsed time in hundredths of seconds */
    /* system_get_cpu_freq() returns 80 in typical cases */
    div = system_get_cpu_freq() * (1000 * 1000 / 100 );
    time = xthal_get_ccount () - start_time;
    time /= div;

    if ( ! dht_status ) {
	dht_count = os_sprintf ( dht_msg, "%d %d BAD BAD BAD\n", time, 0 );
	os_printf ( "TCP sending %d bytes\n", dht_count );
	espconn_send ( arg, dht_msg, dht_count );
	return;
    }

    dht_tf = 320 + (dht_tc * 9) / 5;

    // os_printf ( "humidity = %d\n", dht_hum );
    // os_printf ( "temperature (C) = %d\n", dht_tc );
    // os_printf ( "temperature (F) = %d\n", dht_tf );

    dht_count = os_sprintf ( dht_msg, "%d %d %d %d %d\n", time, 0, dht_hum, dht_tc, dht_tf );
    os_printf ( "TCP sending %d bytes\n", dht_count );
    espconn_send ( arg, dht_msg, dht_count );
}
 
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

    send_temps ( arg );
}

/* I never see this get called for a client */
void ICACHE_FLASH_ATTR
tcp_client_rcv ( void *arg, char *data, unsigned short len )
{
    os_printf ( "TCP rcv\n" );

    /* daytime service returns 26 bytes.
     * last two bytes are \r\n.
     * There is no null terminator
     */
    //os_printf ( "Last character: %02x %02x\n", data[len-2], data[len-1] );
    //data[len-2] = '\0';
    //os_printf ( "%s\n", data );
}

/* I never see this get called for a client */
void ICACHE_FLASH_ATTR
tcp_client_sent ( void *arg )
{
    os_printf ( "TCP sent\n" );
}

struct espconn client_conn;

void
start_client ( void )
{
    esp_tcp *tp;
    char buf[16];
    struct espconn *c;

    c = &client_conn;
    os_bzero ( c, sizeof(struct espconn) );

    c->type=ESPCONN_TCP;
    c->state=ESPCONN_NONE;

    tp = (esp_tcp *) os_zalloc ( sizeof(esp_tcp) );
    c->proto.tcp = tp;

    tp->local_port = espconn_port();
    tp->remote_port = DATA_PORT;
    load_ip ( tp->remote_ip );

    espconn_regist_connectcb ( c, tcp_connect_cb );
    espconn_regist_disconcb ( c, tcp_disconnect_cb);
    espconn_regist_reconcb ( c, tcp_reconnect_cb );
    espconn_regist_recvcb ( c, tcp_client_rcv );
    espconn_regist_sentcb ( c, tcp_client_sent );

    os_printf ( "Connecting to port %d ", tp->remote_port );
    os_printf ( "local port %d ", tp->local_port );
    os_printf ( "at %s\n", ip2str ( buf, (char *) tp->remote_ip ) );

    /* Do this here so if there is a long delay getting
     * a wireless connection on the first startupt, this
     * will ensure the DHT chip is ready to go.
     */
    harvest_data ();

    /* TCP connect should be fast */
    // set_watchdog ( WATCHDOG_TCP );

    espconn_connect ( c );
}
#endif /* CLIENT */

void
wifi_event ( System_Event_t *e )
{
    int event = e->event;

    if ( event == EVENT_STAMODE_GOT_IP ) {
	os_printf ( "WIFI Event, got IP\n" );
	show_ip ();
	// start_timer ();
	setup_server ();
	// start_client ();
    } else if ( event == EVENT_STAMODE_CONNECTED ) {
	os_printf ( "WIFI Event, connected\n" );
    } else if ( event == EVENT_STAMODE_DISCONNECTED ) {
	os_printf ( "WIFI Event, disconnected\n" );
    } else {
	os_printf ( "Unknown event %d !\n", event );
    }
}

/* --------------------------------------------- */
/* --------------------------------------------- */

/* Not in use */
#ifdef TIMER

static os_timer_t timer1;

/* The timer runs collecting data continuously at 2 Hz */
void
timer_func1 ( void *arg )
{
    harvest_data ();
}

void
start_timer ( void )
{
    os_timer_disarm ( &timer1 );
    os_timer_setfn ( &timer1, timer_func1, NULL );
    os_timer_arm ( &timer1, DELAY, 1 );
}
#endif

#define DELAY	2000

// #define DHT_GPIO	14	/* D5 */	
// #define DHT_GPIO	5	/* D1 */	
// #define DHT_GPIO	4	/* D2 */	
#define DHT_GPIO	12	/* D6 */	

#ifdef notdef
void
harvest_data ( void )
{

#ifdef notdef
    unsigned short v33;
    unsigned short adc;

    v33 = readvdd33 ();
    adc = system_adc_read ();

    //os_printf ( "Vdd33 = %d\n", v33 );
    //os_printf ( "ADC = %d\n", adc );
#endif

    //dht_status = dht_sensor ( DHT_GPIO, &dht_tc, &dht_hum );

    if ( ! dht_status )
	os_printf ( "No data from DHT\n" );
    else
	os_printf ( "Got data from DHT\n" );
}
#endif

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

/* Not in use */
#ifdef SLEEP
/* delay in seconds.
 * can be as large as 4931 seconds.
 */
#define INTERVAL	60

void
next_time ( void )
{
    unsigned long time;

    time = xthal_get_ccount () - start_time;

    os_printf( "sleeping after %ld\n", time );
    system_deep_sleep ( INTERVAL * 1000 * 1000 );
}
#endif /* SLEEP */

#ifdef notdef
#define TIMER_REPEAT	1
#define TIMER_ONCE	0

static os_timer_t dog;

void
run_watchdog ( void *arg )
{
    os_printf ( "Watchdog triggered\n" );
    // next_time ();
}

void
set_watchdog ( int delay )
{
    os_timer_disarm ( &dog );
    os_timer_setfn ( &dog, run_watchdog, NULL );
    os_timer_arm ( &dog, delay, TIMER_ONCE );
}
#endif

void
user_init ( void )
{
    struct station_config conf;
    // unsigned long time;

    // start_time = xthal_get_ccount ();
    // set_watchdog ( WATCHDOG_INIT );

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

    /* once we fall off the end, it is all about event handlers */
}

/* THE END */
