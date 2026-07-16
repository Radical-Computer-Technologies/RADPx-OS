.section .text
.global _start
.type _start, %function

#ifndef RADIX_USER_MAIN
#define RADIX_USER_MAIN main
#endif

_start:
    ldr x0, [sp]
    add x1, sp, #8
    add x2, x1, x0, lsl #3
    add x2, x2, #8
    bl RADIX_USER_MAIN
    mov x8, #10
    svc #0
1:
    wfe
    b 1b
