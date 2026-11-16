# BeatBax FM-1 — build helpers for the feature 001 bring-up app.
# Run under WSL/Linux (JieLi pi32 toolchain). From PowerShell:
#   wsl -d Ubuntu make package
#
# Env (defaults match Felucca / local bring-up):
#   JIELI_TOOLCHAIN  toolchain root (pi32v2 clang + ld + objcopy)
#   AC79_SDK         AC79 SDK (needed by fm1pkg for uboot.boot)

APP_DIR  := firmware/app
BUILD    := $(APP_DIR)/build
NAME     := beatbax-fm1-001
PRODUCT  := FM-1_1001
BIN      := $(BUILD)/$(NAME).bin
FWSC     := $(BUILD)/$(NAME).fwsc

export JIELI_TOOLCHAIN ?= $(HOME)/.jieli/toolchain
export AC79_SDK        ?= $(HOME)/.jieli/ac79-sdk

.DEFAULT_GOAL := help

.PHONY: help all clean build package

help:
	@echo "BeatBax FM-1 (feature 001 bring-up)"
	@echo ""
	@echo "  make build     compile $(NAME).bin"
	@echo "  make package   build + pack $(NAME).fwsc (Felucca ota, $(PRODUCT))"
	@echo "  make clean     remove build outputs (keeps known-good/)"
	@echo "  make all       alias for package"
	@echo ""
	@echo "JIELI_TOOLCHAIN=$(JIELI_TOOLCHAIN)"
	@echo "AC79_SDK=$(AC79_SDK)"
	@echo "Outputs: $(BIN)  $(FWSC)"

all: package

build:
	cd $(APP_DIR) && sh ./build.sh

package: build
	cd $(APP_DIR) && python3 ./pack_with_felucca_ota.py \
		build/$(NAME).bin build/$(NAME).fwsc --product $(PRODUCT)

clean:
	@mkdir -p $(BUILD)
	@# Drop objects and packages in build/; never touch known-good/.
	find $(BUILD) -maxdepth 1 -type f ! -path '$(BUILD)/known-good/*' -delete
	@echo "cleaned $(BUILD) (known-good preserved)"
