.section .multiboot2,"a"
.align 8
mb2_header_start:
    .long 0xe85250d6
    .long 0
    .long mb2_header_end - mb2_header_start
    .long -(0xe85250d6 + 0 + (mb2_header_end - mb2_header_start))

    .align 8
    .short 5
    .short 0
    .long 20
    .long 1920
    .long 1080
    .long 32

    .align 8
    .short 0
    .short 0
    .long 8
mb2_header_end:

.section .bss
.align 16
stack_bottom:
    // 512 KiB kernel boot stack. The graphical shell (rad_slint_shell_start) runs
    // on this stack directly during init, and Slint's freestanding software
    // renderer recurses deeply through the item tree; 32 KiB overflowed into
    // adjacent .bss (page tables) with no guard page, corrupting a return address
    // and faulting wild in the Slint trampolines. 512 KiB gives ample headroom.
    .skip 524288
.global x86_boot_stack_top
x86_boot_stack_top:
stack_top:

.align 4096
.global x86_boot_pml4
x86_boot_pml4:
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd0:
    .skip 4096
pd1:
    .skip 4096
pd2:
    .skip 4096
pd3:
    .skip 4096

.section .text
.code32
.global _start
.type _start, @function
_start:
    cli
    mov $stack_top, %esp
    mov %eax, boot_magic
    mov %ebx, boot_info

    call setup_page_tables
    call enable_long_mode

    lgdt gdt64_ptr
    ljmp $0x08, $long_mode_entry

setup_page_tables:
    mov $pml4, %edi
    xor %eax, %eax
    mov $6144, %ecx
    rep stosl

    mov $pdpt, %eax
    or $0x07, %eax
    mov %eax, pml4

    mov $pd0, %eax
    or $0x07, %eax
    mov %eax, pdpt
    mov $pd1, %eax
    or $0x07, %eax
    mov %eax, pdpt+8
    mov $pd2, %eax
    or $0x07, %eax
    mov %eax, pdpt+16
    mov $pd3, %eax
    or $0x07, %eax
    mov %eax, pdpt+24

    mov $0, %ecx
1:
    mov %ecx, %eax
    shl $21, %eax
    or $0x87, %eax
    mov %eax, pd0(,%ecx,8)
    movl $0, pd0+4(,%ecx,8)
    inc %ecx
    cmp $2048, %ecx
    jne 1b
    ret

enable_long_mode:
    mov $pml4, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    mov $0xc0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0
    ret

.align 8
gdt64:
    .quad 0
    .quad 0x00af9a000000ffff
    .quad 0x00af92000000ffff
gdt64_ptr:
    .word gdt64_ptr - gdt64 - 1
    .long gdt64

.code64
long_mode_entry:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    mov $stack_top, %rsp
    xor %rbp, %rbp
    call x86_run_global_constructors
    mov boot_magic(%rip), %edi
    mov boot_info(%rip), %esi
    call kernel_main64
halt:
    cli
    hlt
    jmp halt

.section .text.ap_trampoline,"ax"
.global x86_ap_trampoline_start
.global x86_ap_trampoline_end
.code16
x86_ap_trampoline_start:
    cli
    cld
    mov %cs, %ax
    mov %ax, %ds
    mov %ax, %ss
    mov $0x0ff0, %sp
    movl $0x7000, %esi
    lgdt %cs:(ap_gdt_ptr - x86_ap_trampoline_start)
    mov %cr0, %eax
    or $0x1, %eax
    mov %eax, %cr0
    ljmpl $0x18, $(0x7000 + ap_protected - x86_ap_trampoline_start)

.code32
ap_protected:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    mov (ap_pml4 - x86_ap_trampoline_start)(%esi), %eax
    mov %eax, %cr3
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4
    mov $0xc0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0
    movl $(0x7000 + ap_long_mode - x86_ap_trampoline_start), (ap_long_jump - x86_ap_trampoline_start)(%esi)
    ljmp *(ap_long_jump - x86_ap_trampoline_start)(%esi)

.code64
ap_long_mode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    movq (ap_stack - x86_ap_trampoline_start)(%rsi), %rsp
    xor %rbp, %rbp
    movl (ap_core - x86_ap_trampoline_start)(%rsi), %edi
    movq (ap_entry - x86_ap_trampoline_start)(%rsi), %rax
    call *%rax
1:
    cli
    hlt
    jmp 1b

.align 8
ap_gdt:
    .quad 0
    .quad 0x00af9a000000ffff
    .quad 0x00cf92000000ffff
    .quad 0x00cf9a000000ffff
ap_gdt_ptr:
    .word ap_gdt_ptr - ap_gdt - 1
    .long 0x7000 + ap_gdt - x86_ap_trampoline_start
ap_long_jump:
    .long 0
    .word 0x08
ap_pml4:
    .long 0
ap_core:
    .long 0
ap_stack:
    .quad 0
ap_entry:
    .quad 0
x86_ap_trampoline_end:

.section .rodata
.global x86_ap_trampoline_pml4_offset
.global x86_ap_trampoline_core_offset
.global x86_ap_trampoline_stack_offset
.global x86_ap_trampoline_entry_offset
x86_ap_trampoline_pml4_offset:
    .long ap_pml4 - x86_ap_trampoline_start
x86_ap_trampoline_core_offset:
    .long ap_core - x86_ap_trampoline_start
x86_ap_trampoline_stack_offset:
    .long ap_stack - x86_ap_trampoline_start
x86_ap_trampoline_entry_offset:
    .long ap_entry - x86_ap_trampoline_start

.section .data
.align 8
boot_magic:
    .long 0
boot_info:
    .long 0
