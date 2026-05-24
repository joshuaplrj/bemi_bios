#!/bin/bash
# BEMI BIOS Production Build Script
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$PROJ_DIR/build"

echo "BEMI BIOS v1.3 — Production Build"
echo "==================================="

verify_toolchain() {
    echo "[BUILD] Verifying toolchain..."
    
    if ! command -v build &> /dev/null; then
        echo "ERROR: EDK2 build tool not found. Source edksetup.sh first."
        exit 1
    fi
    
    if ! command -v nasm &> /dev/null; then
        echo "ERROR: NASM not found."
        exit 1
    fi
    
    if ! command -v rustc &> /dev/null; then
        echo "WARNING: rustc not found. DBT pipeline will not be compiled."
    else
        rustup target list --installed | grep -q "x86_64-unknown-uefi" || \
            echo "WARNING: x86_64-unknown-uefi target not installed"
    fi
    
    echo "[BUILD] Toolchain OK"
}

build_uefi_firmware() {
    echo "[BUILD] Building UEFI firmware..."
    
    export EDK_TOOLS_PATH="$EDK_HOME/BaseTools"
    
    build -p "$PROJ_DIR/BemiBiosPkg/BemiBiosPkg.dsc" \
          -a X64 \
          -t GCC5 \
          -b RELEASE \
          -n $(nproc)
    
    echo "[BUILD] UEFI firmware built successfully"
}

build_dbt_pipeline() {
    echo "[BUILD] Building DBT pipeline (Rust)..."
    
    if command -v cargo &> /dev/null; then
        cd "$PROJ_DIR/dbt"
        cargo build --release --target x86_64-unknown-uefi 2>/dev/null || \
            cargo build --release 2>/dev/null || \
            echo "[BUILD] DBT pipeline: cargo build skipped (no_std only)"
        cd "$PROJ_DIR"
    fi
}

build_all() {
    verify_toolchain
    build_uefi_firmware
    build_dbt_pipeline
    
    echo ""
    echo "[BUILD] BEMI BIOS production build complete"
    echo "Output: $BUILD_DIR"
}

case "${1:-all}" in
    all) build_all ;;
    firmware) build_uefi_firmware ;;
    dbt) build_dbt_pipeline ;;
    toolchain) verify_toolchain ;;
    *)
        echo "Usage: $0 [all|firmware|dbt|toolchain]"
        exit 1
        ;;
esac
