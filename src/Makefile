all:
	g++ http.cc -o server /usr/local/lib/libuv.a
	g++ -g -I ../include mem_pool.cc -o mem_pool /usr/local/lib/libuv.a

clean:
	rm server
	rm mem_pool