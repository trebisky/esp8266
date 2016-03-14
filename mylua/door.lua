srv=net.createServer(net.TCP)
srv:listen(80,function(conn)
    conn:on("receive",function(conn,payload)
        print(node.heap())
        -- door="open"
        if gpio.read(8)==1 then door="OPEN" else door="CLOSED" end
        conn:send("<h1> The door is " .. door .."</h1>")
    end)
    conn:on("sent",function(conn) conn:close() end)
end)
