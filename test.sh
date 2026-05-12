# wrk -t12 -c10000 -d5m --latency http://127.0.0.1:9091/uptime
wrk -t12 -c500 -d5m --latency http://192.168.2.41:9091/uptime # Rpi configuration