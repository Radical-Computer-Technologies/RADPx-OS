.section .text
.global _start
.type _start, %function

.equ SYS_READ, 0
.equ SYS_WRITE, 1
.equ SYS_EXIT, 10
.equ SYS_FORK, 11
.equ SYS_EXECVE, 12
.equ SYS_WAITPID, 13
.equ SYS_GETPID, 14
.equ SYS_GETCWD, 19
.equ SYS_PIPE, 21

_start:
    mov x8, #SYS_GETPID
    svc #0
    cmp x0, #1
    b.lt fail

    ldr x0, =cwd_storage
    mov x1, #128
    mov x8, #SYS_GETCWD
    svc #0
    cbnz x0, fail

    ldr x0, =radsh_boot_msg
    bl write_cstr

    ldr x0, =pipefds
    mov x8, #SYS_PIPE
    svc #0
    cbnz x0, fail

    ldr x1, =cow_byte
    mov w2, #'P'
    strb w2, [x1]

    mov x8, #SYS_FORK
    svc #0
    cbz x0, child_path
    mov x19, x0

parent_path:
    ldr x0, =fork_msg
    bl write_cstr
    mov x0, x19
    ldr x1, =wait_status
    mov x2, #0
    mov x8, #SYS_WAITPID
    svc #0
    cmp x0, x19
    b.ne fail
    ldr x0, =fork_wait_msg
    bl write_cstr
    ldr x0, =pipefds
    ldr w0, [x0]
    ldr x1, =pipe_byte
    mov x2, #1
    mov x8, #SYS_READ
    svc #0
    cmp x0, #1
    b.ne fail
    ldr x0, =pipe_byte
    ldrb w0, [x0]
    cmp w0, #'K'
    b.ne fail
    ldr x0, =fd_inherit_msg
    bl write_cstr
    ldr x1, =cow_byte
    ldrb w2, [x1]
    cmp w2, #'P'
    b.ne fail
    ldr x0, =cow_parent_msg
    bl write_cstr

    ldr x0, =script_path
    ldr x1, =script_argv
    mov x2, #0
    mov x8, #SYS_EXECVE
    svc #0
    b fail

child_path:
    ldr x1, =cow_byte
    mov w2, #'C'
    strb w2, [x1]
    ldr x0, =fork_child_msg
    bl write_cstr
    ldr x0, =pipefds
    ldr w0, [x0, #4]
    ldr x1, =pipe_payload
    mov x2, #1
    mov x8, #SYS_WRITE
    svc #0
    mov x0, #7
    mov x8, #SYS_EXIT
    svc #0

fail:
    mov x0, #2
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
radsh_boot_msg:
    .asciz "RADIX_AARCH64_USER_RADSH_BOOT_OK\n"
fork_msg:
    .asciz "RADIX_AARCH64_USER_FORK_OK\n"
fork_child_msg:
    .asciz "RADIX_AARCH64_USER_FORK_CHILD_OK\n"
fork_wait_msg:
    .asciz "RADIX_AARCH64_USER_FORK_WAIT_OK\n"
fd_inherit_msg:
    .asciz "RADIX_AARCH64_USER_FORK_FD_INHERIT_OK\n"
cow_parent_msg:
    .asciz "RADIX_AARCH64_USER_COW_PARENT_ISOLATED_OK\n"
script_path:
    .asciz "/bin/test.sh"
script_arg0:
    .asciz "test.sh"
pipe_payload:
    .ascii "K"

.section .data
.align 3
script_argv:
    .quad script_arg0
    .quad 0
cow_byte:
    .byte 'P'

.section .bss
.align 3
cwd_storage:
    .skip 128
pipefds:
    .skip 8
wait_status:
    .skip 4
pipe_byte:
    .skip 1
