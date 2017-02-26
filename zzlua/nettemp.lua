SDA_PIN = 2
SCL_PIN = 1
id = 0

local ADDR = 0x77 --BMP180 address
local REG_CALIBRATION = 0xAA
local REG_CONTROL = 0xF4
local REG_RESULT = 0xF6

local COMMAND_TEMPERATURE = 0x2E

local TDELAY = 4500

--[[
-- ultra low power
local COMMAND_PRESSURE = 0x34
local PDELAY = 4500
local PDIV = 256
local OSS = 0

-- standard
local COMMAND_PRESSURE = 0x74
local PDELAY = 7500
local PDIV = 128
local OSS = 1

-- high resolution
local COMMAND_PRESSURE = 0xB4
local PDELAY = 13500
local PDIV = 64
local OSS = 2

-- ultra high resolution
local COMMAND_PRESSURE = 0xF4
local PDELAY = 25500
local PDIV = 32
local OSS = 3
--]]

local COMMAND_PRESSURE = 0x74
local PDELAY = 7500
local PDIV = 128
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
  if ( x < 4 or x > 6 ) and rv > 32767 then
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
  write_reg(REG_CONTROL, COMMAND_PRESSURE)
  tmr.delay(PDELAY)
  local dataP = read_reg(0xF6, 3)

  UP = string.byte(dataP, 1) * 65536 + string.byte(dataP, 2) * 256 + string.byte(dataP, 3)
  return ( UP/PDIV )
end

i2c.setup(id, SDA_PIN, SCL_PIN, i2c.SLOW)

srv=net.createServer(net.TCP)
srv:listen(82,function(conn)
    print ( "New connection" )
    local buf
    conn:on("receive", function(client,req)
	if ( string.sub(req,1,3) == "cal" ) then
	    local cal
	    for i=1,11,1 do
	        cal = read_cal ( i )
		client:send ( "" .. cal .. "\n" )
	    end
	elseif ( string.sub(req,1,3) == "oss" ) then
	    client:send ( "" .. oss .. "\n" )
	else
	    local t = read_temp()
	    local p = read_pressure()
	    client:send ( t .. " " ..  p .. "\n" )
	end

        collectgarbage()
    end)
end)
