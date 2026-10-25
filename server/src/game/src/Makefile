CC ?= gcc
CXX ?= g++

GAME_VERSION := $(shell cat ../../__REVISION__ 2>/dev/null || echo "1")
LBITS := $(shell getconf LONG_BIT)

GccMajorVersion := $(shell expr `$(CXX) -dumpversion | cut -f1 -d.`)
GccMinorVersion := $(shell expr `$(CXX) -dumpversion | cut -f2 -d.`)
GccMinorEGT8 := $(shell expr $(GccMinorVersion) \>= 8)

INCDIR =
LIBDIR =
BINDIR = ..

## LIST OF CONSTANTS BEGIN
ENABLE_GOOGLE_TEST = 0
ENABLE_LUA_5_VERSION = 5
ENABLE_GCC_AUTODEPEND = 1
ENABLE_STATIC = 0
## LIST OF CONSTANTS END

OBJDIR = .obj_lua55
MAIN_TARGET ?= $(BINDIR)/game_r$(GAME_VERSION)

$(shell if [ ! -d $(OBJDIR) ]; then mkdir $(OBJDIR); fi)

# Depend Path File
ifneq ($(ENABLE_GCC_AUTODEPEND), 1)
DEPFILE = Depend
endif

# Standard Libraries
LIBS = -lm -lmd

# Project Flags
CFLAGS  = -g -Wall -Wextra -O2 -pipe -fexceptions -fno-strict-aliasing -pthread -D_THREAD_SAFE -DNDEBUG

CXXFLAGS = -std=c++20

# for gcc
ifneq '' '$(findstring g++,$(CXX))'
ifneq ($(wildcard /usr/local/lib/gcc14),)
CFLAGS += -Wl,-rpath=/usr/local/lib/gcc14
CXXFLAGS += -Wl,-rpath=/usr/local/lib/gcc14
endif
endif

# Database client libraries (MariaDB Connector/C)
INCDIR += -I/usr/local/include/mysql -I/usr/local/include/mariadb -I/usr/local/include
LIBDIR += -L/usr/local/lib/mysql -L/usr/local/lib/mariadb -L/usr/local/lib
LIBS += -lmariadb -lz

# Runtime library search paths (rpath)
ifneq ($(wildcard /usr/local/lib/gcc14),)
LIBDIR += -Wl,-rpath=/usr/local/lib/gcc14
endif
ifneq ($(wildcard /usr/local/lib/mysql),)
LIBDIR += -Wl,-rpath=/usr/local/lib/mysql
endif
ifneq ($(wildcard /usr/local/lib/mariadb),)
LIBDIR += -Wl,-rpath=/usr/local/lib/mariadb
endif

ifeq ($(ENABLE_STATIC), 1)
CFLAGS += -static
endif

# FreeBSD stack protector
CFLAGS += -fstack-protector-all

# Version defines
CFLAGS += -D__USER__=\"$(USER)\" -D__HOSTNAME__=\"$(HOSTNAME)\" -D__PWD__=\"$(PWD)\" -D__GAME_VERSION__=\"$(GAME_VERSION)\"

# Boost
INCDIR += -I"../../../../extern/common/include/boost"

# DevIL
INCDIR += -I"../../../../extern/server/include"
ifeq ($(LBITS),64)
ifneq ($(wildcard /usr/local/lib/libIL.a),)
LIBS += /usr/local/lib/libIL.a
else
LIBS += -lIL
endif
else
LIBS += $(firstword $(wildcard ../../../../extern/server/lib/libIL.a /usr/local/lib/libIL.a -lIL))
endif

# CryptoPP
ifeq ($(LBITS),64)
ifneq ($(wildcard ../../../../extern/server/cryptopp/libcryptopp.a),)
LIBS += ../../../../extern/server/cryptopp/libcryptopp.a
else ifneq ($(wildcard ../../../../extern/server/lib/libcryptopp.a),)
LIBS += ../../../../extern/server/lib/libcryptopp.a
else ifneq ($(wildcard /usr/local/lib/libcryptopp.a),)
LIBS += /usr/local/lib/libcryptopp.a
ifneq ($(wildcard /lib/libc++.so.1 /usr/lib/libc++.so),)
LIBS += -lc++
endif
else
LIBS += -lcryptopp
ifneq ($(wildcard /lib/libc++.so.1 /usr/lib/libc++.so),)
LIBS += -lc++
endif
endif
else
LIBS += $(firstword $(wildcard ../../../../extern/server/lib/libcryptopp.a /usr/local/lib/libcryptopp.a -lcryptopp))
endif

# GTest
ifeq ($(ENABLE_GOOGLE_TEST), 1)
LIBS += /usr/local/lib/libgtest.a
CFLAGS += -DENABLE_GOOGLE_TEST
endif

# OpenSSL
INCDIR += -I/usr/include
LIBS += -lssl -lcrypto

# Lua 5.5.1
CFLAGS += -DENABLE_LUA_5_VERSION=$(ENABLE_LUA_5_VERSION)
INCDIR += -I../../liblua/5.5/src
LIBDIR += -L../../liblua/5.5/src
LIBS += ../../liblua/5.5/src/liblua.a

# Project Libraries
INCDIR += -I/usr/local/include
INCDIR += -I../../../../extern/common/include -I../../../../extern/server/include
INCDIR += -I../../libpoly
INCDIR += -I../../libgame/include
INCDIR += -I../../libthecore/include
INCDIR += -I../../libthecore/src
INCDIR += -I../../libsql
INCDIR += -I../../common
INCDIR += -I../..
INCDIR += -I.
LIBDIR += -L/usr/local/lib

LIBDIR += -L../../libthecore/lib -L../../libpoly -L../../libsql -L../../libgame/lib
LIBS += -lthecore -lpoly -lsql -lgame

# AUTO_DETECT_SOURCE_FILES BEGIN
# Automatyczne wykrycie wszystkich plików .cpp z wyłączeniem main.cpp
CPPFILE = $(filter-out main.cpp, $(wildcard *.cpp))

# Automatyczne wykrycie wszystkich plików .c
CFILE = $(wildcard *.c)

# main.cpp jest traktowany osobno
MAINCPP = main.cpp
# AUTO_DETECT_SOURCE_FILES END

# PROJECT_OBJ_FILES BEGIN
COBJS	= $(CFILE:%.c=$(OBJDIR)/%.o)
CPPOBJS	= $(CPPFILE:%.cpp=$(OBJDIR)/%.o)
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
CDEPS	= $(COBJS:%.o=%.d)
CPPDEPS	= $(CPPOBJS:%.o=%.d)
endif

MAINOBJ = $(MAINCPP:%.cpp=$(OBJDIR)/%.o)
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
MAINDEPS = $(MAINOBJ:%.o=%.d)
endif

# PROJECT_OBJ_FILES END

# Target Paths
default: $(MAIN_TARGET)

$(OBJDIR)/%.o: %.c
	@echo compiling $<
	@$(CC) $(CFLAGS) $(INCDIR) -c $< -o $@
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
	@$(CC) -MM -MG -MP $(CFLAGS) $(INCDIR) -c $< -o $(OBJDIR)/$*.d
	@sed -i '' -e's/$*.o:/$(OBJDIR)\/$*.o:/g' $(OBJDIR)/$*.d
endif

$(OBJDIR)/%.o: %.cpp
	@echo compiling $<
	@$(CXX) $(CFLAGS) $(CXXFLAGS) $(INCDIR) -c $< -o $@
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
	@$(CXX) -MM -MG -MP $(CFLAGS) $(CXXFLAGS) $(INCDIR) -c $< -o $(OBJDIR)/$*.d
	@sed -i '' -e's/$*.o:/$(OBJDIR)\/$*.o:/g' $(OBJDIR)/$*.d
endif

limit_time:
	@echo update limit time
	@python update_limit_time.py

$(MAIN_TARGET): $(CPPOBJS) $(COBJS) $(MAINOBJ)
	@echo linking $(MAIN_TARGET)
	@$(CXX) $(CFLAGS) $(CXXFLAGS) $(LIBDIR) $(COBJS) $(CPPOBJS) $(MAINOBJ) $(LIBS) -o $(MAIN_TARGET)

symlink:
	@ln -fs game_r$(GAME_VERSION) $(BINDIR)/game_symlink

strip:
	@cp $(MAIN_TARGET) $(BINDIR)/game_r
	@strip $(BINDIR)/game_r

clean:
	@echo cleaning $(MAIN_TARGET) $(OBJDIR)
	@rm -f $(COBJS) $(CPPOBJS) $(MAINOBJ)
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
	@rm -f $(CDEPS) $(CPPDEPS) $(MAINDEPS)
endif
	@rm -f $(MAIN_TARGET)

clean_all:
	@echo cleaning all game objects and targets
	@rm -rf .obj .obj_lua55
	@rm -f $(BINDIR)/game_r*

dep:
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
	@echo "Note: gcc autodepend is autodetected, so target dep skipped"
else
	makedepend -f $(DEPFILE) $(INCDIR) -I/usr/include/c++/3.3 -I/usr/include/c++/4.2 -p$(OBJDIR)/ $(CPPFILE) $(CFILE) $(MAINCPP) 2> /dev/null > $(DEPFILE)
endif

# Dodatkowy target do wyświetlenia wykrytych plików (do debugowania)
show-files:
	@echo "Detected .c files:"
	@echo $(CFILE)
	@echo "Detected .cpp files (excluding main.cpp):"
	@echo $(CPPFILE)
	@echo "Main file:"
	@echo $(MAINCPP)
	@echo "Object files to be created:"
	@echo $(COBJS) $(CPPOBJS) $(MAINOBJ)

# AUTO_DEPEND_CHECK BEGIN
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
sinclude $(CDEPS)
sinclude $(CPPDEPS)
sinclude $(MAINDEPS)
else
sinclude $(DEPFILE)
endif
# AUTO_DEPEND_CHECK END