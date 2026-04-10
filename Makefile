# ──────────────────────────────────────────────────────────────
#  Usage
# ──────────────────────────────────────────────────────────────
#
#  Run a single test:
#
#    make tests/functions/contains_header_end
#    make tests/functions/get_content_length
#    make tests/functions/read_http_server
#
#  Run all tests:
#
#    make tests
#
#  Build the HTTP server:
#
#    make server
#
#  Clean all build artifacts:
#
#    make clean
#
# ──────────────────────────────────────────────────────────────

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c2x -g
BUILD    = build
SRC_DIR  = src

# Every .c in src/ compiled to a .o
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
SRC_OBJS  = $(patsubst $(SRC_DIR)/%.c, $(BUILD)/src/%.o, $(SRC_FILES))

# ── Compile src/*.c → build/src/*.o ──────────────────────────

$(BUILD)/src/%.o: $(SRC_DIR)/%.c | $(BUILD)/src
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/src:
	mkdir -p $@

# ── make tests/functions/foo  ────────────────────────────────
#
#  Compiles tests/functions/foo.c, links with all src/*.o, runs it.

TEST_FILES  = $(wildcard tests/**/*.c)
TEST_TARGETS = $(basename $(TEST_FILES))

tests/%: tests/%.c $(SRC_OBJS)
	@mkdir -p $(BUILD)/$(dir $@)
	$(CC) $(CFLAGS) $< $(SRC_OBJS) -o $(BUILD)/$@
	./$(BUILD)/$@

# ── make tests  ──────────────────────────────────────────────
#
#  Compiles and runs every test file under tests/.

.PHONY: tests
tests: $(TEST_TARGETS)

# ── make server ──────────────────────────────────────────────

SERVER_BIN = $(BUILD)/server

$(SERVER_BIN): $(SRC_OBJS) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: server
server: $(SERVER_BIN)

# ── Housekeeping ─────────────────────────────────────────────

$(BUILD):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD)
