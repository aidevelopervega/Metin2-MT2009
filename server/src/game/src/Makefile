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
OBJDIR = .obj
$(shell if [ ! -d $(OBJDIR) ]; then mkdir $(OBJDIR); fi)

## LIST OF CONSTANTS BEGIN
ENABLE_GOOGLE_TEST = 0
ENABLE_LUA_5_VERSION = 0
ENABLE_GCC_AUTODEPEND = 1
ENABLE_STATIC = 0
## LIST OF CONSTANTS END

# Depend Path File
ifneq ($(ENABLE_GCC_AUTODEPEND), 1)
DEPFILE = Depend
endif

# Standard Libraries
LIBS = -lm -lmd

# Project Flags
CFLAGS  = -g -Wall -O2 -pipe -fexceptions -fno-strict-aliasing -pthread -D_THREAD_SAFE -DNDEBUG
ifeq ($(LBITS),32)
CFLAGS += -m32
endif
CFLAGS += -Wno-deprecated-declarations -Wno-nonnull-compare -Wno-array-bounds -Wno-address
CFLAGS += -Wno-int-in-bool-context -Wno-format-truncation -Wno-stringop-truncation -Wno-sign-compare -Wno-deprecated-enum-enum-conversion

CXXFLAGS = -std=c++20

# for clang
ifneq '' '$(findstring clang++,$(CXX))'
CFLAGS += -Wno-invalid-source-encoding -Wno-deprecated-anon-enum-enum-conversion -Wno-unknown-warning-option
# for gcc
else ifneq '' '$(findstring g++,$(CXX))'
ifneq ($(wildcard /usr/local/lib/gcc14),)
CFLAGS += -Wl,-rpath=/usr/local/lib/gcc14
CXXFLAGS += -Wl,-rpath=/usr/local/lib/gcc14
endif
endif

# Database client libraries (MySQL / MariaDB)
INCDIR += -I/usr/local/include/mysql -I/usr/local/include/mariadb -I/usr/local/include
LIBDIR += -L/usr/local/lib/mysql -L/usr/local/lib/mariadb -L/usr/local/lib
ifneq ($(wildcard /usr/local/lib/mariadb/libmariadb.so /usr/local/lib/libmariadb.so /usr/local/lib/mariadb/libmariadb.a /usr/local/lib/libmariadb.a),)
LIBS += -lmariadb -lz
else ifneq ($(wildcard /usr/local/lib/mysql/libmysqlclient.so /usr/local/lib/mysql/libmysqlclient.a /usr/local/lib/libmysqlclient.so /usr/local/lib/libmysqlclient.a),)
LIBS += -lmysqlclient -lz
else
LIBS += -lmariadb -lz
endif

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

ifeq ($(GccMinorEGT8), 1)
CFLAGS += -Wno-unused-local-typedefs
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

# Lua
CFLAGS += -DENABLE_LUA_5_VERSION=ENABLE_LUA_5_VERSION
ifeq ($(ENABLE_LUA_5_VERSION), 2)
INCDIR += -I../../liblua/5.2/install/include
LIBDIR += -L../../liblua/5.2/install/lib
LIBS += ../../liblua/5.2/install/lib/liblua.a
else
INCDIR += -I../../liblua/5.0/include
LIBDIR += -L../../liblua/5.0/lib
LIBS += ../../liblua/5.0/lib/liblua.a ../../liblua/5.0/lib/liblualib.a
endif

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
ifeq ($(LBITS),32)
LIBDIR += -L../../../../extern/server/lib
endif

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
MAIN_TARGET = $(BINDIR)/game_r$(GAME_VERSION)

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