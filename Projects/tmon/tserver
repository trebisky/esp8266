#!/usr/bin/ruby

# Ruby server to absorb temperature data
# Tom Trebisky  4-13-2016
# Enhanced with select and timer 3-21-2020

# Data this logs will look like this:
# 12-03-2016 10:24:01 18 0 347 109 516
# 12-03-2016 10:25:00 18 0 338 112 521
# 12-03-2016 10:25:59 17 0 331 112 521
# 12-03-2016 10:26:59 18 0 326 112 521
# 12-03-2016 10:27:58 18 0 325 111 519

# The first two columns are date and time
# Then we have a "delay" since the last reading.
# Then an always zero that used to mean something.
# Then: humidity, temp-C, temp-F
#  these values are all *10 so 519 is really 51.9 degrees

require 'socket'
require 'timeout'

myport = 2001
$tmo = 10.0

# We expect data every 60 seconds.
# If this many seconds goes by without data, we squawk.
max_stale = 5 * 60.0

# We don't want a flood of messages when the battery goes dead
$msg_interval = 60.0
$msg_wait = 0.0

$stale = 0.0

server = TCPServer.open myport
puts "Listening on port #{myport}"
#puts server
STDOUT.flush


# get a timestamp
def get_ts
    #td = Time.now.to_a
    s,m,h,d,mo,y,wd,yd,dst,z = Time.now.to_a

    hh = "%02d" % h
    mm = "%02d" % m
    ss = "%02d" % s

    mom = "%02d" % mo
    dd = "%02d" % d

    hms = hh + ":" + mm + ":" + ss
    mdy = mom + "-" + dd + "-" + y.to_s
    return mdy + " " + hms
end

# this just logs exactly the line received with
# a timestamp prepended.
# The read timeout does work, but is rarely useful.
# It would tell us if a connection happened, but no
# data was provided in a timely manner.
def get_stuff ( x )
    temps = nil
    begin
	ts = get_ts
        Timeout.timeout(2.0) {
	    # client.puts "blah blah"
	    temps = x.gets
        }
	print ts + " " + temps
        $stale = 0.0
    rescue Timeout::Error
	print ts + " Read timed out\n"
    end
    STDOUT.flush
end

def dead_batt
    if $msg_wait < $tmo
      puts "Battery DEAD"
      $msg_wait = $msg_interval
    else
      $msg_wait -= $tmo
    end
end

# The idea here is to wrap our accept with a select call so
# we have a timer tick going to provide periodic activation
# rather than just blocking forever in accept, and we can
# note how stale our data is and throw a fit if it gets too
# stale (meaning the battery in the ESP unit went dead).
loop {
    # select args: R W E timeout
    # returns an array of arrays
    stat = select [server], nil, nil, $tmo
    if stat
      #puts "Connection"
      #puts stat
      rs, ws, es = stat
      ss = rs[0]
      #puts ss
      #if ss == server
      #  puts "Connection, looks good"
      #else
      #  puts "Connection, looks WEIRD"
      #end
#      x = server.accept()
      x = ss.accept()
      get_stuff x
      x.close
    else
      $stale += $tmo
      if $stale > max_stale
        dead_batt
      end
    end
}

# THE END
