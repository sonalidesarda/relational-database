Verano: Verano.o
	g++ -L/usr/local/db6/lib -o $@ $< -ldb_cxx -lsqlparser

Verano.o : Verano.cpp
	g++ -I/usr/local/db6/include -DHAVE_CXX_STDHEADERS -D_GNU_SOURCE -D_REENTRANT -O3 -std=c++11 -c -o $@ $<

test: test.o heap_storage.o 
	  g++ -L/usr/local/db6/lib -g -o $@ $^ -ldb_cxx -lsqlparser

test.o: heap_storage_test.cpp
		g++ -I/usr/local/db6/include -DHAVE_CXX_STDHEADERS -D_GNU_SOURCE -D_REENTRANT -O3 -std=c++11 -c -g -o $@ $<

heap_storage.o : heap_storage.cpp heap_storage.h storage_engine.h
			     g++ -I/usr/local/db6/include -DHAVE_CXX_STDHEADERS -D_GNU_SOURCE -D_REENTRANT -O3 -std=c++11 -c -g -o $@ $<

cleantest:
	rm -f test.o test 
	
clean : 
	rm -f Verano.o Verano
