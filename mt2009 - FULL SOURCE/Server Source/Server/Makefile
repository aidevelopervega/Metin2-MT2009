CC = clang
CXX = clang++

PLATFORM = $(shell file /bin/ls | cut -d' ' -f3 | cut -d'-' -f1)
BSD_VERSION = $(shell uname -v 2>&1 | cut -d' ' -f2 | cut -d'.' -f1)
SVR_VERSION = $(shell cat __REVISION__)

.PHONY:  liblua libsql libgame libpoly libthecore game db

default: liblua libsql libgame libpoly libthecore game db
	@echo "--------------------------------------"
	@echo "Build Done"
	@echo "--------------------------------------"

quick: .
	@if [ -d "liblua" ]; then $(MAKE) -C liblua/5.0; elif [ -d "../../../server/src/liblua" ]; then $(MAKE) -C ../../../server/src/liblua/5.0; fi
	@if [ -d "libsql" ]; then $(MAKE) -C libsql; elif [ -d "../../../server/src/libsql" ]; then $(MAKE) -C ../../../server/src/libsql; fi
	@if [ -d "libgame" ]; then $(MAKE) -C libgame/src; elif [ -d "../../../server/src/libgame" ]; then $(MAKE) -C ../../../server/src/libgame/src; fi
	@if [ -d "libpoly" ]; then $(MAKE) -C libpoly; elif [ -d "../../../server/src/libpoly" ]; then $(MAKE) -C ../../../server/src/libpoly; fi
	@if [ -d "libthecore" ]; then $(MAKE) -C libthecore/src; elif [ -d "../../../server/src/libthecore" ]; then $(MAKE) -C ../../../server/src/libthecore/src; fi
	touch game/src/main.cpp
	$(MAKE) -C game/src
	$(MAKE) -C game/src symlink
	touch db/src/Main.cpp
	$(MAKE) -C db/src
	$(MAKE) -C db/src symlink

liblua: .
	@if [ -d "$@" ]; then $(MAKE) -C $@/5.0 clean && $(MAKE) -C $@/5.0; \
	elif [ -d "../../../server/src/$@" ]; then $(MAKE) -C ../../../server/src/$@/5.0 clean && $(MAKE) -C ../../../server/src/$@/5.0; fi

libsql: .
	@if [ -d "$@" ]; then touch $@/Depend && $(MAKE) -C $@ dep && $(MAKE) -C $@ clean && $(MAKE) -C $@; \
	elif [ -d "../../../server/src/$@" ]; then touch ../../../server/src/$@/Depend && $(MAKE) -C ../../../server/src/$@ dep && $(MAKE) -C ../../../server/src/$@ clean && $(MAKE) -C ../../../server/src/$@; fi

libgame: .
	@if [ -d "$@" ]; then touch $@/src/Depend && $(MAKE) -C $@/src dep && $(MAKE) -C $@/src clean && $(MAKE) -C $@/src; \
	elif [ -d "../../../server/src/$@" ]; then touch ../../../server/src/$@/src/Depend && $(MAKE) -C ../../../server/src/$@/src dep && $(MAKE) -C ../../../server/src/$@/src clean && $(MAKE) -C ../../../server/src/$@/src; fi

libpoly: .
	@if [ -d "$@" ]; then touch $@/Depend && $(MAKE) -C $@ dep && $(MAKE) -C $@ clean && $(MAKE) -C $@; \
	elif [ -d "../../../server/src/$@" ]; then touch ../../../server/src/$@/Depend && $(MAKE) -C ../../../server/src/$@ dep && $(MAKE) -C ../../../server/src/$@ clean && $(MAKE) -C ../../../server/src/$@; fi

libthecore: .
	@if [ -d "$@" ]; then touch $@/src/Depend && $(MAKE) -C $@/src dep && $(MAKE) -C $@/src clean && $(MAKE) -C $@/src; \
	elif [ -d "../../../server/src/$@" ]; then touch ../../../server/src/$@/src/Depend && $(MAKE) -C ../../../server/src/$@/src dep && $(MAKE) -C ../../../server/src/$@/src clean && $(MAKE) -C ../../../server/src/$@/src; fi

game: .
	@touch $@/src/Depend
	$(MAKE) -C $@/src dep
	$(MAKE) -C $@/src clean
	# $(MAKE) -C $@/src limit_time
	$(MAKE) -C $@/src
	$(MAKE) -C $@/src symlink

db: .
	@touch $@/src/Depend
	$(MAKE) -C $@/src dep
	$(MAKE) -C $@/src clean
	$(MAKE) -C $@/src
	$(MAKE) -C $@/src symlink

ver:
	@$(CC) -v

ver2:
	@$(CC) -v
	$(MAKE) -C game/src ver

symlink:
	$(MAKE) -C game/src symlink
	$(MAKE) -C db/src symlink

strip:
	$(MAKE) -C game/src strip
	$(MAKE) -C db/src strip

clean:
	@if [ -d "liblua" ]; then $(MAKE) -C liblua/5.0 clean; elif [ -d "../../../server/src/liblua" ]; then $(MAKE) -C ../../../server/src/liblua/5.0 clean; fi
	@if [ -d "libsql" ]; then $(MAKE) -C libsql clean; elif [ -d "../../../server/src/libsql" ]; then $(MAKE) -C ../../../server/src/libsql clean; fi
	@if [ -d "libgame" ]; then $(MAKE) -C libgame/src clean; elif [ -d "../../../server/src/libgame" ]; then $(MAKE) -C ../../../server/src/libgame/src clean; fi
	@if [ -d "libpoly" ]; then $(MAKE) -C libpoly clean; elif [ -d "../../../server/src/libpoly" ]; then $(MAKE) -C ../../../server/src/libpoly clean; fi
	@if [ -d "libthecore" ]; then $(MAKE) -C libthecore/src clean; elif [ -d "../../../server/src/libthecore" ]; then $(MAKE) -C ../../../server/src/libthecore/src clean; fi
	$(MAKE) -C game/src clean
	$(MAKE) -C db/src clean

all:
	@echo "--------------------------------------"
	@echo "Update Revision"
	@echo "--------------------------------------"
	@expr $(SVR_VERSION) + 1 > __REVISION__
	@cat  __REVISION__

	@echo "--------------------------------------"
	@echo "Full Build Start"
	@echo "--------------------------------------"

	@if [ -d "liblua" ]; then $(MAKE) -C liblua/5.0 clean && $(MAKE) -C liblua/5.0; elif [ -d "../../../server/src/liblua" ]; then $(MAKE) -C ../../../server/src/liblua/5.0 clean && $(MAKE) -C ../../../server/src/liblua/5.0; fi

	# $(MAKE) -C liblua/5.2 clean
	# $(MAKE) -C liblua/5.2 freebsd
	# $(MAKE) -C liblua/5.2 local

	@if [ -d "libsql" ]; then touch libsql/Depend && $(MAKE) -C libsql dep && $(MAKE) -C libsql clean && $(MAKE) -C libsql; \
	elif [ -d "../../../server/src/libsql" ]; then touch ../../../server/src/libsql/Depend && $(MAKE) -C ../../../server/src/libsql dep && $(MAKE) -C ../../../server/src/libsql clean && $(MAKE) -C ../../../server/src/libsql; fi

	@if [ -d "libgame" ]; then touch libgame/src/Depend && $(MAKE) -C libgame/src dep && $(MAKE) -C libgame/src clean && $(MAKE) -C libgame/src; \
	elif [ -d "../../../server/src/libgame" ]; then touch ../../../server/src/libgame/src/Depend && $(MAKE) -C ../../../server/src/libgame/src dep && $(MAKE) -C ../../../server/src/libgame/src clean && $(MAKE) -C ../../../server/src/libgame/src; fi

	@if [ -d "libpoly" ]; then touch libpoly/Depend && $(MAKE) -C libpoly dep && $(MAKE) -C libpoly clean && $(MAKE) -C libpoly; \
	elif [ -d "../../../server/src/libpoly" ]; then touch ../../../server/src/libpoly/Depend && $(MAKE) -C ../../../server/src/libpoly dep && $(MAKE) -C ../../../server/src/libpoly clean && $(MAKE) -C ../../../server/src/libpoly; fi

	@if [ -d "libthecore" ]; then touch libthecore/src/Depend && $(MAKE) -C libthecore/src dep && $(MAKE) -C libthecore/src clean && $(MAKE) -C libthecore/src; \
	elif [ -d "../../../server/src/libthecore" ]; then touch ../../../server/src/libthecore/src/Depend && $(MAKE) -C ../../../server/src/libthecore/src dep && $(MAKE) -C ../../../server/src/libthecore/src clean && $(MAKE) -C ../../../server/src/libthecore/src; fi

	@touch game/src/Depend
	$(MAKE) -C game/src dep
	$(MAKE) -C game/src clean
	# $(MAKE) -C game/src limit_time
	$(MAKE) -C game/src
	$(MAKE) -C game/src symlink
	# $(MAKE) -C game/src strip

	@touch db/src/Depend
	$(MAKE) -C db/src dep
	$(MAKE) -C db/src clean
	$(MAKE) -C db/src
	$(MAKE) -C db/src symlink
	# $(MAKE) -C db/src strip
	@echo "--------------------------------------"
	@echo "Full Build End"
	@echo "--------------------------------------"
