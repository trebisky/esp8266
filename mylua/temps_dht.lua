PIN = 2
delay = 3000

dht22 = require("dht22")

tmr.alarm(0, delay, 1, function() 
    local tc, h
    dht22.read(PIN)
    tci = dht22.getTemperature()
    hi = dht22.getHumidity()

    tc = tci / 10
    tf = 32 + (9 * tc) / 5
    h = hi / 10

    if ( tc > 100 ) then
	print ( "??" )
    else
	print ( "Temperature (C): " .. tc )
	print ( "Temperature (F): " .. tf )
	print ( "       Humidity: " .. h )
    end
end )
