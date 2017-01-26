CC = gcc
FFI_DIR = /usr/include/x86_64-linux-gnu/
FFCB_DIR = libffcb/src
FFCB_BIN = libffcb/bin/libffcb.a
JITAS_DIR = libjitas/src
JITAS_BIN = libjitas/bin/libjitas.a
INTERPRETER_INCLUDE = "../interpreter/interpreter.h"

CFLAGS_COMMON = '-DINTERPRETER_INCLUDE=$(INTERPRETER_INCLUDE)' -D_GNU_SOURCE --std=c99 -I$(FFI_DIR) -I$(FFCB_DIR) -I$(JITAS_DIR)

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

all: CFLAGS = -Wall -O2 -g $(CFLAGS_COMMON)
all: $(RUN)

debug: CFLAGS = -Wall -g $(CFLAGS_COMMON)
debug: $(RUN)

release: CFLAGS = -O2 $(CFLAGS_COMMON)
release: $(RUN)

portable: CFLAGS = '-DINTERPRETER_INCLUDE=$(INTERPRETER_INCLUDE)' -D_PTRS_NOASM -D_PTRS_NOCALLBACK -D_XOPEN_SOURCE=700 -std=c99 -O2 -I$(FFI_DIR)
portable: $(BIN) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS)
	$(CC) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS) -o $(BIN)/ptrs -rdynamic -ldl -lffi -lpthread

install: release
	cp $(RUN) /usr/local/bin/

remove:
	rm /usr/local/bin/ptrs

clean:
	if [ -d $(BIN) ]; then rm -r $(BIN); fi

cleandeps:
	$(MAKE) -C libffcb clean
	$(MAKE) -C libjitas clean

$(RUN): $(FFCB_BIN) $(JITAS_BIN) $(BIN) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS)
	$(CC) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS) $(FFCB_BIN) $(JITAS_BIN) -o $(BIN)/ptrs -rdynamic -ldl -lffi -lpthread

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
