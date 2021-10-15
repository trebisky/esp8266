The esp8266 tmon project

This is sort of a mini weather station that I run at my house.
It uses a RHT03 temperature/humidity sensor (a DHT-22 clone)
along with an esp8266 module and powered by a pair of 18650 batteries.

The battery pair are in parallel and were recovered from an
old laptop.  Now that I have a low dropout regulator to
derive 3.3 volts from them, the thing runs for months
before I need to swap the battery.  I also sleep between
measuring temperatures once a minute.

The latest thing here is the GUI, which is written in Python
and displays that last day, or 2 days, or 3 days or whatever
along with a variety of exciting pieces of data (such as
current temperature, sunrise/sunset times, battery voltage.
