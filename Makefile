CC = gcc
FFI_DIR = /usr/include/x86_64-linux-gnu/
CFLAGS = -Wall --std=gnu99 -g -I$(FFI_DIR)
NASMFLAGS = -f elf64
BIN = bin

SHARED_OBJECTS += $(BIN)/ast.o
SHARED_OBJECTS += $(BIN)/main.o

RUN += $(BIN)/ptrs

RUN_LIB_OBJECTS += $(BIN)/conversion.o
RUN_LIB_OBJECTS += $(BIN)/error.o
RUN_LIB_OBJECTS += $(BIN)/object.o
RUN_LIB_OBJECTS += $(BIN)/scope.o
RUN_LIB_OBJECTS += $(BIN)/call.o

RUN_OBJECTS += $(BIN)/statements.o
RUN_OBJECTS += $(BIN)/specialexpr.o
RUN_OBJECTS += $(BIN)/ops.o

all: $(RUN)

clean:
	rm bin -r

$(RUN): $(BIN) $(SHARED_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS)
	gcc $(SHARED_OBJECTS) $(RUN_LIB_OBJECTS) $(RUN_OBJECTS) -o $(BIN)/ptrs -ldl -lffi

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
