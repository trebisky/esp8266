#!/usr/bin/ruby
# $Id: MMTsocket.rb,v 1.21 2012/05/23 22:10:48 tom Exp $

# MMTsocket.rb

require 'socket'
require 'timeout'

# Here is some captured output from the f/5 hexapod crate while at
# Sunnyside on 3/13/2003.  Only the version string is changed to
# provide a clue for the innocent.

Hexsim = %q(
version SIMULATED, Compiled: Wed Mar 12 11:58:41 MST 2003
pod_names fbeacd
matrix f5_vertex
reply_s The Lights are on
reply_q But nobody is home
cell_reply Eat more fish!
pod_status 0000
cell_status 0000
status_1 00
status_f 00
status_2 00
status_b 00
status_3 00
status_e 00
status_4 00
status_a 00
status_5 00
status_c 00
status_6 00
status_d 00
lvdt_1 -26380
lvdt_f -26380
lvdt_2 -25645
lvdt_b -25645
lvdt_3 -26382
lvdt_e -26382
lvdt_4 -26794
lvdt_a -26794
lvdt_5 -26622
lvdt_c -26622
lvdt_6 -26684
lvdt_d -26684
encoder_1 -26380
encoder_f -26380
encoder_2 -25645
encoder_b -25645
encoder_3 -26382
encoder_e -26382
encoder_4 -26794
encoder_a -26794
encoder_5 -26622
encoder_c -26622
encoder_6 -26684
encoder_d -26684
command_1 -26380
command_f -26380
command_2 -25645
command_b -25645
command_3 -26382
command_e -26382
command_4 -26794
command_a -26794
command_5 -26622
command_c -26622
command_6 -26684
command_d -26684
error_1 0
error_f 0
error_2 0
error_b 0
error_3 0
error_e 0
error_4 0
error_a 0
error_5 0
error_c 0
error_6 0
error_d 0
curxyz_x 0
curxyz_y 0
curxyz_z 0
curxyz_tz 0
curxyz_ty 0
curxyz_tx 0
cmdxyz_x 0
cmdxyz_y 0
cmdxyz_z 0
cmdxyz_tz 0
cmdxyz_ty 0
cmdxyz_tx 0
errxyz_x 0
errxyz_y 0
errxyz_z 0
errxyz_tz 0
errxyz_ty 0
errxyz_tx 0
force_1 0
force_2 0
force_3 0
force_4 0
force_5 0
force_6 0
cellpos_1 0
cellpos_2 0
cellpos_3 0
cellpos_4 0
cellpos_5 0
cellpos_6 0
sensor_1 0
sensor_2 0
sensor_3 0
sensor_4 0
sensor_5 0
)

class MMTsocket
	attr_reader	:status

	@@timeout = 1.5
	@@simulate = nil

	@@mount_crate = "mount"
	@@mount_port = 5240

	# This is now hexapod linux running on hacksaw
	@@hexapod_crate = "hexapod"
	@@hexapod_port = 5340

	def initialize ( host, port, timeout=nil )
	    @sock = nil
	    @status = nil

	    @simulate = @@simulate
	    return if @simulate

	    timeout = @@timeout unless timeout
	    # I can only call it a ruby bug, but sometimes this
	    # gets:
	    #     /usr/lib/ruby/1.8/timeout.rb:60:in `timeout': execution expired
	    #     from /mmt/scripts/MMTsocket.rb:136:in `initialize
	    # we could trap the string "execution expired", but we certainly
	    # should not need to ....

	#    begin
	#	if timeout > 0
	#	    timeout ( timeout ) {
	#		@sock = TCPSocket.open( host, port )
	#	    }
	#	else
	#	    @sock = TCPSocket.open( host, port )
	#	end
	#    rescue Timeout::Error
	#	@status = "Timeout"
	#    rescue Errno::ECONNREFUSED
	#	@status = "Refusing connection"
	#    rescue => why
	#	@status = "Error: #{why}"
	#    end

	    begin
		@sock = TCPSocket.open( host, port )
	    rescue Errno::ECONNREFUSED
		@status = "Refusing connection"
	    rescue => why
		@status = "Error: #{why}"
	    end

	end

	def MMTsocket.set_mount ( crate=nil, port=nil )
	    @@mount_crate = crate if crate
	    @@mount_port = port if port
	end
	def MMTsocket.set_hexapod ( crate=nil, port=nil )
	    @@hexapod_crate = crate if crate
	    @@hexapod_port = port if port
	end

	def MMTsocket.set_timeout ( set )
	    @@timeout = set
	end
	def MMTsocket.simulate ( who = "hexapod" )
	    if who == "hexapod"
		@@simulate = Hexsim
	    else
		@@simulate = "version bogus bongos\n"
	    end
	end

	def MMTsocket.hexapod
	    MMTsocket.new( @@hexapod_crate, @@hexapod_port )
	end
	def MMTsocket.mount ( port=nil )
	    port = @@mount_port unless port
	    MMTsocket.new( @@mount_crate, port )
	end

	def MMTsocket.sim ( port=5240 )
	    MMTsocket.new( "vxsim", port )
	end
	def MMTsocket.cell ( port=5810 )
	    MMTsocket.new( "mmtcell", port )
	end
	def MMTsocket.bcell ( port=5800 )
	    MMTsocket.new( "mmtcell", port )
	end
	def MMTsocket.ecell ( port=5801 )
	    MMTsocket.new( "mmtcell", port )
	end
	def MMTsocket.bmount ( port=5220 )
	    MMTsocket.new( @@mount_crate, port )
	end
	def bsend ( code, addr )
	    return unless @sock
	    cmd = [code,addr].pack( "nn" )
	    @sock.print( cmd )
	end
	def bget ( code, addr, count )
	    return nil unless @sock
	    cmd = [code,addr].pack( "nn" )
	    @sock.print( cmd )
	    @sock.recv( count )
	end
	def brecv ( count )
	    return nil unless @sock
	    @sock.recv( count )
	end
	def MMTsocket.reboot ( host="mount" )
	    port = 5230
	    if host == "mmtcell"
	    	port = 5899
	    end
	    b = MMTsocket.new( host, port )
	    b.bsend( 123, 0 )
	    b.done
	end
	def MMTsocket.hexall
	    c = MMTsocket.hexapod
	    return nil if c.status
	    rv = c.values
	    c.done
	    rv
	end
	def MMTsocket.hextags
	    c = MMTsocket.hexapod
	    return nil if c.status
	    rv = c.tags
	    c.done
	    rv
	end
	def MMTsocket.hexcmd ( cmd )
	    c = MMTsocket.hexapod
	    return nil if c.status
	    rv = c.command( cmd )
	    c.done
	    rv = nil if rv == "OK"
	    rv
	end
	def MMTsocket.mountcmd ( cmd )
	    c = MMTsocket.mount
	    return nil if c.status
	    rv = c.command( cmd )
	    c.done
	    rv = nil if rv == "OK"
	    rv
	end
	def MMTsocket.hexapod_get ( tag )
	    c = MMTsocket.hexapod
	    return nil if c.status
	    rv = c.get( tag )
	    c.done
	    rv = nil if rv == "OK"
	    rv
	end
	def MMTsocket.mount_get ( tag )
	    c = MMTsocket.mount
	    return nil if c.status
	    rv = c.get( tag )
	    c.done
	    rv = nil if rv == "OK"
	    rv
	end
	def ident ( id="MMT" )
	    send( "@ident #{id}" )
	    check()
	end
	def done ()
	    @sock.close if @sock
	end
	def show ()
	    each { |x| print x }
	end
	def showit ( what )
	    send( what )
	    show()
	end
	def version()
	    showit( "version" )
	end
	def readline ()
	    return nil unless @sock
	    @sock.readline().chomp
	end

	# when called from command, and we send a bogus command
	# we will probably get a two line reply.
	# The hexapod would send:
	#   ? bogus
	#   .EOF
	def check ()
	    return "OK" if @simulate
	    return "IES" unless @sock
	    @sock.readline().chomp
	end
	def each ()
	    if @simulate
		@simulate.split("\n").each { |l|
		    yield l+"\n" if l != ""
		}
	    	return
	    end
	    return unless @sock
	    loop do
		begin
		    x = @sock.readline()
		rescue EOFError
		    break
		end
		break if x =~ /^.EOF/
		yield x
	    end
	end
	def dump ()	# XXX ??
	    each { |data|
		print data.length, data
	    }
	end

	# get an array with all the tag names
	def tags ( what="all" )
	    send( what )
	    r = Array.new
	    each { |l| r << l.chomp.split[0] }
	    r
	end

	# get a hash with all the values
	def values ( what="all" )
	    send( what )
	    r = Hash.new
	    each { |l|
	    	(t,v) = l.chomp.split(' ',2)
		r[t] = v
	    }
	    r
	end

	# get a single value
	def get ( tag )
	    send( "get #{tag}" )
	    rv = nil
	    each { |l|
	    	(t,v) = l.chomp.split(' ',2)
		rv = v if t == tag
	    }
	    rv
	end

	def command ( cmd, args=nil )
	    send( cmd )
	    args.each { |a| send a.to_s } if args
	    check()
	end

	def send ( msg )
	    return unless @sock
	    @sock.print( msg + "\n" )
	end

end

# THE END
