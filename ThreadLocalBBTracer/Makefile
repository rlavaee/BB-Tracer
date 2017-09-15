LEVEL = ..
DYLIB_NAME = TL_BB_tracer_rt
CXX= g++

include $(LEVEL)/Makefile.common


# Build and install this archive.                                                                                                            
CXXFLAGS=-O2 -fPIC -std=c++11  -Wall -I$(BOOST_INCDIR) -I$(LEVEL)/include

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $< -o $@

all: tracer.o
	$(CXX) -shared  $(CXXFLAGS) $^ -o $(LEVEL)/lib/$(DYLIB_FILENAME)  $(LDFLAGS)
	ar cr  $(LEVEL)/lib/$(ARCH_FILENAME) $^
	ranlib $(LEVEL)/lib/$(ARCH_FILENAME)
clean:
	rm -f $(LEVEL)/lib/$(DYLIB_FILENAME) $(LEVEL)/lib/$(ARCH_FILENAME)  *.o

