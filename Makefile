CC = gcc
FFI_DIR = /usr/include/x86_64-linux-gnu/
INTERPRETER_INCLUDE = "../interpreter/interpreter.h"
CFLAGS = -Wall '-DINTERPRETER_INCLUDE=$(INTERPRETER_INCLUDE)' -D_GNU_SOURCE --std=gnu99 -g -I$(FFI_DIR)

BIN = bin
RUN = $(BIN)/ptrs

PARSER_OBJECTS += $(BIN)/ast.o

RUN_LIB_OBJECTS += $(BIN)/conversion.o
RUN_LIB_OBJECTS += $(BIN)/error.o
RUN_LIB_OBJECTS += $(BIN)/scope.o
RUN_LIB_OBJECTS += $(BIN)/call.o
RUN_LIB_OBJECTS += $(BIN)/stack.o
RUN_LIB_OBJECTS += $(BIN)/run.o

RUN_OBJECTS += $(BIN)/statements.o
RUN_OBJECTS += $(BIN)/specialexpr.o
RUN_OBJECTS += $(BIN)/ops.o
RUN_OBJECTS += $(BIN)/main.o

all: debug

debug: $(RUN)

release: CFLAGS = '-DINTERPRETER_INCLUDE=$(INTERPRETER_INCLUDE)' -D_GNU_SOURCE --std=gnu99 -O2 -I$(FFI_DIR)
release: $(RUN)

install: release
	cp $(RUN) /usr/local/bin/

remove:
	rm /usr/local/bin/ptrs

clean:
	if [ -d $(BIN) ]; then rm -r $(BIN); fi

$(RUN): $(BIN) $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS)
	gcc $(PARSER_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS) -o $(BIN)/ptrs -rdynamic -ldl -lavcall -lcallback

$(BIN):
	mkdir $(BIN)

$(BIN)/%.o: parser/%.c
	gcc $(CFLAGS) -c $< -o $@

$(BIN)/%.o: interpreter/lib/%.c
	gcc $(CFLAGS) -c $< -o $@

$(BIN)/%.o: interpreter/lib/%.asm
	nasm $(NASMFLAGS) $< -o $@

$(BIN)/%.o: interpreter/%.c
	gcc $(CFLAGS) -c $< -o $@
