TARGET := npshell

CXX := g++
ifeq (/usr/bin/g++-11,$(wildcard /usr/bin/g++-11*))
    CXX=g++-11
endif

CXXFLAGS := -std=c++17 -Wall -Wextra -g

.PHONY: all
all: $(TARGET)

%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f *.o $(TARGET)