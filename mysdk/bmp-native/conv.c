/* conv.c
 */
#include <stdio.h>

/* This raw pressure value is already shifted by 8-oss */
int rawt = 25179;
int rawp = 77455;

int ac1 = 8240;
int ac2 = -1196;
int ac3 = -14709;
int ac4 = 32912;
int ac5 = 24959;
int ac6 = 16487;
int b1 = 6515;
int b2 = 48;
int mb = -32768;
int mc = -11786;
int md = 2845;

int b5;

/* Yields temperature in 0.1 degrees C */
int
conv_temp ( int raw )
{
	int x1, x2;

	x1 = (raw - ac6) * ac5 / 32768;
	x2 = mc * 2048 / (x1 + md);
	b5 = x1 + x2;
	return (b5+8) / 16;
}

/* Yields pressure in Pa */
int
conv_pressure ( int raw, int oss )
{
	int osspow = 2^oss;
	int b3, b6, b4, b7;
	int x1, x2, x3;
	int p;

	b6 = b5 - 4000;
        x1 = b2 * (b6 * b6 / 4096) / 2048;
        x2 = ac2 * b6 / 2048;
	x3 = x1 + x2;
	b3 = ((ac1 * 4 + x3) * osspow + 2) / 4;
	x1 = ac3 * b6 / 8192;
	x2 = (b1 * (b6 * b6 / 4096)) / 65536;
	x3 = (x1 + x2 + 2) / 4;

	b4 = ac4 * (x3 + 32768) / 32768;
	b7 = (raw - b3) * (50000/osspow);

	p = (b7 / b4) * 2;
	x1 = (p / 256) * (p / 256);
	x1 = (x1 * 3038) / 65536;
	x2 = (-7357 * p) / 65536;
	p += (x1 + x2 + 3791) / 16;

	return p;
}

#define OSS	0

#define MB_TUCSON	84.0

int
main ( int argc, char **argv )
{
	int t;
	double tc;
	double tf;
	int p;
	double pmb;
	double pmb_sea;

	t = conv_temp ( rawt );
	tc = t / 10.0;
	tf = 32.0 + tc * 1.8;

	printf ( "Temp = %d\n", t );
	printf ( "Temp (C) = %.3f\n", tc );
	printf ( "Temp (F) = %.3f\n", tf );

	p = conv_pressure ( rawp, OSS );
	pmb = p / 100.0;
	pmb_sea = pmb + MB_TUCSON;

	printf ( "Pressure (Pa) = %d\n", p );
	printf ( "Pressure (mb) = %.2f\n", pmb );
	printf ( "Pressure (mb, sea level) = %.2f\n", pmb_sea );
}
