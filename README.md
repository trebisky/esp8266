Here are a variety of ESP8266 related things.

1. NoSDK - some experiments in C programming right on the bootrom
2. mysdk - my most interesting things are here.
3. reverse - reverse engineering, especially the bootrom
4. zzlua - dead end work with NodeMCU and Lua.

I have put a lot of energy into disassembling and annotating the ESP8266 bootrom.

People have found my tcp_server code useful:

    mysdk/misc/tcp_server.c

The Bootrom disassembly is here:

    reverse/bootrom/boot.txt
