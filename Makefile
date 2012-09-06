all:
	g++ -std=c++0x -O2 -c -fPIC wdt.cpp
	g++ -shared -o libwdt.so wdt.o -lpthread

test:
	g++ -std=c++0x -o test_wdt test_wdt.cpp -L. -lwdt

clean:
	rm -rf *.o test_wdt libwdt.so
