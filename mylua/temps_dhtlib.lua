PIN = 6
delay = 2000

tmr.alarm(0, delay, 1, function() 
    local t, h, td, hd
    status, t,h, td,hd = dht.read(PIN)

    td = td / 100
    hd = hd / 100
    tx = t*10 + td
    tf = 320 + (9 * tx) / 5
    tfd = tf % 10
    tf = tf / 10

--    print ( "Status: " .. status )
    if status == 0 then
	print ( "Temperature (C): " .. t .. "." .. td )
	print ( "Temperature (F): " .. tf .. "." .. tfd )
--	print ( "Temperature (F): " .. tf )
	print ( "       Humidity: " .. h .. "." .. hd )
    else
	print (" ?????\n" )
    end
end )
