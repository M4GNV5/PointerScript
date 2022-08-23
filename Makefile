CC = gcc
LIBJIT_DIR = libjit/include
LIBJIT_BIN = libjit/jit/.libs/libjit.a
JIT_INCLUDE = "../jit/jit.h"

BIN = bin
BIN_DIRS += $(BIN)
BIN_DIRS += $(BIN)/lib
BIN_DIRS += $(BIN)/ops

RUN = $(BIN)/ptrs

PARSER_OBJECTS += $(BIN)/ast.o
PARSER_OBJECTS += $(BIN)/nativetypes.o

RUN_OBJECTS += $(BIN)/statements.o
RUN_OBJECTS += $(BIN)/specialexpr.o
RUN_OBJECTS += $(BIN)/vtables.o
RUN_OBJECTS += $(BIN)/main.o

RUN_OBJECTS += $(BIN)/lib/conversion.o
RUN_OBJECTS += $(BIN)/lib/error.o
RUN_OBJECTS += $(BIN)/lib/astlist.o
RUN_OBJECTS += $(BIN)/lib/call.o
RUN_OBJECTS += $(BIN)/lib/run.o
RUN_OBJECTS += $(BIN)/lib/nativetypes.o
RUN_OBJECTS += $(BIN)/lib/struct.o
RUN_OBJECTS += $(BIN)/lib/util.o
RUN_OBJECTS += $(BIN)/lib/flow.o

RUN_OBJECTS += $(BIN)/ops/binary.o
RUN_OBJECTS += $(BIN)/ops/unary.o
RUN_OBJECTS += $(BIN)/ops/special.o

EXTERN_LIBS += $(LIBJIT_BIN)
EXTERN_LIBS += -lm
EXTERN_LIBS += -ldl
EXTERN_LIBS += -lpthread

CFLAGS = -I$(LIBJIT_DIR) -std=c99

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

debug: CFLAGS += -g
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

$(RUN): $(LIBJIT_BIN) $(BIN_DIRS) $(PARSER_OBJECTS) $(RUN_OBJECTS)
	$(CC) $(PARSER_OBJECTS) $(RUN_OBJECTS) -o $(BIN)/ptrs -rdynamic $(EXTERN_LIBS)

$(BIN):
	mkdir $(BIN)

$(BIN)/%:
	mkdir $@

$(LIBJIT_BIN):
	cd libjit && ./bootstrap
	cd libjit && ./configure
	$(MAKE) -C libjit

$(BIN)/%.o: parser/%.c $(LIBJIT_BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: jit/%.c $(LIBJIT_BIN)
	$(CC) $(CFLAGS) -c $< -o $@
