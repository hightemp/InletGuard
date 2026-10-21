# SPDX-License-Identifier: MIT
# Standalone build of the core unit tests. Does not need PHP.
#
#   make -C tests/unit            # build and run
#   make -C tests/unit SANITIZE=1 # with ASan/UBSan
#   make -C tests/unit FILTER=version

CC       ?= cc
ROOT     := ../..
CFLAGS   ?= -O0 -g
CFLAGS   += -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic -I$(ROOT)/src -I.

ifeq ($(SANITIZE),1)
CFLAGS   += -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
LDFLAGS  += -fsanitize=address,undefined
endif

CORE_SRCS := $(wildcard $(ROOT)/src/core/*.c)
TEST_SRCS := runner.c $(wildcard test_*.c)
BIN       := ig_unit

.PHONY: all run clean

all: run

$(BIN): $(TEST_SRCS) $(CORE_SRCS) ig_test.h $(wildcard $(ROOT)/src/core/*.h)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRCS) $(CORE_SRCS) $(LDFLAGS)

run: $(BIN)
	./$(BIN) $(FILTER)

clean:
	rm -f $(BIN)
