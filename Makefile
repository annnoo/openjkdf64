V=1
BUILD_DIR=build/n64
SOURCE_DIR=.

include $(N64_INST)/include/n64.mk

N64_ROM_TITLE="OpenJKDF2"

# ── Minimal boot ROM ──────────────────────────────────────────────────────────
# Only compile main_n64.c for now.
# Goal: get a clean .z64 that boots and displays text.
# Engine files will be added incrementally after this works.

SRCS := src/main_n64.c

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

# ── Compile flags ─────────────────────────────────────────────────────────────

CFLAGS += \
    -DTARGET_N64 \
    -Isrc

# ── Top-level targets ─────────────────────────────────────────────────────────

.PHONY: all clean

all: openjkdf2.z64

openjkdf2.z64: N64_ROM_TITLE="OpenJKDF2"

$(BUILD_DIR)/openjkdf2.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) openjkdf2.z64
