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

#define SERVER_PORT	1013

#ifdef notdef
static char *ssid = "polecat";
static char *pass = "Slow and steady.";

static char *ssid = "Hoot_Owl";
static char *pass = "The gift of God is eternal life!";
#endif

static char *ssid = "Hoot_Owl";
static char *pass = "The gift of God is eternal life!";

void show_ip ( void );

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

void ICACHE_FLASH_ATTR tcp_reconnect_cb ( void *arg, sint8 err )
{
    os_printf ( "TCP reconnect\n" );
}

void ICACHE_FLASH_ATTR tcp_disconnect_cb ( void *arg )
{
    os_printf ( "TCP disconnect\n" );
}

#ifdef notdef
static int x_count = 0;

/* Callback for when data is received */
void ICACHE_FLASH_ATTR tcp_receive_data ( void *arg, char *buf, unsigned short len )
{
    os_printf ( "TCP receive data: %d bytes\n", len );
    // espconn_send ( arg, buf, len );
    os_memcpy ( x_msg, buf, len );
    x_len = len;
    x_count = 1;
}

/* Call back for when data being sent is finished being sent */
void ICACHE_FLASH_ATTR tcp_send_data ( void *arg )
{
    os_printf ( "TCP send data (done)\n" );
    if ( x_count > 0 ) {
	espconn_send ( arg, x_msg, x_len );
	x_count--;
    }
}
#endif

// static int x_len;
// static char x_msg[64];

int dht_count;
char dht_msg[64];

/* Callback for when data is received */
/* Data received when using telnet ends with cr-lf pair */
void ICACHE_FLASH_ATTR tcp_receive_data ( void *arg, char *buf, unsigned short len )
{
    // os_printf ( "TCP receive data: %d bytes\n", len );
    // os_memcpy ( x_msg, buf, len );
    // x_len = len;

    // espconn_send ( arg, x_msg, x_len );
    os_printf ( "TCP sending %d bytes\n", dht_count );
    espconn_send ( arg, dht_msg, dht_count );
}

/* Call back for when data being sent is finished being sent */
/* not needed unless we are waiting to send more */
void ICACHE_FLASH_ATTR tcp_send_data ( void *arg )
{
    // os_printf ( "TCP send data (done)\n" );
}
 
void ICACHE_FLASH_ATTR tcp_connect_cb ( void *arg )
{
    struct espconn *conn = (struct espconn *)arg;
    char buf[16];

    os_printf ( "TCP connect\n" );
    os_printf ( "  remote ip: %s\n", ip2str ( buf, conn->proto.tcp->remote_ip ) );
    os_printf ( "  remote port: %d\n", conn->proto.tcp->remote_port );

    espconn_regist_recvcb( conn, tcp_receive_data );
    espconn_regist_sentcb( conn, tcp_send_data );
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

/* --------------------------------------------- */
/* --------------------------------------------- */

#define DELAY	2000
// #define GPIO	14	/* D5 */	
// #define GPIO	5	/* D1 */	
// #define GPIO	4	/* D2 */	
#define GPIO	12	/* D6 */	

static os_timer_t timer1;

void
timer_func1 ( void *arg )
{
    int pin = GPIO;
    int t, h;
    int tf;

    (void) dht_sensor ( pin, &t, &h );

    tf = 320 + (t * 9) / 5;

    // os_printf ( "humidity = %d\n", h );
    // os_printf ( "temperature (C) = %d\n", t );
    // os_printf ( "temperature (F) = %d\n", tf );

    // dht_count = os_sprintf ( dht_msg, "%d %d %d\r\n", h, t, tf );
    dht_count = os_sprintf ( dht_msg, "%d %d %d\n", h, t, tf );
}

void
dht_init( void )
{
    unsigned short xx[8];
    unsigned short yy;

    os_timer_disarm ( &timer1 );
    os_timer_setfn ( &timer1, timer_func1, NULL );
    os_timer_arm ( &timer1, DELAY, 1 );

    // pin_input ( GPIO );
    os_printf ( "Ready\n" );

    read_sar_dout ( xx );
    yy = system_adc_read ();
    yy = readvdd33 ();
}

void user_init()
{
    struct station_config conf;

    // This is used to setup the serial communication
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    dht_init ();

    wifi_set_opmode(STATION_MODE);

    // lua.wifi.sta.config("polecat","Slow and steady.")
    // os_bzero ( &conf, sizeof(struct station_config) );
    os_memset ( &conf, 0, sizeof(struct station_config) );
    os_memcpy (&conf.ssid, ssid, 32);
    os_memcpy (&conf.password, pass, 64 );
    wifi_station_set_config (&conf);

    // lua.print(wifi.sta.getip())

    // And this is used to print some information
    os_printf("\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());
    system_print_meminfo();
    os_delay_us ( 1 );
    os_printf ( "CPU Hz = %d\n", system_get_cpu_freq() );

    show_mac ();
    show_ip ();

    /* set a callback for wifi events */
    wifi_set_event_handler_cb ( wifi_event );
}

/* THE END */
