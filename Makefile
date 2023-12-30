CXX := clang++

## TODO: set CPPFLAGS and LDFLAGS for Linux

all: kcomp

kcomp:    driver.o parser.o scanner.o kcomp.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -o kcomp driver.o parser.o scanner.o kcomp.o `llvm-config --cxxflags --ldflags --libs --libfiles --system-libs`

kcomp.o:  kcomp.cpp driver.hpp
	$(CXX) $(CPPFLAGS) -c kcomp.cpp -std=c++17 -fno-exceptions -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
	
parser.o: parser.cpp
	$(CXX) $(CPPFLAGS) -c parser.cpp -std=c++17 -fno-exceptions -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
	
scanner.o: scanner.cpp parser.hpp
	$(CXX) $(CPPFLAGS) -c scanner.cpp -std=c++17 -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS 
	
driver.o: driver.cpp parser.hpp driver.hpp
	$(CXX) $(CPPFLAGS) -c driver.cpp -std=c++17 -fno-exceptions -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS 

parser.cpp, parser.hpp: parser.yy 
	bison -o parser.cpp parser.yy

scanner.cpp: scanner.ll
	flex -o scanner.cpp scanner.ll

.PHONY: clean all

clean:
	rm -f *~ driver.o scanner.o parser.o kcomp.o kcomp scanner.cpp parser.cpp parser.hpp
