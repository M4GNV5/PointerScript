CC = gcc
FFCB_DIR = libffcb/src
FFCB_BIN = libffcb/bin/libffcb.a
JITAS_DIR = libjitas/src
JITAS_BIN = libjitas/bin/libjitas.a
INTERPRETER_INCLUDE = "../interpreter/interpreter.h"

BIN = bin
RUN = $(BIN)/ptrs

PARSER_OBJECTS += $(BIN)/ast.o

RUN_LIB_OBJECTS += $(BIN)/conversion.o
RUN_LIB_OBJECTS += $(BIN)/error.o
RUN_LIB_OBJECTS += $(BIN)/debug.o
RUN_LIB_OBJECTS += $(BIN)/scope.o
RUN_LIB_OBJECTS += $(BIN)/call.o
RUN_LIB_OBJECTS += $(BIN)/run.o
RUN_LIB_OBJECTS += $(BIN)/struct.o
RUN_LIB_OBJECTS += $(BIN)/astlist.o
RUN_LIB_OBJECTS += $(BIN)/nativetypes.o

RUN_OBJECTS += $(BIN)/statements.o
RUN_OBJECTS += $(BIN)/specialexpr.o
RUN_OBJECTS += $(BIN)/algorithm.o
RUN_OBJECTS += $(BIN)/ops.o
RUN_OBJECTS += $(BIN)/main.o

EXTERN_LIBS += -ldl
EXTERN_LIBS += -lffi

CFLAGS = '-DINTERPRETER_INCLUDE=$(INTERPRETER_INCLUDE)' -std=c99

ifdef PORTABLE
	ARCH = PORTABLE
else
	ARCH = $(shell uname -m)
endif

ifeq ($(ARCH),x86_64)
	CFLAGS += -I$(FFCB_DIR) -I$(JITAS_DIR)
	EXTERN_LIBS += $(FFCB_BIN)
	EXTERN_LIBS += $(JITAS_BIN)
else
	CFLAGS += -D_PTRS_PORTABLE
	$(info Building portable. Inline assembly and returning non-integers from native callbacks will not be available)
endif

ifeq ($(shell uname -o),GNU/Linux)
	CFLAGS += -D_GNU_SOURCE
else
	CFLAGS += -D_XOPEN_SOURCE=700
endif

all: CFLAGS += -Wall -O2 -g
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
	$(MAKE) -C libffcb clean
	$(MAKE) -C libjitas clean
	if [ -d $(BIN) ]; then rm -r $(BIN); fi

$(RUN): $(EXTERN_LIBS) $(BIN) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS)
	$(CC) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS) -o $(BIN)/ptrs -rdynamic $(EXTERN_LIBS)

$(BIN):
	mkdir $(BIN)

$(FFCB_BIN):
	$(MAKE) -C libffcb

$(JITAS_BIN):
	$(MAKE) -C libjitas

$(BIN)/%.o: parser/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: interpreter/lib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: interpreter/%.c
	$(CC) $(CFLAGS) -c $< -o $@
