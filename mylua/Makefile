# I hate remembering these long commands

TTY = /dev/ttyUSB1

wifi:
	./luatool.py -p $(TTY) -f wifi.lua

load:
	./luatool.py -p $(TTY) -f blink.lua

dht:
	./luatool.py -p $(TTY) -f dht22.lua

bmp:
	./luatool.py -p $(TTY) -f bmp.lua
	#./luatool.py -p $(TTY) -f bmp180.lua

net:
	./luatool.py -p $(TTY) -f nettemp.lua

temps:
	#./luatool.py -p $(TTY) -f temps.lua
	./luatool.py -p $(TTY) -f temps_dhtlib.lua

scan:
	./luatool.py -p $(TTY) -f scanner.lua

list:
	./luatool.py -p $(TTY) -l

nodemcu:
	esptool --port $(TTY) write_flash 0x00000 nodemcu_integer_0.9.6-dev_20150704.bin

prune:
	#./luatool.py -p $(TTY) --delete main.lua
	./luatool.py -p $(TTY) --delete blink.lua
	./luatool.py -p $(TTY) --delete door.lua
	./luatool.py -p $(TTY) --delete wifi.lua
	./luatool.py -p $(TTY) --delete webled.lua
	./luatool.py -p $(TTY) --delete dht22.lua

term:
	picocom $(TTY)
