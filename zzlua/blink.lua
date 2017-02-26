-- pin 0 is the light on the big board.
-- pin 4 is the light on the little board.
-- pin = 4
gpio.mode(pin, gpio.OUTPUT)
led_state = gpio.LOW
delay = 200

tmr.alarm(0, delay, 1, function() 
    if ( led_state == gpio.HIGH ) then
	led_state = gpio.LOW
    else
	led_state = gpio.HIGH
    end
    gpio.write(pin, led_state)
end )
