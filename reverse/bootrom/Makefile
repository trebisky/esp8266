# Makefile for ESP8266 development
# Tom Trebisky  12-26-2015
# 
# Added stuff for the new "espdis" program 7-7-2018

all:
	echo "You need to be more specific"

install:
	cp dumper /home/tom/bin

#PORT		= /dev/ttyUSB2
PORT		= /dev/ttyUSB1

# This is a good way to verify that the boot loader on the ESP8266 is running
info:
	esptool -p $(PORT) read_mac
	esptool -p $(PORT) flash_id

# Fetch the boot loader (not generally useful)
bootrom.bin:
	#esptool -p $(PORT) dump_mem 0x40000000 65536 esp8266_rom.bin
	esptool -p $(PORT) dump_mem 0x40000000 65536 bootrom.bin

# This works, but is brute force and has many out of sync sections.
# I found something better online.
naive.dis:
	xtensa-lx106-elf-objdump -D -b binary -mxtensa bootrom.bin >naive.dis

#REMOTE = /www/cholla/esp8266/bootrom/boot.txt
REMOTE = /u1/www/esp8266/bootrom/boot.txt

cinstall:
	ssh cholla rm $(REMOTE)
	scp boot.txt cholla:$(REMOTE)
	ssh cholla chmod a-w $(REMOTE)

#fetch:
#	scp cholla:/www/cholla/esp8266/bootrom/boot.txt boot.cholla

# generate the naive one big thing disassembly (useless)
#naive.dis:	bootrom.elf
#	xtensa-lx106-elf-objdump -mxtensa -d bootrom.elf >naive.dis

bootrom.odx:	bootrom.bin
	odx bootrom.bin >bootrom.odx

bootrom.elf:	wrap bootrom.bin
	./wrap bootrom.bin bootrom.elf

bootsym.elf:	wrap bootrom.bin calls.sy
	./wrap bootrom.bin bootsym.elf calls.sy

showsym:	bootsym.elf
	xtensa-lx106-elf-nm bootsym.elf

# ------------------------------------------
# Run new multipass disassembler 7-7-2018

new.dis:
	./espdis all >new.dis

# This became boot.txt
#mergeit:
#	./merge_dis >boot_NEW.txt

# ------------------------------------------
# Do the following:
#
# generate the multi-section disassembly
# creates first.dis

#PORK = esp-elf-rom/bootrom.elf
#PORK = bootrom.elf
PORK = bootsym.elf

# First call check adds 31 new calls
# Second call check adds 1 new call
first:	$(PORK)
	cp Syms/rom.sy calls.sy
	./gendis $(PORK)
	./call_check | sort >new1.sy
	mv calls.sy tmp.sy
	cat tmp.sy new1.sy | sort >calls.sy
	rm new1.sy tmp.sy
	./gendis $(PORK)
	./call_check | sort >new2.sy
	mv calls.sy tmp.sy
	cat tmp.sy new2.sy | sort >calls.sy
	rm new2.sy tmp.sy
	./gendis $(PORK)
	#./call_check | sort >new3.sy

# now clean up the l32r stuff
second:	first.dis
	./loads >second.dis

clean:
	rm -f *.sy
