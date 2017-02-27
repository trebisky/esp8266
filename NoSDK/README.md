The idea here is to write C code without the SDK at all and load it on top
of the bootrom.

This yields extremely small executables that load in a fraction of a second.

Clearly this would not work for anything that needed wireless networking,
but if you wanted to just use the ESP8266 as a little controller, it could
be just the thing!  This deserves more attention.

1. baud -- Linux utility to set unusual baud rates
2. first -- a variety of "bare metal" experiments
