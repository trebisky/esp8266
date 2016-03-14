/* ESP8266 sdk experiments - play with wifi
 * Tom Trebisky  12-28-2015
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

#include "user_interface.h"

static char *ssid = "polecat";
static char *pass = "Slow and steady.";

void show_ip ( void );

void
show_mac ( void )
{
    unsigned char mac[6];

    wifi_get_macaddr ( STATION_IF, mac );
    os_printf ( "MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
	mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
}

void
print_ip ( unsigned int ip )
{
    int n1, n2, n3, n4;

    n1 = ip & 0xff;
    ip >>= 8;
    n2 = ip & 0xff;
    ip >>= 8;
    n3 = ip & 0xff;
    ip >>= 8;
    n4 = ip & 0xff;
    os_printf ( "%d.%d.%d.%d", n1, n2, n3, n4 );
}

void
show_ip ( void )
{
    struct ip_info info;

    wifi_get_ip_info ( STATION_IF, &info );
    os_printf ( "IP: %08x\n", info.ip );
    os_printf ( "IP: " );
	print_ip ( info.ip.addr );
	os_printf ( "\n" );
}

void
wifi_event ( System_Event_t *e )
{
    int event = e->event;

    if ( event == EVENT_STAMODE_GOT_IP ) {
	os_printf ( "Event, got IP\n" );
	show_ip ();
    } else if ( event == EVENT_STAMODE_CONNECTED ) {
	os_printf ( "Event, connected\n" );
    } else if ( event == EVENT_STAMODE_DISCONNECTED ) {
	os_printf ( "Event, disconnected\n" );
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

    show_mac ();
    show_ip ();

    /* set a callback for wifi events */
    wifi_set_event_handler_cb ( wifi_event );
}

/* THE END */
