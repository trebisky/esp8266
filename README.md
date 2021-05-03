Here are a variety of ESP8266 related things.

1. NoSDK - some experiments in C programming directly on the bootrom
2. Projects - my experiments and projects are here.
3. reverse - reverse engineering, especially the bootrom
4. zzlua - dead end work with NodeMCU and Lua.
5. esptool - the ancient python esptool that I still use and hack on

I have put a lot of energy into disassembling and annotating the ESP8266 bootrom.

People have found my tcp_server code useful:

    Projects/misc/tcp_server.c

The Bootrom disassembly is here:

    reverse/bootrom/boot.txt

In April of 2021, I began some more work with the bootrom disassembly and realized that
I had pushed a badly messed up copy of boot.txt back in 2018 when I used my new disassembler.
That is fixed now, and my apologies to anyone who was inconvenienced over those 3 years.
