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

	printf ( "Temp (raw) = %d\n", rawt );
	printf ( "Temp = %d\n", t );
	printf ( "Temp (C) = %.3f\n", tc );
	printf ( "Temp (F) = %.3f\n", tf );

	p = conv_pressure ( rawp, OSS );
	pmb = p / 100.0;
	pmb_sea = pmb + MB_TUCSON;

	printf ( "Pressure (raw) = %d\n", rawp );
	printf ( "Pressure (Pa) = %d\n", p );
	printf ( "Pressure (mb) = %.2f\n", pmb );
	printf ( "Pressure (mb, sea level) = %.2f\n", pmb_sea );
}

/*
-- 16-bit  two's complement
-- value: 16-bit integer
local function twoCompl(value)
 if value > 32767 then value = -(65535 - value + 1)
 end
 return value
end

-- initialize module
-- sda: SDA pin
-- scl SCL pin
function M.init(sda, scl)
  i2c.setup(id, sda, scl, i2c.SLOW)
  local calibration = read_reg(REG_CALIBRATION, 22)

  AC1 = twoCompl(string.byte(calibration, 1) * 256 + string.byte(calibration, 2))
  AC2 = twoCompl(string.byte(calibration, 3) * 256 + string.byte(calibration, 4))
  AC3 = twoCompl(string.byte(calibration, 5) * 256 + string.byte(calibration, 6))
  AC4 = string.byte(calibration, 7) * 256 + string.byte(calibration, 8)
  AC5 = string.byte(calibration, 9) * 256 + string.byte(calibration, 10)
  AC6 = string.byte(calibration, 11) * 256 + string.byte(calibration, 12)
  B1 = twoCompl(string.byte(calibration, 13) * 256 + string.byte(calibration, 14))
  B2 = twoCompl(string.byte(calibration, 15) * 256 + string.byte(calibration, 16))
  MB = twoCompl(string.byte(calibration, 17) * 256 + string.byte(calibration, 18))
  MC = twoCompl(string.byte(calibration, 19) * 256 + string.byte(calibration, 20))
  MD = twoCompl(string.byte(calibration, 21) * 256 + string.byte(calibration, 22))

  init = true
end

-- read temperature from BMP180
local function read_temp()
  write_reg(REG_CONTROL, COMMAND_TEMPERATURE)
  tmr.delay(5000)
  local dataT = read_reg(REG_RESULT, 2)
  UT = string.byte(dataT, 1) * 256 + string.byte(dataT, 2)
  local X1 = (UT - AC6) * AC5 / 32768
  local X2 = MC * 2048 / (X1 + MD)
  B5 = X1 + X2
  t = (B5 + 8) / 16
  return(t)
end

-- read pressure from BMP180
-- must be read after read temperature
local function read_pressure(oss)
  write_reg(REG_CONTROL, COMMAND_PRESSURE[oss + 1]);
  tmr.delay(30000);
  local dataP = read_reg(0xF6, 3)
  local UP = string.byte(dataP, 1) * 65536 + string.byte(dataP, 2) * 256 + string.byte(dataP, 1)
  UP = UP / 2 ^ (8 - oss)
  local B6 = B5 - 4000
  local X1 = B2 * (B6 * B6 / 4096) / 2048
  local X2 = AC2 * B6 / 2048
  local X3 = X1 + X2
  local B3 = ((AC1 * 4 + X3) * 2 ^ oss + 2) / 4
  X1 = AC3 * B6 / 8192
  X2 = (B1 * (B6 * B6 / 4096)) / 65536
  X3 = (X1 + X2 + 2) / 4
  local B4 = AC4 * (X3 + 32768) / 32768
  local B7 = (UP - B3) * (50000/2 ^ oss)
  p = (B7 / B4) * 2
  X1 = (p / 256) * (p / 256)
  X1 = (X1 * 3038) / 65536
  X2 = (-7357 * p) / 65536
  p = p +(X1 + X2 + 3791) / 16
  return (p)
end

-- read temperature and pressure from BMP180
-- oss: oversampling setting. 0-3
function M.read(oss)
  if (oss == nil) then
     oss = 0
  end
  if (not init) then
     print("init() must be called before read.")
  else  
     read_temp()
     read_pressure(oss)
  end
end;

-- get temperature
function M.getTemperature()
  return t
end

-- get pressure
function M.getPressure()
  return p
end

return M
*/
