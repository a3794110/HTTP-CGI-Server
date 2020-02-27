CXX = g++
CFLAGS = --std=c++14
webserver: 
	$(CXX) $(CFLAGS) -o webserver main.cpp
clean:
	rm webserver