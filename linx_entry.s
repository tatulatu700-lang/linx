.intel_syntax noprefix
.text
.globl _start

_start:
    xor rbp, rbp
    mov rdi, [rsp]
    lea rsi, [rsp + 8]
    and rsp, -16
    call linx_main
    mov rdi, rax
    mov rax, 60
    syscall
