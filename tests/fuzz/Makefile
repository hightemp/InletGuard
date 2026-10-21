# SPDX-License-Identifier: MIT
# libFuzzer harness. Discovers fuzz_*.c targets, builds each one together with
# the core sources and runs it for FUZZ_SECONDS.
#
#   make -C tests/fuzz run                  # all targets, 60 s each
#   make -C tests/fuzz run FUZZ_SECONDS=300
#
# Seed corpus: corpus/<target>/ (committed). Working corpus and crash
# artifacts: $(FUZZ_WORK_DIR)/corpus/<target>, $(FUZZ_WORK_DIR)/artifacts/<target>.

FUZZ_CC       ?= clang
FUZZ_SRC_DIR  ?= .
FUZZ_WORK_DIR ?= work
FUZZ_SECONDS  ?= 60

ROOT      := ../..
CFLAGS    := -g -O1 -std=c99 -D_POSIX_C_SOURCE=200809L -I$(ROOT)/src \
             -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined
CORE_SRCS := $(wildcard $(ROOT)/src/core/*.c)
TARGETS   := $(basename $(notdir $(wildcard $(FUZZ_SRC_DIR)/fuzz_*.c)))

.PHONY: run list clean

list:
	@echo $(TARGETS)

run:
ifeq ($(strip $(TARGETS)),)
	@echo "no fuzz targets yet"
else
	@failed=""; \
	for t in $(TARGETS); do \
		mkdir -p "$(FUZZ_WORK_DIR)/bin" "$(FUZZ_WORK_DIR)/corpus/$$t" "$(FUZZ_WORK_DIR)/artifacts/$$t"; \
		echo "== building $$t"; \
		$(FUZZ_CC) $(CFLAGS) -o "$(FUZZ_WORK_DIR)/bin/$$t" "$(FUZZ_SRC_DIR)/$$t.c" $(CORE_SRCS) || { failed="$$failed $$t"; continue; }; \
		seeds=""; [ -d "$(FUZZ_SRC_DIR)/corpus/$$t" ] && seeds="$(FUZZ_SRC_DIR)/corpus/$$t"; \
		echo "== fuzzing $$t for $(FUZZ_SECONDS) s"; \
		"$(FUZZ_WORK_DIR)/bin/$$t" -max_total_time=$(FUZZ_SECONDS) -print_final_stats=1 \
			-artifact_prefix="$(FUZZ_WORK_DIR)/artifacts/$$t/" \
			"$(FUZZ_WORK_DIR)/corpus/$$t" $$seeds || failed="$$failed $$t"; \
	done; \
	if [ -n "$$failed" ]; then echo "fuzzing found problems in:$$failed"; exit 1; fi; \
	echo "fuzzing passed: $(TARGETS)"
endif

clean:
	rm -rf "$(FUZZ_WORK_DIR)"
