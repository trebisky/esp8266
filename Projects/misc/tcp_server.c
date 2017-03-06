/* ESP8266 sdk experiments
 * Set up a TCP server.
 * Tom Trebisky  12-28-2015
 * Echos stuff received on port 88
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

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

static int x_count = 0;
static int x_len;
static char x_msg[64];

void ICACHE_FLASH_ATTR tcp_receive_data ( void *arg, char *buf, unsigned short len )
{
    os_printf ( "TCP receive data: %d bytes\n", len );
    espconn_send ( arg, buf, len );
    os_memcpy ( x_msg, buf, len );
    x_len = len;
    x_count = 1;
}

void ICACHE_FLASH_ATTR tcp_send_data ( void *arg )
{
    os_printf ( "TCP send data\n" );
    if ( x_count > 0 ) {
	espconn_send ( arg, x_msg, x_len );
	x_count--;
    }
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
    my_tcp_conn.local_port=88;
    c->proto.tcp=&my_tcp_conn;

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
 
void user_init()
{
    struct station_config conf;

    // This is used to setup the serial communication
    uart_div_modify(0, UART_CLK_FREQ / 115200);

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
