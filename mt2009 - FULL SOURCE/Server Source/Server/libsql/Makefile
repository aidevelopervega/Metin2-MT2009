CXX ?= g++

BIN     = ./libsql.a

INCDIR  = -I../../Extern/include -I../libthecore/include -I/usr/local/include/mysql -I/usr/local/include
CFLAGS  = $(INCDIR) -Wall -Wextra -O2 -pipe -D_THREAD_SAFE -fno-exceptions -fno-rtti -std=c++20

LIBS =

OBJFILES = AsyncSQL.o Semaphore.o Tellwait.o Statement.o

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
	$(CXX) $(CFLAGS) -MM *.cpp > Depend

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

sinclude Depend

