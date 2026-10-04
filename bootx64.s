.intel_syntax noprefix
.text
.globl efi_main

efi_main:
    # MS x64 ABI: RCX = ImageHandle, RDX = SystemTable
    sub rsp, 40
    and rsp, -16
    call linx_uefi_dispatch
    add rsp, 40
    ret
