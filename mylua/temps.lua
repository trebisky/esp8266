OSS = 1 -- oversampling setting (0-3)
SDA_PIN = 2
SCL_PIN = 1

delay = 2000

bmp180 = require("bmp180")
bmp180.init(SDA_PIN, SCL_PIN)

tmr.alarm(0, delay, 1, function() 
    local tci, pi

    bmp180.read(OSS)

    tci = bmp180.getTemperature()
    pi = bmp180.getPressure()

    tc = tci / 10
    tf = 32 + (9 * tc) / 5
    p = pi / 10

    if ( tc > 100 ) then
	print ( "??" )
    else
	print ( "Temperature (C): " .. tc )
	print ( "Temperature (F): " .. tf )
	print ( "       Pressure: " .. p )
    end
end )

-- temperature in degrees Celsius  and Farenheit
--print("Temperature: "..(t/10).."."..(t%10).." deg C")
--print("Temperature: "..(9 * t / 50 + 32).."."..(9 * t / 5 % 10).." deg F")

-- pressure in differents units
--print("Pressure: "..(p).." Pa")
--print("Pressure: "..(p / 100).."."..(p % 100).." hPa")
--print("Pressure: "..(p / 100).."."..(p % 100).." mbar")
--print("Pressure: "..(p * 75 / 10000).."."..((p * 75 % 10000) / 1000).." mmHg")
