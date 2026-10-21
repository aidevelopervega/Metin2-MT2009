CXX = clang++
CPPFLAGS=-g -std=c++20

INCLUDEDIR =
LIBDIR =

## LIST OF CONSTANTS BEGIN
ENABLE_LUA_5_VERSION = 5
ENABLE_STATIC = 1
## LIST OF CONSTANTS END

ifeq ($(ENABLE_STATIC), 1)
CPPFLAGS+= -static
endif

CPPFLAGS += -DLUA_V=551 -DENABLE_LUA_5_VERSION=5
INCLUDEDIR += -I../../../liblua/5.5/src
INCLUDEDIR += -I../../../../../extern/common/include -I../../../../../extern/common/include/boost
LIBS += ../../../liblua/5.5/src/liblua.a -lm

all: qc create_conversion

qc: qc.cc qc.o crc32.o
	$(CXX) $(CPPFLAGS) -o ./qc $(INCLUDEDIR) $(LIBDIR) qc.o crc32.o $(LIBS)

crc32.o: crc32.cc
	$(CXX) $(CPPFLAGS) -c $< $(INCLUDEDIR) $(LIBDIR)

qc.o: qc.cc
	$(CXX) $(CPPFLAGS) -c $< $(INCLUDEDIR) $(LIBDIR)

create_conversion.o: create_conversion.cc
	$(CXX) $(CPPFLAGS) -c $< $(INCLUDEDIR) $(LIBDIR)

create_conversion: crc32.o create_conversion.o
	$(CXX) -g -o ./create_conversion $(INCLUDEDIR) $(LIBDIR) create_conversion.o crc32.o $(LIBS)

clean:
	rm -f qc cc qc.o crc32.o cc.o create_conversion.o
