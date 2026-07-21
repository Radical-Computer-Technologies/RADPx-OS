.section .text
.code64

.global x86_load_gdt
.type x86_load_gdt, @function
x86_load_gdt:
    lgdt (%rdi)
    pushq $0x08
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    ret

.global x86_load_idt
.type x86_load_idt, @function
x86_load_idt:
    lidt (%rdi)
    ret

.macro IRQ_STUB vector
.global x86_irq_\vector\()_entry
.type x86_irq_\vector\()_entry, @function
x86_irq_\vector\()_entry:
    pushq $\vector
    jmp x86_irq_common
.endm

IRQ_STUB 32
IRQ_STUB 33
IRQ_STUB 34
IRQ_STUB 35
IRQ_STUB 36
IRQ_STUB 37
IRQ_STUB 38
IRQ_STUB 39
IRQ_STUB 40
IRQ_STUB 41
IRQ_STUB 42
IRQ_STUB 43
IRQ_STUB 44
IRQ_STUB 45
IRQ_STUB 46
IRQ_STUB 47

.global x86_lapic_timer_entry
.type x86_lapic_timer_entry, @function
x86_lapic_timer_entry:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    mov 128(%rsp), %rdi
    # Preserve the interrupted FPU/SSE (XMM) state across the handler. The kernel and the
    # Slint software renderer use SSE heavily; without this, an interrupt landing mid-SSE
    # computation would let the handler clobber XMM and corrupt the interrupted code.
    mov %rsp, %rbp
    sub $512, %rsp
    and $-16, %rsp
    fxsave (%rsp)
    call x86_lapic_timer_dispatch
    fxrstor (%rsp)
    mov %rbp, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    iretq

x86_irq_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    mov 120(%rsp), %rdi
    # Preserve the interrupted FPU/SSE (XMM) state across the handler (see note in the
    # LAPIC timer entry). PS/2 keyboard/mouse and PIT IRQs arrive here and must not clobber
    # the XMM registers of interrupted SSE code (e.g. the Slint renderer / layout math).
    mov %rsp, %rbp
    sub $512, %rsp
    and $-16, %rsp
    fxsave (%rsp)
    call x86_irq_dispatch
    fxrstor (%rsp)
    mov %rbp, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $8, %rsp
    iretq

.global x86_int80_entry
.type x86_int80_entry, @function
x86_int80_entry:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbp
    pushq %rbx
    pushq %r9
    pushq %r8
    pushq %r10
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax
    mov %rsp, %rdi
    call x86_syscall_dispatch_frame
    movabs $0x5241445845584954, %r10
    cmp %r10, %rax
    je x86_user_exit_trampoline
    mov %rax, 0(%rsp)
    popq %rax
    popq %rdi
    popq %rsi
    popq %rdx
    popq %r10
    popq %r8
    popq %r9
    popq %rbx
    popq %rbp
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    iretq

.global x86_syscall_entry
.type x86_syscall_entry, @function
x86_syscall_entry:
    pushq %r9
    mov %r8, %r9
    mov %r10, %r8
    mov %rdx, %rcx
    mov %rsi, %rdx
    mov %rdi, %rsi
    mov %rax, %rdi
    call x86_syscall_dispatch
    addq $8, %rsp
    sysretq

.global x86_exception_no_error_entry
.type x86_exception_no_error_entry, @function
x86_exception_no_error_entry:
    pushq $0
    pushq $0xffffffffffffffff
    jmp x86_exception_common

.global x86_exception_error_entry
.type x86_exception_error_entry, @function
x86_exception_error_entry:
    pushq $0xffffffffffffffff
    jmp x86_exception_common

.global x86_invalid_opcode_entry
.type x86_invalid_opcode_entry, @function
x86_invalid_opcode_entry:
    pushq $0
    pushq $6
    jmp x86_exception_common

.global x86_device_not_available_entry
.type x86_device_not_available_entry, @function
x86_device_not_available_entry:
    pushq $0
    pushq $7
    jmp x86_exception_common

.global x86_double_fault_entry
.type x86_double_fault_entry, @function
x86_double_fault_entry:
    pushq $8
    jmp x86_exception_common

.global x86_invalid_tss_entry
.type x86_invalid_tss_entry, @function
x86_invalid_tss_entry:
    pushq $10
    jmp x86_exception_common

.global x86_segment_not_present_entry
.type x86_segment_not_present_entry, @function
x86_segment_not_present_entry:
    pushq $11
    jmp x86_exception_common

.global x86_stack_segment_entry
.type x86_stack_segment_entry, @function
x86_stack_segment_entry:
    pushq $12
    jmp x86_exception_common

.global x86_general_protection_entry
.type x86_general_protection_entry, @function
x86_general_protection_entry:
    pushq $13
    jmp x86_exception_common

.global x86_page_fault_entry
.type x86_page_fault_entry, @function
x86_page_fault_entry:
    pushq $14
    jmp x86_exception_common

.global x86_floating_point_entry
.type x86_floating_point_entry, @function
x86_floating_point_entry:
    pushq $0
    pushq $16
    jmp x86_exception_common

.global x86_alignment_check_entry
.type x86_alignment_check_entry, @function
x86_alignment_check_entry:
    pushq $17
    jmp x86_exception_common

.global x86_simd_floating_point_entry
.type x86_simd_floating_point_entry, @function
x86_simd_floating_point_entry:
    pushq $0
    pushq $19
    jmp x86_exception_common

.global x86_control_protection_entry
.type x86_control_protection_entry, @function
x86_control_protection_entry:
    pushq $21
    jmp x86_exception_common

x86_exception_common:
    cmpq $14, 0(%rsp)
    jne 2f
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    mov 128(%rsp), %rdi
    # Preserve FPU/SSE (XMM) across the page-fault handler: a #PF taken inside SSE code
    # (e.g. the Slint renderer touching a COW/demand page) returns to that code, so the
    # handler must not leave XMM clobbered.
    mov %rsp, %rbp
    sub $512, %rsp
    and $-16, %rsp
    fxsave (%rsp)
    call x86_page_fault_dispatch
    fxrstor (%rsp)
    mov %rbp, %rsp
    test %rax, %rax
    jz 3f
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq
3:
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
2:
    mov 0(%rsp), %rdi
    mov 8(%rsp), %rsi
    mov 16(%rsp), %rdx
    mov 24(%rsp), %rcx
    call x86_exception_report
1:
    cli
    hlt
    jmp 1b

.global x86_test_int80_getpid
.type x86_test_int80_getpid, @function
x86_test_int80_getpid:
    mov $14, %rax
    int $0x80
    ret

.global x86_enter_user
.type x86_enter_user, @function
x86_enter_user:
    mov %rsp, x86_user_kernel_rsp(%rip)
    mov $0x1b, %ax
    mov %ax, %ds
    mov %ax, %es
    pushq $0x1b
    pushq %rsi
    pushfq
    popq %rax
    orq $0x200, %rax
    pushq %rax
    pushq $0x23
    pushq %rdi
    iretq

.global x86_enter_user_context
.type x86_enter_user_context, @function
x86_enter_user_context:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbp
    pushq %rbx
    mov %rsp, 0(%rdi)
    pushq %rdi
    mov %rsp, %rdi
    addq $8, %rdi
    call x86_cpu_set_kernel_stack_top
    popq %rdi
    pushq %rdi
    call x86_user_set_active_context
    popq %rdi
    mov 40(%rdi), %r11
    test %r11, %r11
    jz 1f
    mov 64(%rdi), %rax
    mov 72(%rdi), %rbx
    mov 80(%rdi), %rbp
    mov 88(%rdi), %r12
    mov 96(%rdi), %r13
    mov 104(%rdi), %r14
    mov 112(%rdi), %r15
    mov $0x1b, %dx
    mov %dx, %ds
    mov %dx, %es
    pushq $0x1b
    pushq 48(%rdi)
    pushq 56(%rdi)
    pushq $0x23
    pushq %r11
    iretq
1:
    mov 8(%rdi), %r11
    mov 16(%rdi), %rsi
    mov $0x1b, %ax
    mov %ax, %ds
    mov %ax, %es
    pushq $0x1b
    pushq %rsi
    pushfq
    popq %rax
    orq $0x200, %rax
    pushq %rax
    pushq $0x23
    pushq %r11
    iretq

x86_user_exit_trampoline:
    call x86_user_active_context
    test %rax, %rax
    jz 1f
    movq $1, 24(%rax)
    mov 0(%rax), %rsp
    popq %rbx
    popq %rbp
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    xor %rax, %rax
    ret
1:
    mov x86_user_kernel_rsp(%rip), %rsp
    xor %rax, %rax
    ret

.global x86_context_switch
.type x86_context_switch, @function
x86_context_switch:
    movq %rsp, 0(%rdi)
    movq %r15, 8(%rdi)
    movq %r14, 16(%rdi)
    movq %r13, 24(%rdi)
    movq %r12, 32(%rdi)
    movq %rbx, 40(%rdi)
    movq %rbp, 48(%rdi)
    movq 8(%rsi), %r15
    movq 16(%rsi), %r14
    movq 24(%rsi), %r13
    movq 32(%rsi), %r12
    movq 40(%rsi), %rbx
    movq 48(%rsi), %rbp
    movq 0(%rsi), %rsp
    ret

.section .bss
.align 8
.global x86_user_kernel_rsp
x86_user_kernel_rsp:
    .quad 0
