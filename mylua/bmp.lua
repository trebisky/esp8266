SDA_PIN = 2
SCL_PIN = 1
id = 0

local ADDR = 0x77 --BMP180 address
local REG_CALIBRATION = 0xAA
local REG_CONTROL = 0xF4
local REG_RESULT = 0xF6

local COMMAND_TEMPERATURE = 0x2E
local COMMAND_PRESSURE = {0x34, 0x74, 0xB4, 0xF4}

local TDELAY = 4500
local PDELAY = 7500
local OSS = 1

local function read_reg(reg_addr, length)
  i2c.start(id)
  i2c.address(id, ADDR, i2c.TRANSMITTER)
  i2c.write(id, reg_addr)
  i2c.stop(id)
  i2c.start(id)
  i2c.address(id, ADDR,i2c.RECEIVER)
  c = i2c.read(id, length)
  i2c.stop(id)
  return c
end

local function write_reg(reg_addr, reg_val)
  i2c.start(id)
  i2c.address(id, ADDR, i2c.TRANSMITTER)
  i2c.write(id, reg_addr)
  i2c.write(id, reg_val)
  i2c.stop(id)
end

-- call with index 1-11
local function read_cal ( x )
  local data = read_reg(REG_CALIBRATION + (x-1)*2, 2)
  local rv = string.byte(data, 1) * 256 + string.byte(data, 2)
  if rv > 32767 then
    rv = -(65536 - rv)
  end
  return rv
end

-- read temperature from BMP180
local function read_temp()
  write_reg(REG_CONTROL, COMMAND_TEMPERATURE)
  tmr.delay(TDELAY)
  local dataT = read_reg(REG_RESULT, 2)
  UT = string.byte(dataT, 1) * 256 + string.byte(dataT, 2)
  return(UT)
end

-- must be read after read temperature
local function read_pressure()
  write_reg(REG_CONTROL, COMMAND_PRESSURE[OSS + 1]);
  tmr.delay(PDELAY);
  local dataP = read_reg(0xF6, 3)

  UP = string.byte(dataP, 1) * 65536 + string.byte(dataP, 2) * 256 + string.byte(dataP, 3)
  UP = UP / 2 ^ (8 - OSS)

  return ( UP )
end

i2c.setup(id, SDA_PIN, SCL_PIN, i2c.SLOW)

print ( " id = " .. id )

t = read_temp()
print ( t )

t = read_pressure()
print ( t )

for i=1,11,1 do
      t = read_cal ( i )
      print ( "cal " .. i .. " = " .. t )
end
