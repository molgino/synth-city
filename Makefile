CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
SAN ?= 0
ifeq ($(SAN),1)
CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1
endif

test: tests/unit
	./tests/unit

tests/unit: tests/unit.cpp src/*.h
	$(CXX) $(CXXFLAGS) -o $@ tests/unit.cpp

clean:
	rm -f tests/unit

.PHONY: test clean
