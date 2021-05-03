# esptool is python, so we don't really need a makefile,
# I keep this just to support "make install"

install: esptool
	cp esptool /home/tom/bin

#PORT           = /dev/ttyUSB0
PORT            = /dev/ttyUSB1

# This is a good way to verify that the boot loader on the ESP8266 is running
info:
	./esptool -p $(PORT) read_mac
	./esptool -p $(PORT) flash_id


		


