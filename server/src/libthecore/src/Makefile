CC ?= gcc
CXX ?= g++
BIN_DIR = ../lib
BIN     = $(BIN_DIR)/libthecore.a
INCLUDE = -I../include -Iargon2 -Iargon2/blake2
INCLUDE += -I../../../../extern/common/include -I../../../../extern/server/include
LBITS := $(shell getconf LONG_BIT)

CFLAGS  = -Wall -Wextra -O2 -pipe -g $(INCLUDE)
CXXFLAGS = $(CFLAGS) -std=c++20
CCFLAGS  = $(CFLAGS) -std=c11

LIBS    =

CXX_OBJFILES = socket.o fdwatch.o buffer.o signal.o log.o utils.o \
	kstbl.o hangul.o heart.o main.o tea.o des.o gost.o memcpy.o DES_table.o \
	auth_security.o

C_OBJFILES = argon2/argon2.o argon2/core.o argon2/encoding.o \
	argon2/opt.o argon2/thread.o argon2/blake2/blake2b.o

OBJFILES = $(CXX_OBJFILES) $(C_OBJFILES)

default:
	$(MAKE) $(BIN)

$(BIN): $(OBJFILES)
	if [ ! -d $(BIN_DIR) ]; then mkdir $(BIN_DIR); fi
	ar cru $(BIN) $(OBJFILES) $(LIBS)
	ranlib $(BIN)
	chmod 700 $(BIN)

clean:
	rm -f *.o argon2/*.o argon2/blake2/*.o
	rm -f $(BIN)

dep:
	$(CXX) $(CXXFLAGS) -MM *.c *.cpp > Depend

auth_security.o: auth_security.cpp ../include/auth_security.h ../include/argon2.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(CXX_OBJFILES):
	$(CXX) $(CXXFLAGS) -c $< -o $@

argon2/%.o: argon2/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

argon2/blake2/%.o: argon2/blake2/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

memcpy: memcpy.o utils.o log.o
	$(CXX) $(CXXFLAGS) -c -D__MAIN__ memcpy.c
	$(CXX) $(CXXFLAGS) -o memcpy memcpy.o utils.o log.o

sinclude Depend
