.section .text
.global _start
.type _start, %function

.equ SYS_WRITE, 1
.equ SYS_EXIT, 10
.equ SYS_EXECVE, 12
.equ SYS_GETPID, 14
.equ SYS_GETTIMEOFDAY, 8
.equ SYS_GETCWD, 19

_start:
    mov x8, #SYS_GETPID
    svc #0
    cmp x0, #1
    b.lt fail

    ldr x0, =timeval_storage
    mov x8, #SYS_GETTIMEOFDAY
    svc #0
    cbnz x0, fail

    ldr x0, =cwd_storage
    mov x1, #128
    mov x8, #SYS_GETCWD
    svc #0
    cbnz x0, fail

    mov x0, #1
    mov x1, #0x1000
    mov x2, #1
    mov x8, #SYS_WRITE
    svc #0
    cmn x0, #2
    b.ne fail
    ldr x0, =invalid_ptr_msg
    bl write_cstr

    ldr x0, =syscalls_msg
    bl write_cstr
    ldr x0, =init_msg
    bl write_cstr

    ldr x0, =radsh_path
    ldr x1, =radsh_argv
    mov x2, #0
    mov x8, #SYS_EXECVE
    svc #0

fail:
    mov x0, #1
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
invalid_ptr_msg:
    .asciz "RADIX_AARCH64_USER_INVALID_PTR_OK\n"
syscalls_msg:
    .asciz "RADIX_AARCH64_USER_SYSCALLS_OK\n"
init_msg:
    .asciz "RADIX_AARCH64_USER_INIT_OK\n"
radsh_path:
    .asciz "/bin/radsh"
radsh_arg0:
    .asciz "radsh"

.section .data
.align 3
radsh_argv:
    .quad radsh_arg0
    .quad 0

.section .bss
.align 3
timeval_storage:
    .skip 16
cwd_storage:
    .skip 128
