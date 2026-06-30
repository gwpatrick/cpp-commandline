CXX      := g++
CXXFLAGS := -std=c++26 -O3 -Wall
TARGETS  := commandline.o minimal_example

all: $(TARGETS)

commandline.o: commandline.cpp src/commandline.hpp src/commandline_ctor.hpp src/commandline_lib.hpp
	$(CXX) $(CXXFLAGS) -Isrc -c $< -o $@

minimal_example: minimal_example.cpp commandline.o
	$(CXX) $(CXXFLAGS)  -Isrc $^ -o $@ 

.PHONY: all install clean distclean

clean:; rm -f *.o *~
distclean: clean; rm -f $(TARGETS)
