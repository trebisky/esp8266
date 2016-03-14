/* dumper.c
 * One of my ESP8266 reverse engineering tools
 * Tom Trebisky  3-7-2016
 *
 * This was used first with the ROM image,
 * then extended to dump from the 0 base application image,
 * then extended again to dump from the 0 or 0x40000 base app image.
 *
 * Why do this at all?
 * First of all, it is less than handy to grub around in odx dumps.
 * On top of that, 32 bits values in there are byte swapped.
 * Finally, this gets called from various scripts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define ROM_BINFILE	"bootrom.bin"
#define TEXT_BINFILE	"prod-0x00000.bin"

#define ROM_BASE  0x40000000
#define TEXT_BASE 0x40100000
#define FLASH_BASE 0x40240000

struct image {
	char *filename;
	unsigned int base;
	int size;
	int skip;
};

struct image rom_image;
struct image text_image;
struct image flash_image;

/* XXX - tacky */
char tname[64];
char fname[64];

int vopt;
int is_rom;
int is_string;

void init_image ( struct image *, char *, unsigned int, int );
void dumpit ( struct image *, unsigned int, int );

int main ( int argc, char **argv )
{
	unsigned int addr;
	unsigned int start;
	char *ap;
	char *prefix;
	int count;

	argc--;
	++argv;

	/* The -v is the "value only option"
	 * without it, you get a line like this:
	 * 40100020:			.long 0x8513d300
	 * with it, you get just the value like this:
	 * 8513d300
	 */
	vopt = 0;

	/* This is the rom versus text image option.
	 * when it is zero, we figure we are working with the
	 * bootrom image.
	 * When it is non-zero, we figure we have a pair of application images.
	 */
	is_rom = 1;

	is_string = 0;

	while ( argc && **argv == '-' ) {
	    ap = *argv;
	    if ( ap[1] == 'v' )
		vopt = 1;
	    if ( ap[1] == 'a' ) {
		vopt = 1;
		is_rom = 0;
		prefix = &ap[2];
	    }
	    if ( ap[1] == 's' ) {
		is_string = 1;
	    }
	    argc--;
	    ++argv;
	}

	if ( argc < 1 ) {
	    printf ( "Usage: dumper [-v -aapp] addr [count]\n" );
	    exit ( 100 );
	}

	/* The argument is a base 16 offset into the image.
	 * This works out for the ROM, but for an application
	 * image we have two images, so we expect a hard address
	 * when working with an application.
	 */
	// printf ( "argc = %d\n", argc );
	// printf ( "argv = %s\n", *argv );
	// offset = atoi ( *argv );
	addr = strtol ( *argv, NULL, 16 );
	// printf ( "addr = 0x%08x\n", offset );

	count = 1;
	if ( argc > 1 ) {
	    count = atoi ( argv[1] );
	}

	if ( is_rom ) {
	    init_image ( &rom_image, ROM_BINFILE, ROM_BASE,  0 );

	    if ( addr < ROM_BASE ) addr += ROM_BASE;

	    dumpit ( &rom_image, addr, count );
	    exit ( 0 );
	}

	/* Not a rom, dumping from an application image */
	/* hello-0x40000.bin */

	strcpy ( tname, prefix );
	strcat ( tname, "-0x00000.bin" );
	init_image ( &text_image, tname, TEXT_BASE,  1 );

	strcpy ( fname, prefix );
	strcat ( fname, "-0x40000.bin" );
	init_image ( &flash_image, fname, FLASH_BASE,  0 );

	start = addr >> 20;
	if ( start == 0x401 )
	    dumpit ( &text_image, addr, count );
	else if ( start == 0x402 )
	    dumpit ( &flash_image, addr, count );
	else {
	    // printf ( "Cannot dump address: %08x\n", addr );
	    printf ( "-----\n" );
	}
}

int
get_size ( char *filename )
{
	struct stat sbuf;

	if ( stat ( filename, &sbuf ) < 0 ) {
	    printf ( "Cannot stat file: %s\n", filename );
	    exit ( 100 );
	}

	return sbuf.st_size;
}

/* Actually just the first 5 entries in the following are really the
 * header, but the first control block follows immediately, so we
 * read it all at once.
 */

struct header {
	unsigned char magic;
	unsigned char count;
	unsigned char flags1;
	unsigned char flags2;
	unsigned int entry;
	unsigned int target;
	unsigned int size;
};

void
init_image ( struct image *ip, char *filename, unsigned int base, int has_header )
{
	int fd;
	struct header fheader;

	ip->filename = filename;
	ip->base = base;
	ip->size = get_size ( ip->filename );
	ip->skip = 0;

	if ( ! has_header )
	    return;

	ip->skip = 16;

	/* The first part of an application image has some headers we need to deal with.
	 */
	fd = open ( ip->filename, O_RDONLY );
	if ( fd < 0 ) {
	    printf ( "Cannot open to read header: %s\n", ip->filename );
	    exit ( 100 );
	}
	if ( read ( fd, &fheader, sizeof(struct header) ) != sizeof(struct header) ) {
	    printf ( "error reading header:  %s\n", ip->filename );
	    exit ( 100 );
	}
	close ( fd );

	/*
	printf ( "Magic: %02x\n", fheader.magic );
	printf ( "Base: %08x\n", fheader.target );
	printf ( "Size: %d\n", fheader.size );
	*/

	if ( fheader.magic != 0xe9 ) {
	    printf ( "invalid header from: %s", ip->filename );
	    exit ( 100 );
	}

	if ( fheader.target != base ) {
	    printf ( "unexpected base: 0x%08x", fheader.target );
	    exit ( 100 );
	}

	/* First block had better be the text image */
	ip->size = fheader.size;
}

/* We used to read the entire image into memory, but this will be
 * much faster !!
 */
char *
get_val ( struct image *ip, unsigned int off )
{
	int fd;
	char buf[4];
	unsigned int *p;
	static char bogus[] = "----";
	static char vbuf[16];

	if ( off < 0 )
	    return bogus;

	if ( off > ip->size - sizeof(unsigned int) ) {
	    // printf ("offset beyond end of image: 0x%08x\n", off );
	    // exit ( 100 );
	    return bogus;
	}

	// printf ( "dump from %s at offset %08x\n", ip->filename, off );

	fd = open ( ip->filename, O_RDONLY );
	if ( fd < 0 ) {
	    printf ( "Cannot open %s\n", ip->filename );
	    exit ( 100 );
	}

	lseek ( fd, ip->skip + off, SEEK_SET );
	if ( read ( fd, buf, 4 ) != 4 ) {
	    printf ( "Read error:  %s\n", ip->filename );
	    exit ( 100 );
	}
	close ( fd );

	p = (unsigned int *) buf;
	sprintf ( vbuf, "%08x", *p );
	return vbuf;
}

char *
get_str ( struct image *ip, unsigned int off )
{
	int fd;
	static char buf[256];
	unsigned int *p;
	static char bogus[] = "?";
	int n;

	if ( off < 0 )
	    return bogus;

	if ( off > ip->size - sizeof(unsigned int) ) {
	    // printf ("offset beyond end of image: 0x%08x\n", off );
	    // exit ( 100 );
	    return bogus;
	}

	/* 256 bytes is a wild guess */
	n = 256;
	if ( ip->size - off < 256 )
	    n = ip->size - off;

	fd = open ( ip->filename, O_RDONLY );
	if ( fd < 0 ) {
	    printf ( "Cannot open %s\n", ip->filename );
	    exit ( 100 );
	}

	lseek ( fd, ip->skip + off, SEEK_SET );

	if ( read ( fd, buf, n ) != n ) {
	    printf ( "Read error:  %s\n", ip->filename );
	    exit ( 100 );
	}
	close ( fd );

	/* guarantee termination */
	buf[n-1] = '\0';

	return buf;
}

void
dumpstr ( struct image *ip, unsigned int addr )
{
	unsigned int off;
	char *str;
	char cc;

	off = addr - ip->base;

	str = get_str ( ip, off );
	while ( cc = *str++ ) {
	    if ( cc == '\n' )
		printf ( "\\n" );
	    else
		putchar ( cc );
	}
	printf ( "\n" );
}

void
dump32 ( struct image *ip, unsigned int addr, int count )
{
	int i;
	unsigned int *p;
	char *val;
	unsigned int off;

	// This runs on a 64 bit x86
	// happily an unsigned int is a 4 byte object
	// Also the x86 is little endian like the Xtensa lx106
	// printf ( "uint is %d bytes\n", sizeof(unsigned int) );

	off = addr - ip->base;

	for ( i=0; i<count; i++ ) {
	    val = get_val ( ip, off );
	    // printf ( "image address %08x (offset %x) contains: %08x\n", addr, off, val );
	    if ( vopt ) {
		printf ( "%s\n", val );
	    } else 
		printf ( "%08x:\t\t\t.long 0x%s\n", addr, val );
	    off += 4;
	}
}

void
dumpit ( struct image *ip, unsigned int addr, int count )
{
	if ( is_string )
	    dumpstr ( ip, addr );
	else
	    dump32 ( ip, addr, count );
}

/* THE END */
