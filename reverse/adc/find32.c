/* find32.c
 * look for a certain 32 bit thing in a binary image
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

unsigned int val1 = 0x40101224;
unsigned int val2 = 0x24121040;

#define BUFSIZE	(256 * 1024)

unsigned char image[BUFSIZE];

void search ( int );

int
main ( int argc, char **argv )
{
	int fd;
	int n;

	if ( argc < 2 ) {
	    printf ( "Usage: find32 file\n" );
	    exit ( 0 );
	}

	printf ( "Looking in %s\n", argv[1] );

	fd = open ( argv[1], O_RDONLY );
	n = read ( fd, image, BUFSIZE );
	close ( fd );

	if ( n <= 0 || n == BUFSIZE ) {
	    printf ( "Read error\n" );
	    exit ( 0 );
	}

	printf ( "%d bytes read\n", n );
	search ( n );
}

void
search ( int n )
{
	int x;
	unsigned int *p;

	for ( x = 0; x <= n-4; x++ ) {
	    p = (unsigned int *) &image[x];
	    if ( *p == val1 ) {
		printf ( "Found %08x at %x\n", val1, x );
	    }
	    if ( *p == val2 ) {
		printf ( "Found %08x at %x\n", val2, x );
	    }
	}
}

/* THE END */
