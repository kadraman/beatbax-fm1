# BeatBax FM-1 — build and test helpers.
#
# Host targets (song model, fixtures, oracle tests):
#   make test        run full host test suite + beatbax verify oracle
#   make test-unit   compile and run C song-model unit & idempotence tests
#
# Firmware targets (JieLi pi32 toolchain under WSL/Linux):
#   From PowerShell: wsl -d Ubuntu make package
#
# Env (defaults match Felucca / local bring-up):
#   JIELI_TOOLCHAIN  toolchain root (pi32v2 clang + ld + objcopy)
#   AC79_SDK         AC79 SDK (needed by fm1pkg for uboot.boot)
#   PYTHON           host python command (default: python3)
#   CC               host C compiler (default: gcc)

PYTHON   ?= python3
CC       ?= gcc
CFLAGS   ?= -std=c99 -Wall -Wextra -Ihost/song

APP_DIR  := firmware/app
BUILD    := $(APP_DIR)/build
NAME     := beatbax-fm1-001
PRODUCT  := FM-1_1001
BIN      := $(BUILD)/$(NAME).bin
FWSC     := $(BUILD)/$(NAME).fwsc

export JIELI_TOOLCHAIN ?= $(HOME)/.jieli/toolchain
export AC79_SDK        ?= $(HOME)/.jieli/ac79-sdk

.DEFAULT_GOAL := help

.PHONY: help all clean build package test test-host test-unit

help:
	@echo "BeatBax FM-1 (firmware & host tooling)"
	@echo ""
	@echo "Host targets (standard host GCC & Python):"
	@echo "  make test        run full host test suite + beatbax verify oracle"
	@echo "  make test-unit   compile and run C song-model unit & idempotence tests"
	@echo ""
	@echo "Firmware targets (WSL/Linux with JieLi toolchain):"
	@echo "  make build       compile $(NAME).bin"
	@echo "  make package     build + pack $(NAME).fwsc (Felucca ota, $(PRODUCT))"
	@echo "  make clean       remove build and test outputs (keeps known-good/)"
	@echo "  make all         alias for package"
	@echo ""
	@echo "JIELI_TOOLCHAIN=$(JIELI_TOOLCHAIN)"
	@echo "AC79_SDK=$(AC79_SDK)"
	@echo "Outputs: $(BIN)  $(FWSC)"

all: package

# --- Host song-model targets ---

test-unit:
	$(CC) $(CFLAGS) host/song/song_model.c host/song/song_parse.c host/song/song_write.c \
		host/tests/test_roundtrip.c -o host/tests/test_roundtrip
	./host/tests/test_roundtrip

test-host:
	$(PYTHON) tools/verify-bax.py

test: test-host

# --- Firmware targets (WSL/Linux) ---

build:
	cd $(APP_DIR) && sh ./build.sh

package: build
	cd $(APP_DIR) && python3 ./pack_with_felucca_ota.py \
		build/$(NAME).bin build/$(NAME).fwsc --product $(PRODUCT)

# --- Cleanup ---

clean:
	@mkdir -p $(BUILD)
	@# Drop objects and packages in build/; never touch known-good/.
	@if [ -d "$(BUILD)" ]; then find $(BUILD) -maxdepth 1 -type f ! -path '$(BUILD)/known-good/*' -delete; fi
	rm -f host/tests/test_roundtrip host/tests/test_roundtrip.exe
	rm -f songs/gameboy/*.canon*.bax
	@echo "cleaned build and test outputs (known-good preserved)"
