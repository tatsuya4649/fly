CC = gcc
CFLAG = -g3 -O0 -W -Wall -Werror -Wcast-align
TARGET = fly
BUILD_FILES := server.o response.o header.o alloc.o fs.o method.o version.o math.o request.o util.o connect.o body.o api.o test_api.o fsignal.o main.o
SOURCE_FILES := $(BUILD_FILES:%.o=%.c)
.PHONY: all clean lib build test

MAJOR_VERSION:=1
MINOR_VERSION:=0
RELEA_VERSION:=0

LIBDIR := lib
LIBNAME := lib$(TARGET).so.$(MAJOR_VERSION)
BUILDDIR := build
TESTDIR := test
TEST_EXEC := fly_test
TEST_FILES := test_server.cpp test_signal.cpp test_fs.cpp test_api.cpp test_request.cpp test_util.cpp test_math.cpp test_body.cpp test_connect.cpp
ifdef FLY_TEST 
MACROS := $(MACROS) -D FLY_TEST
endif

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
	@gcc -shared $(MACROS) -fPIC -Wl,-soname,$(LIBNAME) -o $(LIBDIR)/$(LIBNAME).$(MINOR_VERSION).$(RELEA_VERSION) $(CFLAG) $(filter-out main.c, $^)
	@ldconfig -n $(LIBDIR)
	@ln -s $(LIBNAME) $(LIBDIR)/lib$(TARGET).so

test: clean_lib lib 
	@mkdir -p $(TESTDIR)
	@g++ -I. $(MACROS) -o $(TESTDIR)/$(TEST_EXEC) $(addprefix $(TESTDIR)/, $(TEST_FILES)) -L ./$(LIBDIR) -lgtest_main -lgtest -lgmock -lpthread -lfly 
	@LD_LIBRARY_PATH=./lib ./$(TESTDIR)/$(TEST_EXEC)

%.o:	%.c
	gcc -c $(CFLAG) -o $@ $<

clean_lib:
	@rm -f $(LIBDIR)/lib$(TARGET).so*

clean: clean_lib
	rm -f $(BUILDDIR)/$(TARGET)  *.o
	rm -f .*.swp
	rm -f $(TESTDIR)/$(TEST_EXEC)
