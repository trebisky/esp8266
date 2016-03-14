#include "c_types.h"
#include "mem.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "ip_addr.h"
#include "espconn.h"
#include "os_type.h"

#include "user_interface.h"

#define MAXTIMINGS 10000
#define BREAKTIME 20
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

static volatile os_timer_t some_timer;

static char hwaddr[6];

static char *ssid = "polecat";
static char *pass = "Slow and steady.";

#define	TARGET_IP	"192.168.0.5"
#define	TARGET_PORT	1337

/* tjt - look up mux address given GPIO */
static const int
 mux[] = {
    PERIPHS_IO_MUX_GPIO0_U,     /* 0 - D3 */
    PERIPHS_IO_MUX_U0TXD_U,     /* 1 - uart */
    PERIPHS_IO_MUX_GPIO2_U,     /* 2 - D4 */
    PERIPHS_IO_MUX_U0RXD_U,     /* 3 - uart */
    PERIPHS_IO_MUX_GPIO4_U,     /* 4 - D2 */
    PERIPHS_IO_MUX_GPIO5_U,     /* 5 - D1 */
    0,  /* 6 */
    0,  /* 7 */
    0,  /* 8 */
    PERIPHS_IO_MUX_SD_DATA2_U,  /* 9   - D11 (SD2) */
    PERIPHS_IO_MUX_SD_DATA3_U,  /* 10  - D12 (SD3) */
    0,  /* 11 */
    PERIPHS_IO_MUX_MTDI_U,      /* 12 - D6 */
    PERIPHS_IO_MUX_MTCK_U,      /* 13 - D7 */
    PERIPHS_IO_MUX_MTMS_U,      /* 14 - D5 */
    PERIPHS_IO_MUX_MTDO_U       /* 15 - D8 */
};

static const int func[] = { 0, 3, 0, 3,   0, 0, 3, 3,   3, 3, 3, 3,   3, 3, 3, 3 };

/* Could require up to 16 bytes */
static char * ICACHE_FLASH_ATTR
ip2str ( char *buf, unsigned char *p )
{
    os_sprintf ( buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3] );
    return buf;
}

static void ICACHE_FLASH_ATTR
show_ip ( void )
{
    struct ip_info info;
    char buf[16];

    wifi_get_ip_info ( STATION_IF, &info );
    // os_printf ( "IP: %08x\n", info.ip );
    // os_printf ( "IP: %s\n", ip2str_i ( buf, info.ip.addr ) );
    os_printf ( "IP: %s\n", ip2str ( buf, (char *) &info.ip.addr ) );
}

static void ICACHE_FLASH_ATTR
wifi_event ( System_Event_t *e )
{
    int event = e->event;

    if ( event == EVENT_STAMODE_GOT_IP ) {
        os_printf ( "WIFI Event, got IP\n" );
        show_ip ();
        // setup_server ();
    } else if ( event == EVENT_STAMODE_CONNECTED ) {
        os_printf ( "WIFI Event, connected\n" );
    } else if ( event == EVENT_STAMODE_DISCONNECTED ) {
        os_printf ( "WIFI Event, disconnected\n" );
    } else {
        os_printf ( "Unknown event %d !\n", event );
    }
}

static void ICACHE_FLASH_ATTR
wifi_setup ( void )
{
    struct station_config conf;

    wifi_set_opmode(STATION_MODE);

    // os_bzero ( &conf, sizeof(struct station_config) );
    os_memset ( &conf, 0, sizeof(struct station_config) );
    os_memcpy (&conf.ssid, ssid, 32);
    os_memcpy (&conf.password, pass, 64 );
    wifi_station_set_config (&conf);

    /* set a callback for wifi events */
    wifi_set_event_handler_cb ( wifi_event );
}

static void ICACHE_FLASH_ATTR
at_tcpclient_sent_cb(void *arg) {
    os_printf("sent callback\n");
     struct espconn *pespconn = (struct espconn *)arg;
     espconn_disconnect(pespconn);
}

static void ICACHE_FLASH_ATTR
at_tcpclient_discon_cb(void *arg) {
    struct espconn *pespconn = (struct espconn *)arg;
    os_free(pespconn->proto.tcp);

    os_free(pespconn);
    os_printf("disconnect callback\n");

}

// float temp_p, hum_p;
static int temp_p, hum_p;

static void ICACHE_FLASH_ATTR
at_tcpclient_connect_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;

  os_printf("tcp client connect\r\n");
  os_printf("pespconn %p\r\n", pespconn);

  char payload[128];
  
  espconn_regist_sentcb(pespconn, at_tcpclient_sent_cb);
  espconn_regist_disconcb(pespconn, at_tcpclient_discon_cb);

  os_sprintf(payload, MACSTR ",%d,%d\n", MAC2STR(hwaddr), temp_p, hum_p );

  os_printf(payload);
  //sent?!
  espconn_sent(pespconn, payload, strlen(payload));
}

static void ICACHE_FLASH_ATTR
// sendReading(float t, float h)
sendReading ( int t, int h)
{
    struct espconn *pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
    if (pCon == NULL) {
        os_printf("CONNECT FAIL\r\n");
        return;
    }
    pCon->type = ESPCONN_TCP;
    pCon->state = ESPCONN_NONE;

    uint32_t ip = ipaddr_addr(TARGET_IP);

    pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    pCon->proto.tcp->local_port = espconn_port();
    pCon->proto.tcp->remote_port = TARGET_PORT;

    //gad vide hvorfor denne ikke bare sættes ligesom alt det andet?
    os_memcpy(pCon->proto.tcp->remote_ip, &ip, 4);

    //u need some functions
    espconn_regist_connectcb(pCon, at_tcpclient_connect_cb);
    //kan den undværes?
    //espconn_regist_reconcb(pCon, at_tcpclient_recon_cb);
    espconn_connect(pCon);

    //we will use these in the connect callback..
    // lastTemp = t;
    // lastHum = h;
}

static void ICACHE_FLASH_ATTR
readDHT ( int pin )
{
    int counter = 0;
    int laststate = 1;
    int i = 0;
    int j = 0;
    int checksum = 0;
    //int bitidx = 0;
    //int bits[250];

    int data[100];

    data[0] = data[1] = data[2] = data[3] = data[4] = 0;

    PIN_FUNC_SELECT ( mux[pin], func[pin] );
    PIN_PULLUP_EN ( mux[pin] );

    GPIO_OUTPUT_SET(pin, 1);
    os_delay_us(250000);
    GPIO_OUTPUT_SET(pin, 0);
    os_delay_us(20000);
    GPIO_OUTPUT_SET(pin, 1);
    os_delay_us(40);

    GPIO_DIS_OUTPUT(pin);

    // PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);
    // PIN_PULLUP_EN ( mux[pin] );

    // wait for pin to drop?
    while (GPIO_INPUT_GET(pin) == 1 && i<100000) {
          os_delay_us(1);
          i++;
    }

    // os_printf ( "Drop took %d microseconds\n", i );
    // Always reports zero

    if(i==100000)
          return;

    // read data!
      
    for (i = 0; i < MAXTIMINGS; i++) {
        counter = 0;
        while ( GPIO_INPUT_GET(pin) == laststate) {
            counter++;
	    os_delay_us(1);
            if (counter == 1000)
                break;
        }

        laststate = GPIO_INPUT_GET(pin);
        if (counter == 1000)
	    break;

        //bits[bitidx++] = counter;

        if ((i>3) && (i%2 == 0)) {
            // shove each bit into the storage bytes
            data[j/8] <<= 1;
            if (counter > BREAKTIME)
                data[j/8] |= 1;
            j++;
        }
    }

/*
    for (i=3; i<bitidx; i+=2) {
        os_printf("bit %d: %d\n", i-3, bits[i]);
        os_printf("bit %d: %d (%d)\n", i-2, bits[i+1], bits[i+1] > BREAKTIME);
    }
    os_printf("Data (%d): 0x%x 0x%x 0x%x 0x%x 0x%x\n", j, data[0], data[1], data[2], data[3], data[4]);
*/

    if (j >= 39) {
        checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
        if (data[4] == checksum) {
            /* yay! checksum is valid */
            
            hum_p = data[0] * 256 + data[1];
            // hum_p /= 10;

            temp_p = (data[2] & 0x7F)* 256 + data[3];
            // temp_p /= 10.0;
            if (data[2] & 0x80)
                temp_p *= -1;
        }
    }
}

// #define DHT_GPIO	0	/* D3 - OK */
// #define DHT_GPIO	2	/* D4 - OK */
// #define DHT_GPIO	4	/* D2 - OK */
// #define DHT_GPIO	5	/* D1 - OK */
// #define DHT_GPIO	9	/* D11/SD2 - causes WDT resets */
// #define DHT_GPIO	10	/* D12/SD3 - causes WDT resets */
#define DHT_GPIO	12	/* D6 - OK */
// #define DHT_GPIO	13	/* D7 - OK */
// #define DHT_GPIO	14	/* D5 - OK */
// #define DHT_GPIO	15	/* D8 - interferes with boot */

static int last_us = 0;

static void ICACHE_FLASH_ATTR
ticker (void *arg)
{
    unsigned int ccount;
    int rate;
    int us;
    unsigned int *p;

    readDHT ( DHT_GPIO );

    os_printf("Temp =  %d *C, Hum = %d \%\n", temp_p, hum_p );

#ifdef notdef
    ccount = xthal_get_ccount();
    rate = system_get_cpu_freq();
    us = ccount / rate;
    os_printf ( "us = %d, %d, ccount = %d  rate = %d\n", us-last_us, us, ccount, rate );
    last_us = us;
    p = (unsigned int *) 0x40002ec8;
    os_printf ( "Ptr %08x fetches %08x\n", p, *p );
    p = (unsigned int *) *p;
    os_printf ( "Ptr %08x fetches %08x\n", p, *p );
#endif

    // sendReading(temp_p, hum_p);
}

// The update period in ms
// #define INTERVAL	4000
#define INTERVAL	2000

void ICACHE_FLASH_ATTR user_init(void)
{
    // tjt - need this once on any board
    wifi_setup ();

    // This is used to setup the serial communication
    // tjt - I like 115200 baud.
    uart_div_modify(0, UART_CLK_FREQ / 115200);
    os_printf("\nRead values from GPIO %d\n", DHT_GPIO );

    //Set GPIO2 to output mode
    // PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    // PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);

    wifi_get_macaddr(0, hwaddr);

    os_timer_disarm(&some_timer);

    //Setup timer
    os_timer_setfn(&some_timer, (os_timer_func_t *) ticker, NULL);

    //Arm the timer
    //&some_timer is the pointer
    //0 for once and 1 for repeating
    os_timer_arm(&some_timer, INTERVAL, 1);
}

/* THE END */
