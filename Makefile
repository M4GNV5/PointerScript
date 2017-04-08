CC = gcc
MYJIT_DIR = myjit/jitlib
MYJIT_BIN = myjit/jitlib-core.o
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
RUN_OBJECTS += $(BIN)/ops.o
RUN_OBJECTS += $(BIN)/main.o

EXTERN_LIBS += -ldl
EXTERN_LIBS += -lffi
EXTERN_LIBS += -l$(MYJIT_BIN)

CFLAGS = '-DINTERPRETER_INCLUDE=$(INTERPRETER_INCLUDE)' -std=c99 -I$(FFCB_DIR) -I$(JITAS_DIR) -I$(MYJIT_DIR)

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

$(BIN)/%.o: interpreter/lib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/%.o: interpreter/%.c
	$(CC) $(CFLAGS) -c $< -o $@
