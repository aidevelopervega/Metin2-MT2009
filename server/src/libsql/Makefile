CXX ?= g++

BIN     = ./libsql.a

INCDIR  = -I/usr/local/include/mysql -I/usr/local/include/mariadb -I/usr/local/include -I../libthecore/include -I../../../extern/common/include -I../../../extern/server/include -I../../../extern/server/include/mysql
CFLAGS  = $(INCDIR) -Wall -Wextra -O2 -pipe -D_THREAD_SAFE -fno-exceptions -std=c++20

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

