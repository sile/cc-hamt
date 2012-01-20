CC=g++
CC_FLAGS=-O2 -Wall -ansi -pedantic-errors -I include/

all: bin bin/hamt

bin:
	mkdir bin

bin/hamt: src/bin/hamt.cc include/hamt/map.hh
	${CC} ${CC_FLAGS} -o ${@} ${<}

include/hamt/map.hh: include/hamt/hash.hh include/hamt/eql.hh include/hamt/allocator.hh
	touch ${@}

clean:
	rm bin/*
