CC = gcc
CFLAG = -g -O0 -W -Wall -Werror -Wcast-align
TARGET = server
.PHONY: all clean

all: build
	./$(TARGET)

debug: build
	gdb ./$(TARGET)

leak: build
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

build: 	server.o response.o
	gcc -o $(TARGET) $^

%.o:	%.c
	gcc -c $(CFLAG) -o $@ $<

clean:
	rm $(TARGET) *.o
	rm -f .*.swp
