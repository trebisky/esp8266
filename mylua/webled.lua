led1 = 0
led2 = 4
gpio.mode(led1, gpio.OUTPUT)
gpio.mode(led2, gpio.OUTPUT)

gpio.write(led1, gpio.HIGH);
gpio.write(led2, gpio.HIGH);

srv=net.createServer(net.TCP)
srv:listen(80,function(conn)
    conn:on("receive", function(client,request)
        local _, _, method, path, vars = string.find(request, "([A-Z]+) (.+)?(.+) HTTP");
        if(method == nil)then
            _, _, method, path = string.find(request, "([A-Z]+) (.+) HTTP");
        end
        local _GET = {}
        if (vars ~= nil)then
            for k, v in string.gmatch(vars, "(%w+)=(%w+)&*") do
                _GET[k] = v
            end
        end

        local buf = "";
        buf = buf.."<h1> ESP8266 Web Server</h1>";
        buf = buf.."<p>LED 0 <a href=\"?pin=ON1\"><button>ON</button></a>&nbsp;<a href=\"?pin=OFF1\"><button>OFF</button></a></p>";
        buf = buf.."<p>LED 4 <a href=\"?pin=ON2\"><button>ON</button></a>&nbsp;<a href=\"?pin=OFF2\"><button>OFF</button></a></p>";

        if(_GET.pin == "ON1")then
            gpio.write(led1, gpio.LOW);
        elseif(_GET.pin == "OFF1")then
            gpio.write(led1, gpio.HIGH);
        elseif(_GET.pin == "ON2")then
            gpio.write(led2, gpio.LOW);
        elseif(_GET.pin == "OFF2")then
            gpio.write(led2, gpio.HIGH);
        end

        client:send(buf);
        client:close();
        collectgarbage();
    end)
end)
