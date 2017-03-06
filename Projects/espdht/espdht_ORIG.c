#include "c_types.h"
#include "mem.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "ip_addr.h"
#include "espconn.h"
#include "os_type.h"

#define MAXTIMINGS 10000
#define BREAKTIME 20
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
static volatile os_timer_t some_timer;

static float lastTemp, lastHum;

static char hwaddr[6];

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
static void ICACHE_FLASH_ATTR
at_tcpclient_connect_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;

  os_printf("tcp client connect\r\n");
  os_printf("pespconn %p\r\n", pespconn);

  char payload[128];
  
  espconn_regist_sentcb(pespconn, at_tcpclient_sent_cb);
  espconn_regist_disconcb(pespconn, at_tcpclient_discon_cb);
  os_sprintf(payload, MACSTR ",%d,%d\n", MAC2STR(hwaddr), (int)(lastTemp*100), (int)(lastHum*100));
  os_printf(payload);
  //sent?!
  espconn_sent(pespconn, payload, strlen(payload));
}

static void ICACHE_FLASH_ATTR
sendReading(float t, float h)
{
    struct espconn *pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
    if (pCon == NULL)
    {
        os_printf("CONNECT FAIL\r\n");
        return;
    }
    pCon->type = ESPCONN_TCP;
    pCon->state = ESPCONN_NONE;

    uint32_t ip = ipaddr_addr("192.168.1.200");

    pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    pCon->proto.tcp->local_port = espconn_port();
    pCon->proto.tcp->remote_port = 1337;

    //gad vide hvorfor denne ikke bare sættes ligesom alt det andet?
    os_memcpy(pCon->proto.tcp->remote_ip, &ip, 4);

    //u need some functions
    espconn_regist_connectcb(pCon, at_tcpclient_connect_cb);
    //kan den undværes?
    //espconn_regist_reconcb(pCon, at_tcpclient_recon_cb);
    espconn_connect(pCon);

    os_printf("Temp =  %d *C, Hum = %d \%\n", (int)(t*100), (int)(h*100));
    
    //we will use these in the connect callback..
    lastTemp = t;
    lastHum = h;
}


static void ICACHE_FLASH_ATTR
readDHT(void *arg)
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

    GPIO_OUTPUT_SET(2, 1);
    os_delay_us(250000);
    GPIO_OUTPUT_SET(2, 0);
    os_delay_us(20000);
    GPIO_OUTPUT_SET(2, 1);
    os_delay_us(40);
    GPIO_DIS_OUTPUT(2);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);


    // wait for pin to drop?
    while (GPIO_INPUT_GET(2) == 1 && i<100000) {
          os_delay_us(1);
          i++;
    }

        if(i==100000)
          return;

    // read data!
      
    for (i = 0; i < MAXTIMINGS; i++) {
        counter = 0;
        while ( GPIO_INPUT_GET(2) == laststate) {
            counter++;
                        os_delay_us(1);
            if (counter == 1000)
                break;
        }
        laststate = GPIO_INPUT_GET(2);
        if (counter == 1000) break;

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
    float temp_p, hum_p;
    if (j >= 39) {
        checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
        if (data[4] == checksum) {
            /* yay! checksum is valid */
            
            hum_p = data[0] * 256 + data[1];
            hum_p /= 10;

            temp_p = (data[2] & 0x7F)* 256 + data[3];
            temp_p /= 10.0;
            if (data[2] & 0x80)
                temp_p *= -1;
            sendReading(temp_p, hum_p);
            
        }
    }

}

void ICACHE_FLASH_ATTR user_init(void)
{
    os_printf("\r\nGet this sucker going!\r\n");

    //Set GPIO2 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);

    wifi_get_macaddr(0, hwaddr);

    os_timer_disarm(&some_timer);

    //Setup timer
    os_timer_setfn(&some_timer, (os_timer_func_t *)readDHT, NULL);

    //Arm the timer
    //&some_timer is the pointer
    //1000 is the fire time in ms
    //0 for once and 1 for repeating
    os_timer_arm(&some_timer, 4000, 1);

}


