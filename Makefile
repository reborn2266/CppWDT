all:
	g++ -O2 -c -fPIC wdt.cpp
	g++ -shared -o libwdt.so wdt.o -lpthread
