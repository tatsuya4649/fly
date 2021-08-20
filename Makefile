CC = gcc
CFLAG = -g3 -O0 -W -Wall -Werror -Wcast-align
TARGET = server
BUILD_FILES := server.o response.o header.o alloc.o fs.o method.o version.o
.PHONY: all clean

all: build
	./$(TARGET)

debug: build
	gdb ./$(TARGET)

leak: build
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

build:	$(BUILD_FILES)
	gcc -o $(TARGET) $^

%.o:	%.c
	gcc -c $(CFLAG) -o $@ $<

clean:
	rm $(TARGET) *.o
	rm -f .*.swp
