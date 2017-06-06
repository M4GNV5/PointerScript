CC = gcc
LIBJIT_DIR = libjit/include
LIBJIT_BIN = libjit/jit/.libs/libjit.a
JIT_INCLUDE = "../jit/jit.h"

BIN = bin
RUN = $(BIN)/ptrs

PARSER_OBJECTS += $(BIN)/ast.o

RUN_LIB_OBJECTS += $(BIN)/conversion.o
RUN_LIB_OBJECTS += $(BIN)/error.o
RUN_LIB_OBJECTS += $(BIN)/astlist.o
RUN_LIB_OBJECTS += $(BIN)/call.o
RUN_LIB_OBJECTS += $(BIN)/run.o
RUN_LIB_OBJECTS += $(BIN)/nativetypes.o
RUN_LIB_OBJECTS += $(BIN)/struct.o

RUN_OBJECTS += $(BIN)/statements.o
RUN_OBJECTS += $(BIN)/specialexpr.o
RUN_OBJECTS += $(BIN)/ops.o
RUN_OBJECTS += $(BIN)/main.o

EXTERN_LIBS += -lm
EXTERN_LIBS += -ldl
EXTERN_LIBS += -lpthread
EXTERN_LIBS += $(LIBJIT_BIN)

CFLAGS = '-DJIT_INCLUDE=$(JIT_INCLUDE)' -I$(LIBJIT_DIR) -std=c99

ifdef PORTABLE
ARCH = PORTABLE
CFLAGS += -D_XOPEN_SOURCE=700
else
ARCH = $(shell uname -m)
ifeq ($(shell uname -o),GNU/Linux)
CFLAGS += -D_GNU_SOURCE
else
CFLAGS += -D_XOPEN_SOURCE=700
endif
endif

all: CFLAGS += -O2 -g
all: $(RUN)

debug: CFLAGS += -Wall -g
debug: $(RUN)

release: CFLAGS += -O2
release: $(RUN)

install: release
	cp $(RUN) /usr/local/bin/

remove:
	rm /usr/local/bin/ptrs

clean:
	if [ -d $(BIN) ]; then rm -r $(BIN); fi

clean-deps:
	$(MAKE) -C libjit clean

$(RUN): $(MYJIT_BIN) $(BIN) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS)
	$(CC) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS) -o $(BIN)/ptrs -rdynamic $(EXTERN_LIBS)

$(BIN):
	mkdir $(BIN)

$(LIBJIT_BIN):
	cd myjit && ./bootstrap
	cd myjit && ./configure
	$(MAKE) -C myjit

$(BIN)/%.o: parser/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: jit/lib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: jit/%.c
	$(CC) $(CFLAGS) -c $< -o $@
