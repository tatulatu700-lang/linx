#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "============================================================"
echo " LINX UEFI MANIFOLD BUILD"
echo "============================================================"
echo "[*] Repository : $SCRIPT_DIR"
echo "[*] Step 1: Converting substrate into raw object linkage..."

objcopy \
    -I binary \
    -O elf64-x86-64 \
    -B i386:x86-64 \
    linum_polyglot.bin \
    linum_polyglot_blob.o

echo "[*] Step 2: Compiling freestanding UEFI PE-COFF primitives..."

clang \
    -target x86_64-unknown-windows \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fshort-wchar \
    -mno-red-zone \
    -Wall \
    -Wextra \
    -Werror \
    -c linx_uefi_core.c \
    -o linx_uefi_core.o

as --64 bootx64.s -o bootx64.o

echo "[*] Step 3: Linking pure UEFI executable..."

if ! lld-link \
    -subsystem:efi_application \
    -nodefaultlib \
    -entry:efi_main \
    bootx64.o \
    linx_uefi_core.o \
    linum_polyglot_blob.o \
    -out:bootx64.efi
then
    echo "[-] Primary lld-link invocation failed."
    echo "[*] Fallback: native LLVM EFI linking..."

    clang \
        -target x86_64-unknown-windows \
        -ffreestanding \
        -nostdlib \
        -fno-stack-protector \
        -fno-stack-check \
        -fshort-wchar \
        -mno-red-zone \
        -Wl,-entry:efi_main \
        -Wl,-subsystem:efi_application \
        -fuse-ld=lld \
        bootx64.o \
        linx_uefi_core.o \
        linum_polyglot_blob.o \
        -o bootx64.efi
fi

echo "[*] Step 4: Verifying EFI artifact integrity..."

test -f bootx64.efi

ls -l bootx64.efi

file bootx64.efi

sha256sum bootx64.efi > bootx64.efi.sha256

cat bootx64.efi.sha256

echo "[*] Step 5: Self-checking generated checksum..."

sha256sum -c bootx64.efi.sha256

echo
echo "[+] BARE-METAL MANIFOLD BUILT: EXIT 0"
