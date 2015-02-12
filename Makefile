OBJS=compdoc.o parse.o io.o example.o
BIN=test
CFLAGS=-Wall -ggdb

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(BIN) 
	rm $(OBJS)
