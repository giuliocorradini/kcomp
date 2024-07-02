CXX := clang++
CXXFLAGS := -std=c++17 -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

all: kcomp

kcomp: driver.o parser.o scanner.o kcomp.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< `llvm-config --cxxflags --ldflags --libs --libfiles --system-libs`

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< `llvm-config --cxxflags`

driver.o: driver.cpp parser.hpp
	$(CXX) $(CXXFLAGS) -c $< `llvm-config --cxxflags`

parser.cpp, parser.hpp: parser.yy 
	bison -o parser.cpp parser.yy

scanner.cpp: scanner.ll
	flex -o scanner.cpp scanner.ll

.PHONY: clean all

clean:
	rm -f *~ driver.o scanner.o parser.o kcomp.o kcomp scanner.cpp parser.cpp parser.hpp
