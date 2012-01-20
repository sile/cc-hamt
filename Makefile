CC=g++
CC_FLAGS=-Wall -ansi -pedantic-errors -I include/

INC=include/hamt

all: bin bin/hamt

bin:
	mkdir bin

bin/hamt: src/bin/hamt.cc ${INC}/map.hh
	${CC} ${CC_FLAGS} -o ${@} ${<}
