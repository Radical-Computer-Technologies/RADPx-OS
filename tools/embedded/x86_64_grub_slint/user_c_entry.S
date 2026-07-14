.section .text
.code64
.global _start
.type _start, @function
_start:
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    lea 16(%rsp,%rdi,8), %rdx
    call main
    mov %rax, %rdi
    mov $10, %rax
    int $0x80
1:
    jmp 1b
