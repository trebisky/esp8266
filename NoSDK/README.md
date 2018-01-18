The idea here is to write C code without the SDK at all and load it on top
of the bootrom.  Essentially this is bare metal programming on the ESP8266.

This yields extremely small executables that load in a fraction of a second.

Clearly this will not work for anything that needs wireless networking,
but if you just want to just use the ESP8266 as a little controller,
it is just the thing!

1. hello -- a very small simple first program that prints a message
1. hello_cpu -- the above with code added to bump cpu to 80 Mhz
2. uart  -- a bare metal uart driver
2. timer -- bare metal timer interrupts
2. timer_blink -- user timer to blink LED (576 bytes)
2. rtc -- GPIO via the rtc (aka gpio 16) and fast timer experiments
3. misc -- a variety of "bare metal" experiments
4. baud -- Linux utility to set unusual baud rates
5. uart-sdk1 -- old uart experiments with SDK
6. uart-sdk2 -- old uart experiments with SDK
