#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <asm/termios.h>

#include <stdio.h>
#include <stdlib.h>

char *device = "/dev/ttyUSB0";
// int speed = 76800;
int speed = 74880;

int ioctl ( int, int, struct termios2 *);

#ifdef notdef
#include <sys/ioctl.h>
#include <linux/serial.h>

void
try_ioctl ( int fd )
{
    struct serial_struct ser_info; 
    int x;

    x = ioctl( fd, TIOCGSERIAL, &ser_info ); 
    // printf ( "x = %d\n", x );

    // ser_info.flags = ASYNC_SPD_CUST | ASYNC_LOW_LATENCY; 
    // ser_info.custom_divisor = ser_info.baud_base / CUST_BAUD_RATE; 
    // ioctl(ser_dev, TIOCSSERIAL, &ser_info);
}
#endif

void
try_termios ( int fd )
{
    struct termios2 tio;
    int x;

    x = ioctl(fd, TCGETS2, &tio);
    // printf ( "x = %d\n", x );

    tio.c_cflag &= ~CBAUD;
    tio.c_cflag |= BOTHER;
    tio.c_ispeed = speed;
    tio.c_ospeed = speed;
    x = ioctl(fd, TCSETS2, &tio);
    // printf ( "x = %d\n", x );

/*
    close(fd);

    if (r == 0) {
        printf("Changed successfully.\n");
    } else {
        perror("ioctl");
    }
*/
}

int main ( int argc, char **argv )
{
    int fd;

    if ( argc == 2 )
	device = argv[1];

    if ( argc > 2 )
	speed = atoi(argv[2]);

    fd = open ( device, O_RDWR );
    // printf ( "fd = %d\n", fd );
    if ( fd < 0 ) {
	printf ("Sorry, cannot open %s\n", device );
	return 1;
    }

    try_termios ( fd );
    printf ( "Baud rate for %s set to %d\n", device, speed );

}

/* THE END */
