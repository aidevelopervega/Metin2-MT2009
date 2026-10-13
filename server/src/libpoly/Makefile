CXX ?= g++
BIN = ./libpoly.a
LBITS := $(shell getconf LONG_BIT)

INCLUDE += -I../../../extern/common/include -I../../../extern/server/include
LIBS =

CFLAGS  = -Wall -O2 -pipe -fno-exceptions -fno-rtti $(INCLUDE)
ifeq ($(LBITS),32)
CFLAGS += -m32
endif
CFLAGS += -std=c++20

OBJFILES = Base.o Poly.o SymTable.o Symbol.o

default:
	$(MAKE) $(BIN)

$(BIN): $(OBJFILES)
	ar cru $(BIN) $(OBJFILES) $(LIBS)
	ranlib $(BIN)
	chmod 700 $(BIN)

clean:
	rm -f *.o
	rm -f $(BIN)

dep:
	touch Depend
	$(CXX) $(CFLAGS) -MM *.cc > Depend

$(OBJFILES):
	$(CXX) $(CFLAGS) -c $<

include Depend

