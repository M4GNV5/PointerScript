CC = gcc
MYJIT_DIR = myjit/myjit
MYJIT_BIN = myjit/jitlib-core.o
JIT_INCLUDE = "../jit/jit.h"

BIN = bin
RUN = $(BIN)/ptrs

PARSER_OBJECTS += $(BIN)/ast.o

RUN_LIB_OBJECTS += $(BIN)/conversion.o
RUN_LIB_OBJECTS += $(BIN)/error.o
RUN_LIB_OBJECTS += $(BIN)/scope.o
RUN_LIB_OBJECTS += $(BIN)/astlist.o
RUN_LIB_OBJECTS += $(BIN)/run.o
RUN_LIB_OBJECTS += $(BIN)/struct.o
RUN_LIB_OBJECTS += $(BIN)/nativetypes.o

RUN_OBJECTS += $(BIN)/statements.o
RUN_OBJECTS += $(BIN)/specialexpr.o
RUN_OBJECTS += $(BIN)/ops.o
RUN_OBJECTS += $(BIN)/main.o
RUN_OBJECTS += $(BIN)/alloca.o

EXTERN_LIBS += -ldl
EXTERN_LIBS += -lffi
EXTERN_LIBS += $(MYJIT_BIN)

CFLAGS = '-DJIT_INCLUDE=$(JIT_INCLUDE)' -I$(MYJIT_DIR) -std=c99

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
	$(MAKE) -C myjit clean
	if [ -d $(BIN) ]; then rm -r $(BIN); fi

$(RUN): $(MYJIT_BIN) $(BIN) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS)
	$(CC) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS) -o $(BIN)/ptrs -rdynamic $(EXTERN_LIBS)

$(BIN):
	mkdir $(BIN)

$(MYJIT_BIN):
	$(MAKE) -C myjit

$(BIN)/%.o: parser/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: jit/lib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: jit/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: jit/%.s
	$(CC) -c $< -o $@
