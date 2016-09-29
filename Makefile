CC = gcc
FFI_DIR = /usr/include/x86_64-linux-gnu/
FFCB_DIR = libffcb/src
FFCB_BIN = libffcb/bin/libffcb.a
JITAS_DIR = libjitas/src
JITAS_BIN = libjitas/bin/libjitas.a
INTERPRETER_INCLUDE = "../interpreter/interpreter.h"
CFLAGS = -Wall '-DINTERPRETER_INCLUDE=$(INTERPRETER_INCLUDE)' -D_GNU_SOURCE --std=c99 -g -I$(FFI_DIR) -I$(FFCB_DIR) -I$(JITAS_DIR)

BIN = bin
RUN = $(BIN)/ptrs

PARSER_OBJECTS += $(BIN)/ast.o

RUN_LIB_OBJECTS += $(BIN)/conversion.o
RUN_LIB_OBJECTS += $(BIN)/error.o
RUN_LIB_OBJECTS += $(BIN)/debug.o
RUN_LIB_OBJECTS += $(BIN)/scope.o
RUN_LIB_OBJECTS += $(BIN)/call.o
RUN_LIB_OBJECTS += $(BIN)/stack.o
RUN_LIB_OBJECTS += $(BIN)/run.o
RUN_LIB_OBJECTS += $(BIN)/struct.o
RUN_LIB_OBJECTS += $(BIN)/astlist.o
RUN_LIB_OBJECTS += $(BIN)/nativetypes.o

RUN_OBJECTS += $(BIN)/statements.o
RUN_OBJECTS += $(BIN)/specialexpr.o
RUN_OBJECTS += $(BIN)/ops.o
RUN_OBJECTS += $(BIN)/main.o

all: debug

debug: $(RUN)

release: CFLAGS = '-DINTERPRETER_INCLUDE=$(INTERPRETER_INCLUDE)' -D_GNU_SOURCE --std=c99 -O2 -I$(FFI_DIR) -I$(FFCB_DIR) -I$(JITAS_DIR)
release: $(RUN)

install: release
	cp $(RUN) /usr/local/bin/

remove:
	rm /usr/local/bin/ptrs

clean:
	if [ -d $(BIN) ]; then rm -r $(BIN); fi

$(RUN): $(BIN) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS)
	$(CC) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS) $(FFCB_BIN) $(JITAS_BIN) -o $(BIN)/ptrs -rdynamic -ldl -lffi

$(BIN):
	mkdir $(BIN)

$(BIN)/%.o: parser/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: interpreter/lib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: interpreter/lib/%.asm
	nasm $(NASMFLAGS) $< -o $@

$(BIN)/%.o: interpreter/%.c
	$(CC) $(CFLAGS) -c $< -o $@
