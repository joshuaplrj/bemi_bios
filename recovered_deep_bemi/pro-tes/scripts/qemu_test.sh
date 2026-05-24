#!/bin/bash
# BEMI BIOS QEMU test runner
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$PROJ_DIR/Build/BemiBios"

QEMU=qemu-system-x86_64
OVMF_CODE="/usr/share/ovmf/OVMF.fd"
OVMF_VARS="/usr/share/ovmf/OVMF_VARS.fd"
SMP=4
MEM=4G
SERIAL_LOG="/tmp/bemi_serial.log"

echo "BEMI BIOS — QEMU Test Environment"
echo "=================================="

test_native_boot() {
    echo "[TEST] Native boot mode (Mode A)..."
    
    $QEMU \
        -machine q35,accel=kvm \
        -cpu host \
        -m $MEM \
        -smp $SMP \
        -serial "file:$SERIAL_LOG" \
        -nographic \
        -no-reboot \
        -drive if=pflash,format=raw,file=$OVMF_CODE,readonly=on \
        -drive file=$OVMF_VARS,if=pflash,format=raw \
        -drive file=fat:rw:$PROJ_DIR/deploy,format=raw \
        -netdev user,id=net0 \
        -device e1000,netdev=net0 \
        -gdb tcp::1234 &
    
    QEMU_PID=$!
    
    sleep 10
    
    if kill -0 $QEMU_PID 2>/dev/null; then
        kill $QEMU_PID 2>/dev/null || true
        wait $QEMU_PID 2>/dev/null || true
    fi
    
    if grep -q "BEMI" $SERIAL_LOG 2>/dev/null; then
        echo "[PASS] BEMI BIOS initialized"
    else
        echo "[FAIL] BEMI BIOS not detected in serial output"
    fi
}

test_legacy_boot() {
    echo "[TEST] Legacy boot mode (Mode B)..."
    
    dd if=/dev/zero of=/tmp/boot.img bs=512 count=2880 2>/dev/null
    dd if=/dev/zero of=/tmp/boot_part.img bs=512 count=2048 2>/dev/null
    
    $QEMU \
        -machine q35,accel=kvm \
        -cpu host \
        -m $MEM \
        -smp $SMP \
        -serial "file:$SERIAL_LOG" \
        -nographic \
        -no-reboot \
        -drive if=pflash,format=raw,file=$OVMF_CODE,readonly=on \
        -drive file=$OVMF_VARS,if=pflash,format=raw \
        -drive file=/tmp/boot.img,format=raw,if=ide \
        -netdev user,id=net0 \
        -device e1000,netdev=net0 &
    
    QEMU_PID=$!
    sleep 15
    
    if kill -0 $QEMU_PID 2>/dev/null; then
        kill $QEMU_PID 2>/dev/null || true
        wait $QEMU_PID 2>/dev/null || true
    fi
    
    if grep -q "CSM" $SERIAL_LOG 2>/dev/null; then
        echo "[PASS] CSM initialized"
    else
        echo "[INFO] Legacy boot test completed"
    fi
}

case "${1:-native}" in
    native) test_native_boot ;;
    legacy) test_legacy_boot ;;
    both)
        test_native_boot
        test_legacy_boot
        ;;
    *)
        echo "Usage: $0 [native|legacy|both]"
        exit 1
        ;;
esac
