CC = gcc
CFLAG = -g3 -O0 -W -Wall -Werror -Wcast-align
TARGET = fly
BUILD_FILES := server.o response.o header.o alloc.o fs.o method.o version.o math.o request.o util.o connect.o body.o api.o test_api.o signal.o
SOURCE_FILES := $(BUILD_FILES:%.o=%.c)
.PHONY: all clean lib build

MAJOR_VERSION:=1
MINOR_VERSION:=0
RELEA_VERSION:=0

LIBDIR:=lib
BUILDDIR:=build

all: build
	./$(BUILDDIR)/$(TARGET)

debug: build
	gdb ./$(BUILDDIR)/$(TARGET)

leak: build
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

build:	$(BUILD_FILES)
	@mkdir -p $(BUILDDIR)
	gcc -o $(BUILDDIR)/$(TARGET) $^

lib: $(SOURCE_FILES)
	@mkdir -p $(LIBDIR)
	gcc -shared -fPIC -Wl,-soname,lib$(TARGET).so.$(MAJOR_VERSION) -o $(LIBDIR)/lib$(TARGET).so.$(MAJOR_VERSION).$(MINOR_VERSION).$(RELEA_VERSION) $(CFLAG) $^
	ldconfig -n $(LIBDIR)

%.o:	%.c
	gcc -c $(CFLAG) -o $@ $<

clean:
	rm -f $(BUILDDIR)/$(TARGET) $(LIBDIR)/lib$(TARGET).so* *.o
	rm -f .*.swp
