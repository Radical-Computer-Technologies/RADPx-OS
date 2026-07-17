.section .text
.global _start
.type _start, %function

.equ SYS_READ, 0
.equ SYS_WRITE, 1
.equ SYS_OPEN, 2
.equ SYS_CLOSE, 3
.equ SYS_EXIT, 10

_start:
    mov x20, sp
    ldr x0, [x20]
    cmp x0, #2
    b.lt fail
    add x21, x20, #8
    ldr x22, [x21, #8]
    cbz x22, fail
    ldr x0, =argv_envp_msg
    bl write_cstr

    mov x0, x22
    mov x1, #1
    mov x8, #SYS_OPEN
    svc #0
    cmp x0, #0
    b.lt fail
    mov x19, x0
    mov x0, x19
    ldr x1, =script_buf
    mov x2, #16
    mov x8, #SYS_READ
    svc #0
    cmp x0, #2
    b.lt fail
    ldr x1, =script_buf
    ldrb w2, [x1]
    cmp w2, #'#'
    b.ne fail
    ldrb w2, [x1, #1]
    cmp w2, #'!'
    b.ne fail
    mov x0, x19
    mov x8, #SYS_CLOSE
    svc #0

    ldr x0, =script_msg
    bl write_cstr
    ldr x0, =radsh_exit_msg
    bl write_cstr
    mov x0, #0
    mov x8, #SYS_EXIT
    svc #0

fail:
    mov x0, #3
    mov x8, #SYS_EXIT
    svc #0
1:
    b 1b

write_cstr:
    mov x3, x0
    mov x2, #0
2:
    ldrb w4, [x3, x2]
    cbz w4, 3f
    add x2, x2, #1
    b 2b
3:
    mov x1, x3
    mov x0, #1
    mov x8, #SYS_WRITE
    svc #0
    ret

.section .rodata
argv_envp_msg:
    .asciz "RADIX_AARCH64_USER_ARGV_ENVP_OK\n"
script_msg:
    .asciz "RADIX_AARCH64_USER_SH_SCRIPT_OK\n"
radsh_exit_msg:
    .asciz "RADIX_AARCH64_USER_RADSH_EXIT_OK\n"

.section .bss
.align 3
script_buf:
    .skip 32
