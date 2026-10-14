#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Exercise real BIOS execution after the CLI writes and verifies a NEW regular
# image. No USB device, root privilege, downloaded OS, or bundled BIOS is used.
set -euo pipefail

if [[ $# -ne 1 ]]; then
    printf 'Usage: %s /path/to/kavrune-cli\n' "$0" >&2
    exit 2
fi
cli=$1
qemu=${QEMU:-qemu-system-x86_64}
if ! command -v "$qemu" >/dev/null 2>&1; then
    printf 'SKIP: QEMU is unavailable; install qemu-system-x86 to run the BIOS boot test.\n'
    exit 77
fi
for utility in as ld dd truncate cmp timeout stat; do
    if ! command -v "$utility" >/dev/null 2>&1; then
        printf 'SKIP: BIOS test dependency is unavailable: %s\n' "$utility"
        exit 77
    fi
done
if [[ ! -x $cli ]]; then
    printf 'Kavrune CLI is not executable: %s\n' "$cli" >&2
    exit 2
fi
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
test_dir=$(mktemp -d /tmp/kavrune-boot.XXXXXX)
trap 'rm -rf -- "$test_dir"' EXIT

as --32 -o "$test_dir/boot.o" "$script_dir/../tests/boot-sector.S"
ld -m elf_i386 -Ttext 0x7c00 --oformat binary -e _start \
    -o "$test_dir/boot.bin" "$test_dir/boot.o"
if [[ $(stat -c %s -- "$test_dir/boot.bin") -ne 512 ]]; then
    printf 'The original BIOS test sector must contain exactly 512 bytes.\n' >&2
    exit 1
fi
truncate -s 4194304 -- "$test_dir/source.img"
dd if="$test_dir/boot.bin" of="$test_dir/source.img" conv=notrunc status=none
"$cli" simulate "$test_dir/source.img" "$test_dir/flashed.img"
cmp -- "$test_dir/source.img" "$test_dir/flashed.img"

firmware_options=()
if [[ -n ${QEMU_FIRMWARE_DIR:-} ]]; then
    firmware_options+=(-L "$QEMU_FIRMWARE_DIR")
fi
if [[ -n ${QEMU_BIOS:-} ]]; then
    firmware_options+=(-bios "$QEMU_BIOS")
fi
set +e
timeout --signal=TERM --kill-after=2s 10s "$qemu" \
    -machine pc,accel=tcg -cpu qemu32 -m 32 -boot order=c,strict=on \
    -display none -vga none -monitor none -serial none -nic none \
    -no-reboot \
    -drive "file=$test_dir/flashed.img,format=raw,if=ide,snapshot=on" \
    -debugcon "file:$test_dir/debug.log" \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    "${firmware_options[@]}" >"$test_dir/qemu.log" 2>&1
qemu_status=$?
set -e
if [[ $qemu_status -ne 33 ]]; then
    printf 'BIOS boot test failed: QEMU returned %s; expected isa-debug-exit status 33.\n' "$qemu_status" >&2
    cat -- "$test_dir/qemu.log" >&2
    exit 1
fi
# IDE firmware expects a writable guest disk. snapshot=on keeps guest writes
# in a disposable QEMU overlay; assert the verified backing image is untouched.
cmp -- "$test_dir/source.img" "$test_dir/flashed.img"
printf %s KAVRUNE_BOOT_OK >"$test_dir/expected.log"
if ! cmp -- "$test_dir/expected.log" "$test_dir/debug.log"; then
    printf 'BIOS boot test failed: the guest did not print the exact expected marker.\n' >&2
    exit 1
fi
printf 'BIOS boot verified: KAVRUNE_BOOT_OK, QEMU exit 33; only temporary regular files were used.\n'
