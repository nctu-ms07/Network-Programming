BOOST = /boost_1_77_0

CXX := g++
ifeq (/usr/bin/g++-11,$(wildcard /usr/bin/g++-11*))
    CXX=g++-11
endif

CXXFLAGS := -std=c++17 -Wall -Wextra -g

.PHONY: part1
part1: http_server console.cgi

part2: cgi_server.cpp
	$(CXX) $(CXXFLAGS) -I ${BOOST} $< -o cgi_server -lws2_32 -lwsock32

%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ -lpthread

.PHONY: clean
clean:
	rm -f *.o http_server console.cgi cgi_server.exe
