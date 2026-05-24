# BEMI BIOS Production Build Makefile

PROJ_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
BUILD_DIR := $(PROJ_DIR)/build
DEPLOY_DIR := $(PROJ_DIR)/deploy

EDK2_DSC := $(PROJ_DIR)/BemiBiosPkg/BemiBiosPkg.dsc
C_SOURCES := $(shell find $(PROJ_DIR) -name '*.c' -o -name '*.nasm')
RUST_SOURCES := $(shell find $(PROJ_DIR)/dbt -name '*.rs' 2>/dev/null)

.PHONY: all clean firmware dbt deploy test qemu qemu-native qemu-legacy

all: firmware

firmware: $(BUILD_DIR)/BemiBiosCore.efi

$(BUILD_DIR)/BemiBiosCore.efi: $(EDK2_DSC) $(C_SOURCES)
	@echo "Building BEMI BIOS UEFI firmware..."
	@mkdir -p $(BUILD_DIR)
	build -p $(EDK2_DSC) -a X64 -t GCC5 -b RELEASE -n $$(nproc) \
		--buildtarget=RELEASE \
		--tagname=GCC5
	@cp Build/BemiBios/RELEASE_GCC5/X64/BemiBiosCore.efi $(BUILD_DIR)/
	@mkdir -p $(PROJ_DIR)/tests/qemu
	@cp Build/BemiBios/RELEASE_GCC5/X64/QemuRoundTrip.efi $(PROJ_DIR)/tests/qemu/ 2>/dev/null || true
	@echo "Firmware built: $(BUILD_DIR)/BemiBiosCore.efi"

dbt:
	@echo "Building DBT pipeline (Rust)..."
	@if command -v cargo >/dev/null 2>&1; then \
		cd $(PROJ_DIR)/dbt && cargo build --release 2>/dev/null; \
	else \
		echo "cargo not found, skipping DBT build"; \
	fi

deploy: firmware
	@echo "Preparing deployment..."
	@mkdir -p $(DEPLOY_DIR)/EFI/BOOT
	@cp $(BUILD_DIR)/BemiBiosCore.efi $(DEPLOY_DIR)/EFI/BOOT/BOOTX64.EFI
	@echo "Deployment ready at $(DEPLOY_DIR)"

test: firmware
	@echo "Running test suite..."
	@$(PROJ_DIR)/scripts/qemu_test.sh native

qemu: deploy
	@echo "Launching QEMU with BEMI BIOS..."
	qemu-system-x86_64 \
		-machine q35,accel=kvm \
		-cpu host \
		-m 4G \
		-smp 4 \
		-serial stdio \
		-nographic \
		-no-reboot \
		-drive if=pflash,format=raw,file=/usr/share/ovmf/OVMF.fd,readonly=on \
		-drive file=/usr/share/ovmf/OVMF_VARS.fd,if=pflash,format=raw \
		-drive file=fat:rw:$(DEPLOY_DIR),format=raw

qemu-legacy: firmware
	@echo "Launching QEMU with legacy BIOS boot..."
	@dd if=/dev/zero of=/tmp/bemi_boot.img bs=512 count=2880 2>/dev/null
	qemu-system-x86_64 \
		-machine q35 \
		-cpu host \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-nographic \
		-no-reboot \
		-drive file=/tmp/bemi_boot.img,format=raw

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(PROJ_DIR)/Build
	rm -rf $(PROJ_DIR)/dbt/target
	find $(PROJ_DIR) -name '*.pyc' -delete
	@echo "Clean complete"
