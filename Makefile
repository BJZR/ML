CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = mlc
OBJS = main.o lexer.o parser.o codegen.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c ml.h
	$(CC) $(CFLAGS) -c main.c

lexer.o: lexer.c ml.h
	$(CC) $(CFLAGS) -c lexer.c

parser.o: parser.c ml.h
	$(CC) $(CFLAGS) -c parser.c

codegen.o: codegen.c ml.h
	$(CC) $(CFLAGS) -c codegen.c

clean:
	rm -f $(OBJS) $(TARGET) out.asm out.o programa

.PHONY: all clean