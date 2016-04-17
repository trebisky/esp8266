/* ESP8266 sdk experiments
 * 
 * Sets up a TCP server on port 1013
 * Echos stuff received on port 88
 *
 * Tom Trebisky  12-28-2015
 */

#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

#define TEST_PORT 13	/* daytime */
#define DATA_PORT 2001	/* on trona */

#ifdef notdef
static char *ssid = "polecat";
static char *pass = "Slow and steady.";

static char *ssid = "Hoot_Owl";
static char *pass = "The gift of God is eternal life!";
#endif

static char *ssid = "Hoot_Owl";
static char *pass = "The gift of God is eternal life!";

void show_ip ( void );
void next_time ( void );
void harvest_data ( void );
unsigned long xthal_get_ccount ( void );

unsigned long start_time;

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

#define SERVER_PORT	1013

static int x_count = 0;
static int x_len;
static char x_msg[64];

void ICACHE_FLASH_ATTR
tcp_connect_cb ( void *arg )
{
    os_printf ( "TCP connect\n" );
}

void ICACHE_FLASH_ATTR
tcp_receive_data ( void *arg, char *buf, unsigned short len )
{
    os_printf ( "TCP receive data: %d bytes\n", len );
    espconn_send ( arg, buf, len );
    os_memcpy ( x_msg, buf, len );
    x_len = len;
    x_count = 1;
}

void ICACHE_FLASH_ATTR
tcp_send_data ( void *arg )
{
    os_printf ( "TCP send data\n" );
    if ( x_count > 0 ) {
	espconn_send ( arg, x_msg, x_len );
	x_count--;
    }
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

    os_printf ( "TCP sending %d bytes\n", dht_count );
    espconn_send ( arg, dht_msg, dht_count );
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

/* ------------------------------- */
/* CLIENT stuff starts */
/* ------------------------------- */

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
    // harvest_data ();
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

    espconn_connect ( c );
}

void
wifi_event ( System_Event_t *e )
{
    int event = e->event;

    if ( event == EVENT_STAMODE_GOT_IP ) {
	os_printf ( "WIFI Event, got IP\n" );
	show_ip ();
	// start_timer ();
	// setup_server ();
	start_client ();
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

void
harvest_data ( void )
{
    int dht_hum;
    int dht_tc;
    int dht_tf;
    int dht_status;

    unsigned short v33;
    unsigned short adc;

    v33 = readvdd33 ();
    adc = system_adc_read ();

    //os_printf ( "Vdd33 = %d\n", v33 );
    //os_printf ( "ADC = %d\n", adc );

    dht_status = dht_sensor ( DHT_GPIO, &dht_tc, &dht_hum );

    if ( ! dht_status ) {
	dht_count = os_sprintf ( dht_msg, "%d %d BAD BAD BAD\n" );
	os_printf ( "No data from DHT\n" );
	return;
    }

    dht_tf = 320 + (dht_tc * 9) / 5;

    // os_printf ( "humidity = %d\n", dht_hum );
    // os_printf ( "temperature (C) = %d\n", dht_tc );
    // os_printf ( "temperature (F) = %d\n", dht_tf );

    dht_count = os_sprintf ( dht_msg, "%d %d %d %d %d\n", v33, adc, dht_hum, dht_tc, dht_tf );
    os_printf ( "Got data from DHT\n" );
}

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

void
set_ip_static ( void )
{
    struct ip_info info;

    wifi_station_dhcpc_stop ();
    wifi_softap_dhcps_stop();

    /* This is esp_temp - 192.168.0.9 */
    IP4_ADDR(&info.ip, 192, 168, 0, 9);
    IP4_ADDR(&info.gw, 192, 168, 0, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    wifi_set_ip_info(STATION_IF, &info);
}

void user_init()
{
    struct station_config conf;
    unsigned long time;

    start_time = xthal_get_ccount ();

    // This is used to setup the serial communication
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    harvest_data ();

    time = xthal_get_ccount () - start_time;
    os_printf ( "Time to collect data: %d clocks\n", time );

    set_ip_static ();

    wifi_set_opmode(STATION_MODE);

    // os_bzero ( &conf, sizeof(struct station_config) );
    os_memset ( &conf, 0, sizeof(struct station_config) );
    os_memcpy (&conf.ssid, ssid, 32);
    os_memcpy (&conf.password, pass, 64 );
    wifi_station_set_config (&conf);

    os_printf("\n");

    // os_printf("SDK version:%s\n", system_get_sdk_version());
    // system_print_meminfo();

    os_delay_us ( 1 );
    // os_printf ( "CPU Hz = %d\n", system_get_cpu_freq() );

    show_mac ();
    // show_ip ();

    /* set a callback for wifi events */
    wifi_set_event_handler_cb ( wifi_event );
}

/* THE END */
