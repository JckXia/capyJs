all:
	g++ -g -I ../include mem_pool_test.cc  -o mem_pool_test /usr/local/lib/libuv.a
	g++ -I ../include mem_pool_perf.cc  -o mem_pool_perf_test /usr/local/lib/libuv.a
clean:
	rm mem_pool_perf_test
	rm mem_pool_test