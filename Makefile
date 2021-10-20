CC = gcc
CPP = g++
ifdef DEBUG
CFLAG := -pg -gdwarf-2 -g3 -O0 -W -Wall -Werror -Wcast-align
else
CFLAG := -g3 -O0 -W -Wall -Werror -Wcast-align
endif
TARGET = fly
BUILD_FILES := server.o response.o header.o alloc.o mount.o method.o version.o math.o request.o util.o connect.o body.o route.o test_route.o fsignal.o main.o mime.o encode.o log.o worker.o master.o ftime.o event.o context.o err.o charset.o lang.o cache.o scheme.o buffer.o rbtree.o ssl.o v2.o queue.o bllist.o config.o uri.o
PYBUILD_FILES :=
SOURCE_FILES := $(BUILD_FILES:%.o=%.c)
PYSOURCE_FILES := $(PYBUILD_FILES:%.o=%.c)
.PHONY: all clean lib build test

MAJOR_VERSION:=1
MINOR_VERSION:=0
RELEA_VERSION:=0

DEPEND_LIBS := -lz -lbrotlienc -lcrypto -lbrotlidec -lssl

LIBDIR := lib
LIBNAME := lib$(TARGET).so.$(MAJOR_VERSION)
BUILDDIR := build
TESTDIR := test
TESTPRE := __fly_
TEST_FILES := test_server.cpp test_signal.cpp test_fs.cpp test_route.cpp test_request.cpp test_util.cpp test_math.cpp test_body.cpp test_connect.cpp test_log.cpp
ifdef DEBUG
MACROS := $(MACROS) -D DEBUG
endif
PYTHON := python

all: build
	./$(BUILDDIR)/$(TARGET)

debug: build
	gdb ./$(BUILDDIR)/$(TARGET)

leak: build
	valgrind --leak-check=full --show-leak-kinds=all ./$(BUILDDIR)/$(TARGET)

build:	$(BUILD_FILES)
	@mkdir -p $(BUILDDIR)
	gcc -o $(BUILDDIR)/$(TARGET) $^

pybuild:
	#@rm -rf fly/fly*.so
	$(PYTHON) setup.py build

lib: $(SOURCE_FILES)
	@mkdir -p $(LIBDIR)
	gcc $(CFLAG) -shared $(MACROS) -fPIC -Wl,-soname,$(LIBNAME) $(DEPEND_LIBS) -o $(LIBDIR)/$(LIBNAME).$(MINOR_VERSION).$(RELEA_VERSION) $(filter-out main.c, $^)
	@ldconfig -n $(LIBDIR)
	@ln -s $(LIBNAME) $(LIBDIR)/lib$(TARGET).so

test: test_placeholder clean_lib lib $(addprefix $(TESTPRE), $(TEST_FILES))
test_placeholder:
	@echo -en "\t>>> fly test start <<< \n"
	@mkdir -p $(TESTDIR)

$(TESTPRE)%.cpp:
		@echo "Target: $@"
		@echo "Source: $(subst $(TESTPRE),,$@)"
		$(CPP) -g3 -O0 -I. $(MACROS) -o $(TESTDIR)/$(addprefix $(TESTPRE), $(subst $(TESTPRE),, $@)) $(addprefix $(TESTDIR)/, $(subst $(TESTPRE),,$@)) -L ./$(LIBDIR) $(DEPEND_LIBS) -lgtest_main -lgtest -lgmock -lpthread -lfly
		LD_LIBRARY_PATH=./lib ./$(TESTDIR)/$(addprefix $(TESTPRE), $(subst $(TESTPRE),,$@))
$(TESTPRE)%.c:
		@echo "Target: $@"
		@echo "Source: $(subst $(TESTPRE),,$@)"
		$(CC) -g3 -O0 -I. $(MACROS) -o $(TESTDIR)/$(addprefix $(TESTPRE), $(subst $(TESTPRE),, $@)) $(addprefix $(TESTDIR)/, $(subst $(TESTPRE),,$@)) -L ./$(LIBDIR) $(DEPEND_LIBS) -lgtest_main -lgtest -lgmock -lpthread -lfly
		LD_LIBRARY_PATH=./lib ./$(TESTDIR)/$(addprefix $(TESTPRE), $(subst $(TESTPRE),,$@))

%.o:	%.c
	gcc -c $(CFLAG) -o $@ $<

clean_lib:
	@rm -f $(LIBDIR)/lib$(TARGET).so*

clean: clean_lib
	rm -f $(BUILDDIR)/$(TARGET)  *.o
	rm -f .*.swp
	rm -f $(TESTDIR)/__fly_*
	rm -f fly/*fly*.so
