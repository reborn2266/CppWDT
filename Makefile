all:
	g++ -std=c++0x -O2 -c -fPIC wdt.cpp
	g++ -shared -o libwdt.so wdt.o -lpthread
	g++ -std=c++0x -o test_wdt test_wdt.cpp -L. -lwdt
	g++ -std=c++0x -o test_wdt_hanged test_wdt_hanged.cpp -L. -lwdt
	g++ -std=c++0x -o test_wdt_crash test_wdt_crash.cpp -L. -lwdt

clean:
	rm -rf *.o test_wdt test_wdt_hanged test_wdt_crash libwdt.so
