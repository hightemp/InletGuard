# SPDX-License-Identifier: MIT
# Standalone build of the core unit tests. Does not need PHP.
#
#   make -C tests/unit            # build and run
#   make -C tests/unit SANITIZE=1 # with ASan/UBSan
#   make -C tests/unit FILTER=version

CC       ?= cc
ROOT     := ../..
TP_DIR   := $(ROOT)/third_party/libinjection
OPT      ?= -O0 -g

ifeq ($(SANITIZE),1)
SAN      := -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
endif

# Our code: strict C99. Vendored code: built unmodified with lenient flags.
CFLAGS    := $(OPT) $(SAN) -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic \
             -I$(ROOT)/src -I$(TP_DIR) -I.
TP_CFLAGS := $(OPT) $(SAN) -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS   += $(SAN)

CORE_SRCS := $(wildcard $(ROOT)/src/core/*.c)
TP_SRCS   := $(wildcard $(TP_DIR)/*.c)
TP_OBJS   := $(patsubst $(TP_DIR)/%.c,tp_%.o,$(TP_SRCS))
TEST_SRCS := runner.c $(wildcard test_*.c)
BIN       := ig_unit

.PHONY: all run clean

all: run

tp_%.o: $(TP_DIR)/%.c $(wildcard $(TP_DIR)/*.h)
	$(CC) $(TP_CFLAGS) -c $< -o $@

$(BIN): $(TEST_SRCS) $(CORE_SRCS) $(TP_OBJS) ig_test.h $(wildcard $(ROOT)/src/core/*.h)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRCS) $(CORE_SRCS) $(TP_OBJS) $(LDFLAGS)

run: $(BIN)
	./$(BIN) $(FILTER)

clean:
	rm -f $(BIN) tp_*.o
