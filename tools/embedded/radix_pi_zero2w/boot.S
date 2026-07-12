.section .text.boot, "ax"
.global _start
.type _start, %function

_start:
    msr daifset, #0xf
    mrs x4, mpidr_el1
    and x4, x4, #0xff
    cbz x4, 2f

1:
    wfe
    b 1b

2:
    mov x19, x0
    ldr x1, =__bss_start
    ldr x2, =__bss_end
3:
    cmp x1, x2
    b.hs 4f
    str xzr, [x1], #8
    b 3b

4:
    ldr x1, =radix_pi_boot_argument
    str x19, [x1]
    ldr x30, =__stack_top
    mov sp, x30
    mov x0, x19
    bl radix_pi_entry

5:
    wfe
    b 5b

.section .bss.boot, "aw", %nobits
.align 3
.global radix_pi_boot_argument
radix_pi_boot_argument:
    .skip 8

.section .bss.stack, "aw", %nobits
.align 12
.global __stack_bottom
__stack_bottom:
    .skip 65536
.global __stack_top
__stack_top:
