CXX := clang++
CXXFLAGS := -std=c++17
LLVM_INCLUDES := $(shell llvm-config --cxxflags)

all: kcomp

kcomp: driver.o parser.o scanner.o kcomp.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(shell llvm-config --cxxflags --ldflags --libs --libfiles --system-libs)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< $(LLVM_INCLUDES)

driver.o: driver.cpp parser.hpp
	$(CXX) $(CXXFLAGS) -c $< $(LLVM_INCLUDES)

parser.cpp, parser.hpp: parser.yy 
	bison -o parser.cpp parser.yy

scanner.cpp: scanner.ll
	flex -o scanner.cpp scanner.ll

.PHONY: clean all

clean:
	rm -f *~ driver.o scanner.o parser.o kcomp.o kcomp scanner.cpp parser.cpp parser.hpp
