CC ?= gcc
CXX ?= g++

DB_VERSION = $(shell cat ../../__REVISION__ 2>/dev/null || echo "1")
LBITS := $(shell getconf LONG_BIT)

INCDIR =
LIBDIR =
BINDIR = ..
OBJDIR = .obj
$(shell if [ ! -d $(OBJDIR) ]; then mkdir $(OBJDIR); fi)

# LIST_OF_CONSTANTS BEGIN
ENABLE_GCC_AUTODEPEND = 1
ENABLE_STATIC = 0
# LIST_OF_CONSTANTS END

# Depend Path File
ifneq ($(ENABLE_GCC_AUTODEPEND), 1)
DEPFILE = Depend
endif

# Project Flags
CFLAGS  = -g -Wall -O2 -pipe -fno-rtti -fno-exceptions -pthread -D_THREAD_SAFE
CFLAGS += -Wno-deprecated-declarations -Wno-array-bounds -Wno-address
CFLAGS += -Wno-format-truncation -Wno-deprecated-enum-enum-conversion
CFLAGS += -w
CXXFLAGS = -std=c++20

# Database client libraries
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

# Version defines
CFLAGS += -D__USER__=\"$(USER)\" -D__HOSTNAME__=\"$(HOSTNAME)\" -D__PWD__=\"$(PWD)\" -D__DB_VERSION__=\"$(DB_VERSION)\"

# Project Libraries
INCDIR += -I/usr/local/include
INCDIR += -I../../../Extern/include
INCDIR += -I../../../../server/src/libpoly -I../../libpoly
INCDIR += -I../../../../server/src/libgame/include -I../../libgame/include
LIBDIR += -L../../../Extern/lib

LIBDIR += -L../../libthecore/lib -L../../libsql -L../../libpoly -L../../../../server/src/libpoly -L../../libgame/lib -L../../../../server/src/libgame/lib -L../../../../../tests/build -L../../../tests/build
LIBS += -lthecore -lsql -lpoly -lgame -lm

# OpenSSL
LIBS += -lssl -lcrypto

# AUTO_DETECT_SOURCE_FILES BEGIN
# Automatyczne wykrycie wszystkich plików .cpp w bieżącym folderze
CPPFILE = $(wildcard *.cpp)
# AUTO_DETECT_SOURCE_FILES END

# PROJECT_OBJ_FILES BEGIN
CPPOBJS = $(CPPFILE:%.cpp=$(OBJDIR)/%.o)
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
CPPDEPS	= $(CPPOBJS:%.o=%.d)
endif
# PROJECT_OBJ_FILES END

# Target Paths
MAIN_TARGET = $(BINDIR)/db_r$(DB_VERSION)

default: $(MAIN_TARGET)

$(MAIN_TARGET): $(CPPOBJS)
	@echo linking $(MAIN_TARGET)
	@$(CXX) $(CFLAGS) $(CXXFLAGS) $(LIBDIR) $(CPPOBJS) $(LIBS) -o $(MAIN_TARGET)

$(OBJDIR)/%.o: %.cpp
	@echo compiling $<
	@$(CXX) $(CFLAGS) $(CXXFLAGS) $(INCDIR) -c $< -o $@
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
	@$(CXX) -MM -MG -MP $(CFLAGS) $(CXXFLAGS) $(INCDIR) -c $< -o $(OBJDIR)/$*.d
	@sed -i '' -e's/$*.o:/$(OBJDIR)\/$*.o:/g' $(OBJDIR)/$*.d
endif

$(OBJDIR):
	@mkdir $(OBJDIR)

symlink:
	@ln -fs db_r$(DB_VERSION) $(BINDIR)/db_symlink

strip:
	@cp $(BINDIR)/db_r$(DB_VERSION) $(BINDIR)/db_r
	@strip $(BINDIR)/db_r

clean:
	@rm -f $(CPPOBJS) $(BINDIR)/db_r*

dep:
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
	@echo "Note: gcc autodepend is autodetected, so target dep skipped"
else
	makedepend -f $(DEPFILE) $(INCDIR) -I/usr/include/c++/3.3 -I/usr/include/c++/4.2 -p$(OBJDIR)/ $(CPPFILE) 2> /dev/null > $(DEPFILE)
endif

# Dodatkowy target do wyświetlenia wykrytych plików (do debugowania)
show-files:
	@echo "Detected .cpp files:"
	@echo $(CPPFILE)
	@echo "Object files to be created:"
	@echo $(CPPOBJS)

# AUTO_DEPEND_CHECK BEGIN
ifeq ($(ENABLE_GCC_AUTODEPEND), 1)
sinclude $(CPPDEPS)
else
sinclude $(DEPFILE)
endif
# AUTO_DEPEND_CHECK END