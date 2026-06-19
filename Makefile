# Makefile for chop2e with headers in include/chop2e

BREW_PREFIX := $(shell brew --prefix raylib)

CC := clang
CFLAGS := -I$(BREW_PREFIX)/include -Ichop2e -Iinclude/chop2e -Iinclude -Wall -Wextra -Wno-unused-parameter -O2 -g
LDFLAGS := -L$(BREW_PREFIX)/lib -lraylib \
           -framework CoreVideo -framework IOKit -framework Cocoa \
           -framework GLUT -framework OpenGL
FRAMEWORKS = -framework AudioToolbox -framework CoreAudio -framework CoreFoundation

SRC := $(wildcard chop2e/*.c)
OBJ := $(SRC:chop2e/%.c=build/%.o)
BIN := build/chop2e

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS) $(FRAMEWORKS)

build/%.o: chop2e/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build $(BIN)

