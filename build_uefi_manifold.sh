#!/usr/bin/env bash
set -euo pipefail
cd /home/ron/linx

echo "============================================================"
echo " LINX UEFI MANIFOLD BUILD: SUBSTRATE BARE METAL (PE32+)"
echo "============================================================"

# Compile substrate container into executable section
cat << 'BLOB_EOF' > substrate_anchor.s
.section .text,"rx"
.globl _binary_linum_polyglot_bin_start
.globl _binary_linum_polyglot_bin_end
.balign 16
_binary_linum_polyglot_bin_start:
    .incbin "linum_polyglot.bin"
_binary_linum_polyglot_bin_end:
BLOB_EOF

clang \
    --target=x86_64-unknown-windows-msvc \
    -c substrate_anchor.s \
    -o substrate_blob.obj
rm -f substrate_anchor.s

clang \
    --target=x86_64-unknown-windows-msvc \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fshort-wchar \
    -mno-red-zone \
    -Wall \
    -Wextra \
    -Werror \
    -c linx_uefi_core.c \
    -o linx_uefi_core.obj

lld-link \
    -subsystem:efi_application \
    -nodefaultlib \
    -entry:efi_main \
    -base:0x140000000 \
    -dynamicbase \
    linx_uefi_core.obj \
    substrate_blob.obj \
    -out:bootx64.efi

rm -f linx_uefi_core.obj substrate_blob.obj

sha256sum bootx64.efi > bootx64.efi.sha256
echo "[+] EFI SHA256: $(cat bootx64.efi.sha256)"
echo "[+] BARE-METAL UEFI MANIFOLD READY: EXIT 0"
