# fp_prelude/Makefile
# Builds build/fp_prelude.so as a Bash loadable builtin.
# Assumes Bash dev headers (builtins.h, shell.h) exist under $(BASH_INC).
# Adjust BASH_INC if needed (e.g., make BASH_INC=/usr/local/include/bash).

PKGNAME    := fp_prelude
BUILD_DIR  := build
TARGET     := $(BUILD_DIR)/$(PKGNAME).so

CC         ?= cc
CFLAGS     ?= -std=c11 -O2 -fPIC -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-function -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS    ?= -shared
BASH_INC   ?= /usr/include/bash
INC        := -Iinclude -I$(BASH_INC) \
	      $(addprefix -I,$(wildcard /usr/include/bash*/include))

SRC := src/engine.c src/fx.c src/op_registry.c \
       src/op_cut.c src/op_tr.c src/op_grep.c src/op_take.c src/op_find.c \
       src/op_cat.c src/op_emit.c \
       src/util.c

# Place object files in build/ mirroring src/ file names (flattened)
OBJ := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(filter src/%.c,$(SRC)))

.PHONY: all clean test print-vars

all: $(TARGET)

# Ensure build directory exists before compiling/linking
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile each .c to build/*.o
$(BUILD_DIR)/%.o: src/%.c include/engine.h include/ops.h include/util.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# Link the shared object
$(TARGET): $(OBJ) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

clean:
	rm -rf $(BUILD_DIR)

test: all
	@bash tests/run_golden.sh

print-vars:
	@echo "CC=$(CC)"
	@echo "CFLAGS=$(CFLAGS)"
	@echo "LDFLAGS=$(LDFLAGS)"
	@echo "BASH_INC=$(BASH_INC)"
	@echo "INC=$(INC)"
	@echo "SRC=$(SRC)"
	@echo "OBJ=$(OBJ)"
	@echo "TARGET=$(TARGET)"
