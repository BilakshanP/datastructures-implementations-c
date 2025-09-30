CC = gcc
OUT = target/main
FLAGS = -std=c2x -Wpedantic -Wall -Wextra -Wconversion -lm -g -Isrc -Ilib

# All .c files in lib
LIB_SOURCES := $(shell find lib -name '*.c')

# Main program source
MAIN_SRC := src/main.c

# All test sources and their corresponding executables
TEST_SOURCES := $(wildcard tests/*_test.c)
TEST_BINS := $(TEST_SOURCES:.c=)

# === Build main program ===
build: $(LIB_SOURCES) $(MAIN_SRC)
	$(CC) $(LIB_SOURCES) $(MAIN_SRC) -o $(OUT) $(FLAGS)

run: $(OUT)
	./$(OUT)

# === Tests ===

print-tests:
	@echo "TEST_SOURCES=$(TEST_SOURCES)"
	@echo "TEST_BINS=$(TEST_BINS)"

# Compile & run all tests
test-all: $(TEST_BINS)
	@for exe in $(TEST_BINS); do \
		echo "Running $$exe..."; \
		./$$exe || exit 1; \
	done

# Pattern rule: build each test binary from src/tests/*.c
tests/%_test: tests/%_test.c $(LIB_SOURCES)
	$(CC) $(LIB_SOURCES) $< -o $@ $(FLAGS)

# Dynamic test runner: e.g., `make test-hashset` builds+runs src/tests/hashset_test
test-%: tests/%_test
	@echo "Running $<..."
	@./$<

# === Cleanup ===
clean:
	rm -f $(OUT) $(TEST_BINS)

.PHONY: build run clean test-all test-%
