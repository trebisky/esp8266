# Makefile for ESP8266 development
# Tom Trebisky  12-26-2015

# tjt - be verbose
V = 1

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

# The new python job
ESPTOOL		= esptool
PORT		= /dev/ttyUSB0

# base directory of the ESP8266 SDK package, absolute
##SDK_BASE	?= /opt/Espressif/sdk/
SDK_BASE	= /opt/esp-open-sdk

# Base directory for the compiler
SDK_BIN = $(SDK_BASE)/xtensa-lx106-elf/bin

# select which tools to use as compiler, librarian and linker
#CC		:= $(SDK_BIN)/xtensa-lx106-elf-gcc
#AR		:= $(SDK_BIN)/xtensa-lx106-elf-ar
#LD		:= $(SDK_BIN)/xtensa-lx106-elf-gcc
CC		= xtensa-lx106-elf-gcc
AR		= xtensa-lx106-elf-ar
LD		= xtensa-lx106-elf-gcc

# various paths from the SDK used in this project
SDK_LIBDIR	= $(SDK_BASE)/sdk/lib
SDK_INCDIR	= $(SDK_BASE)/sdk/include

# linker script used for the linker step
# /home/user/ESP8266/esp-open-sdk/esp_iot_sdk_v1.4.0/ld/eagle.app.v6.ld
# /home/user/ESP8266/esp-open-sdk/xtensa-lx106-elf/xtensa-lx106-elf/sysroot/usr/lib/eagle.app.v6.ld
LD_SCRIPT	= $(SDK_BASE)/sdk/ld/eagle.app.v6.ld

# contents of /home/user/ESP8266/esp-open-sdk/esp_iot_sdk_v1.4.0/lib
# libat.a  libcrypto.a  libespnow.a  libjson.a  liblwip_536.a  liblwip.a  libmain.a  libmesh.a  libnet80211.a  libphy.a  libpp.a  libpwm.a  libsmartconfig.a  libssl.a  libupgrade.a  libwpa.a  libwps.a

# libraries used in this project, mainly provided by the SDK (with 1.4.0)
#LIBS		= c gcc hal pp phy net80211 lwip wpa main
LIBS		= c gcc hal pp phy net80211 lwip wpa main crypto
LIBS		:= $(addprefix -l,$(LIBS))

# compiler includes
INCLUDES = -I. -I$(SDK_INCDIR)

# compiler flags
CFLAGS		= -Os -g -O2 -Wpointer-arith -Wundef -Werror -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals  -D__ets__ -DICACHE_FLASH

# linker flags
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static

.PHONY: all flash clean info term

TARGET	= dog

all: $(TARGET)

.c.o:
	$(vecho) "CC $<"
	$(Q) $(CC) $(INCLUDES) $(CFLAGS)  -c $<

#dog: dog.o
#	$(vecho) "LD $@"
#	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT) $(LDFLAGS) -Wl,--start-group $(LIBS) dog.o -Wl,--end-group -o $@

# linker flags
#XLDFLAGS		= -nostdlib -Wl,--no-check-sections -u user_init -Wl,-static

#XLD_SCRIPT	= $(SDK_BASE)/sdk/ld/eagle.first.v6.ld

#XLIBS		= c gcc hal pp phy net80211 lwip wpa main crypto
#XLIBS		= c gcc
XLIBS		= gcc
XLIBS		:= $(addprefix -l,$(XLIBS))

dog: dog.o
	$(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT) $(LDFLAGS) -Wl,--start-group $(XLIBS) dog.o -Wl,--end-group -o $@

# This is important -- without the right options it works sometimes,
# but other times screws up.
# Flash options - anything with a 12E module will be dio
# dio/8m works for my unit #1
#FLOPS = -fm dio -fs 4m
FLOPS = -fm dio -fs 8m

# This loads our code into the flash on the device itself
flash:  $(TARGET)
	$(ESPTOOL) elf2image $(TARGET)
	-$(ESPTOOL) --port $(PORT) write_flash $(FLOPS) 0x00000 $(TARGET)-0x00000.bin 0x40000 $(TARGET)-0x40000.bin

# This is a good way to verify that the boot loader on the ESP8266 is running
info:
	esptool -p $(PORT) read_mac
	esptool -p $(PORT) flash_id

# Fetch the boot loader (not generally useful)
bootrom.bin:
	#esptool -p $(PORT) dump_mem 0x40000000 65536 esp8266_rom.bin
	esptool -p $(PORT) dump_mem 0x40000000 65536 bootrom.bin

#bootrom.dis:
#	xtensa-lx106-elf-objdump -D -b binary -mxtensa bootrom.bin >bootrom.dis

dis:
	xtensa-lx106-elf-objdump -D -mxtensa dog.o >dog.dis

syms:
	xtensa-lx106-elf-nm dog | sort >dog.syms

term:
	picocom -b 115200 $(PORT)

clean:
	$(Q) rm -f $(TARGET)
	$(Q) rm -f *.o
	$(Q) rm -f *.bin
