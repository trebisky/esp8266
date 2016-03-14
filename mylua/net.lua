
srv=net.createServer(net.TCP)
srv:listen(82,function(conn)
    print ( "New connection" )
    conn:on("receive", function(client,request)
	print ( request )

        client:send("dog\n\r");
--        client:close();
        collectgarbage();
    end)
end)
