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
    // CPUECTLR_EL1.SMPEN: join the coherency domain before caches matter.
    // QEMU treats the register as constant; real A53 requires this.
    mrs x5, S3_1_C15_C2_1
    orr x5, x5, #(1 << 6)
    msr S3_1_C15_C2_1, x5
    isb
    mov x19, x0
    ldr x1, =__bss_start
    ldr x2, =__bss_end
3:
    cmp x1, x2
    b.hs 4f
    str xzr, [x1], #8
    b 3b

4:
    ldr x1, =radix_zuboard_boot_argument
    str x19, [x1]
    ldr x30, =__stack_top
    mov sp, x30
    mov x0, x19
    bl radix_zuboard_entry

5:
    wfe
    b 5b

.align 11
.global radix_a53_exception_vectors
.type radix_a53_exception_vectors, %function
radix_a53_exception_vectors:
    b radix_a53_sync_common      // 0x000 curEL SP0 sync
    .align 7
    b radix_a53_irq_spin         // 0x080 curEL SP0 irq (unused: kernel runs SPx)
    .align 7
    b radix_a53_irq_spin         // 0x100 curEL SP0 fiq
    .align 7
    b radix_a53_irq_spin         // 0x180 curEL SP0 serror
    .align 7
    b radix_a53_sync_common      // 0x200 curEL SPx sync
    .align 7
    b radix_a53_irq_common       // 0x280 curEL SPx irq (kernel/task interrupted)
    .align 7
    b radix_a53_irq_spin         // 0x300 curEL SPx fiq
    .align 7
    b radix_a53_irq_spin         // 0x380 curEL SPx serror
    .align 7
    b radix_a53_sync_common      // 0x400 lowerEL AArch64 sync
    .align 7
    b radix_a53_irq_common       // 0x480 lowerEL AArch64 irq (EL0 interrupted)
    .align 7
    b radix_a53_irq_spin
    .align 7
    b radix_a53_irq_spin
    .align 7
    b radix_a53_sync_common
    .align 7
    b radix_a53_irq_spin
    .align 7
    b radix_a53_irq_spin
    .align 7
    b radix_a53_irq_spin
    .align 7

.global radix_a53_install_exception_vectors
.type radix_a53_install_exception_vectors, %function
radix_a53_install_exception_vectors:
    ldr x0, =radix_a53_exception_vectors
    mrs x1, CurrentEL
    lsr x1, x1, #2
    cmp x1, #2
    b.ne 10f
    msr vbar_el2, x0
    msr vbar_el1, x0
    isb
    ret
10:
    msr vbar_el1, x0
    isb
    ret

// A53UserContext layout (static_asserts in radixkernel_a53.cpp):
//   +0 kernel_sp, +8 exit_reason, +16 trap frame (816 bytes: GPR block 0-287,
//   q0-q31 at 288, fpsr 800, fpcr 808), +832 ksave[12]
.global rad_a53_enter_user_context
.type rad_a53_enter_user_context, %function
rad_a53_enter_user_context:
    mov x10, sp
    str x10, [x0, #0]
    add x9, x0, #832
    stp x19, x20, [x9, #0]
    stp x21, x22, [x9, #16]
    stp x23, x24, [x9, #32]
    stp x25, x26, [x9, #48]
    stp x27, x28, [x9, #64]
    stp x29, x30, [x9, #80]
    mrs x12, CurrentEL
    lsr x12, x12, #2
    cmp x12, #2
    b.ne rad_a53_enter_user_context_el1
    msr sp_el1, x10
    mov x11, #(1 << 31)
    msr hcr_el2, x11
    msr cptr_el2, xzr
    msr cntvoff_el2, xzr
    ldr x11, =rad_a53_enter_user_context_el1
    msr elr_el2, x11
    mov x11, #0x3c5
    msr spsr_el2, x11
    eret

rad_a53_enter_user_context_el1:
    ldr x11, =radix_a53_exception_vectors
    msr vbar_el1, x11
    isb
    add x30, x0, #16
    ldr x10, [x30, #248]
    msr sp_el0, x10
    ldr x10, [x30, #256]
    msr elr_el1, x10
    ldr x10, [x30, #264]
    msr spsr_el1, x10
    // Restore the user FP/SIMD state (q block at +288, fpsr +800, fpcr +808).
    add x10, x30, #288
    ldp q0, q1, [x10, #0]
    ldp q2, q3, [x10, #32]
    ldp q4, q5, [x10, #64]
    ldp q6, q7, [x10, #96]
    ldp q8, q9, [x10, #128]
    ldp q10, q11, [x10, #160]
    ldp q12, q13, [x10, #192]
    ldp q14, q15, [x10, #224]
    ldp q16, q17, [x10, #256]
    ldp q18, q19, [x10, #288]
    ldp q20, q21, [x10, #320]
    ldp q22, q23, [x10, #352]
    ldp q24, q25, [x10, #384]
    ldp q26, q27, [x10, #416]
    ldp q28, q29, [x10, #448]
    ldp q30, q31, [x10, #480]
    ldr x11, [x10, #512]
    msr fpsr, x11
    ldr x11, [x10, #520]
    msr fpcr, x11
    ldp x0, x1, [x30, #0]
    ldp x2, x3, [x30, #16]
    ldp x4, x5, [x30, #32]
    ldp x6, x7, [x30, #48]
    ldp x8, x9, [x30, #64]
    ldp x10, x11, [x30, #80]
    ldp x12, x13, [x30, #96]
    ldp x14, x15, [x30, #112]
    ldp x16, x17, [x30, #128]
    ldp x18, x19, [x30, #144]
    ldp x20, x21, [x30, #160]
    ldp x22, x23, [x30, #176]
    ldp x24, x25, [x30, #192]
    ldp x26, x27, [x30, #208]
    ldp x28, x29, [x30, #224]
    ldr x30, [x30, #240]
    eret

radix_a53_restore_user:
    mrs x17, CurrentEL
    lsr x17, x17, #2
    ldr x16, [sp, #248]
    msr sp_el0, x16
    cmp x17, #2
    b.ne 30f
    ldr x16, [sp, #256]
    msr elr_el2, x16
    ldr x16, [sp, #264]
    msr spsr_el2, x16
    b 31f
30:
    ldr x16, [sp, #256]
    msr elr_el1, x16
    ldr x16, [sp, #264]
    msr spsr_el1, x16
31:
    // Restore user FP/SIMD state saved by the entry paths at frame +288.
    add x16, sp, #288
    ldp q0, q1, [x16, #0]
    ldp q2, q3, [x16, #32]
    ldp q4, q5, [x16, #64]
    ldp q6, q7, [x16, #96]
    ldp q8, q9, [x16, #128]
    ldp q10, q11, [x16, #160]
    ldp q12, q13, [x16, #192]
    ldp q14, q15, [x16, #224]
    ldp q16, q17, [x16, #256]
    ldp q18, q19, [x16, #288]
    ldp q20, q21, [x16, #320]
    ldp q22, q23, [x16, #352]
    ldp q24, q25, [x16, #384]
    ldp q26, q27, [x16, #416]
    ldp q28, q29, [x16, #448]
    ldp q30, q31, [x16, #480]
    ldr x17, [x16, #512]
    msr fpsr, x17
    ldr x17, [x16, #520]
    msr fpcr, x17
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    add sp, sp, #816
    eret

radix_a53_sync_common:
    sub sp, sp, #816
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    mrs x0, sp_el0
    str x0, [sp, #248]
    mrs x1, CurrentEL
    lsr x1, x1, #2
    cmp x1, #2
    b.ne 40f
    mrs x0, elr_el2
    str x0, [sp, #256]
    mrs x0, spsr_el2
    str x0, [sp, #264]
    mrs x0, esr_el2
    str x0, [sp, #272]
    mrs x0, far_el2
    str x0, [sp, #280]
    b 41f
40:
    mrs x0, elr_el1
    str x0, [sp, #256]
    mrs x0, spsr_el1
    str x0, [sp, #264]
    mrs x0, esr_el1
    str x0, [sp, #272]
    mrs x0, far_el1
    str x0, [sp, #280]
41:
    // Save the interrupted context's FP/SIMD state (frame +288). Kernel code
    // is -mgeneral-regs-only, so this is the only save/restore site.
    add x0, sp, #288
    stp q0, q1, [x0, #0]
    stp q2, q3, [x0, #32]
    stp q4, q5, [x0, #64]
    stp q6, q7, [x0, #96]
    stp q8, q9, [x0, #128]
    stp q10, q11, [x0, #160]
    stp q12, q13, [x0, #192]
    stp q14, q15, [x0, #224]
    stp q16, q17, [x0, #256]
    stp q18, q19, [x0, #288]
    stp q20, q21, [x0, #320]
    stp q22, q23, [x0, #352]
    stp q24, q25, [x0, #384]
    stp q26, q27, [x0, #416]
    stp q28, q29, [x0, #448]
    stp q30, q31, [x0, #480]
    mrs x1, fpsr
    str x1, [x0, #512]
    mrs x1, fpcr
    str x1, [x0, #520]
    mov x0, sp
    bl rad_a53_exception_dispatch
    // Contract: 0 = resume the interrupted user context from this stack frame,
    // otherwise x0 = A53UserContext* -> unwind to the kernel caller of
    // rad_a53_enter_user_context (restore its callee-saved regs and stack).
    cbz x0, radix_a53_restore_user
    ldr x9, [x0, #0]
    add x10, x0, #832
    ldp x19, x20, [x10, #0]
    ldp x21, x22, [x10, #16]
    ldp x23, x24, [x10, #32]
    ldp x25, x26, [x10, #48]
    ldp x27, x28, [x10, #64]
    ldp x29, x30, [x10, #80]
    mov sp, x9
    ldr x0, [x0, #8]
    ret

// IRQ entry: same 288-byte frame as the sync path, then GIC dispatch in C.
// Always resumes the interrupted context; preemption happens inside the C
// dispatcher via rad_task_yield -- this frame stays parked on the interrupted
// task's stack and unwinds when the task is re-dispatched (x86 pattern).
radix_a53_irq_common:
    sub sp, sp, #816
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    mrs x0, sp_el0
    str x0, [sp, #248]
    mrs x0, elr_el1
    str x0, [sp, #256]
    mrs x0, spsr_el1
    str x0, [sp, #264]
    str xzr, [sp, #272]
    str xzr, [sp, #280]
    // FP/SIMD state of the interrupted context (frame +288): the IRQ may have
    // landed mid-FP-computation at EL0, and the preemption path can run other
    // user code before this frame resumes.
    add x0, sp, #288
    stp q0, q1, [x0, #0]
    stp q2, q3, [x0, #32]
    stp q4, q5, [x0, #64]
    stp q6, q7, [x0, #96]
    stp q8, q9, [x0, #128]
    stp q10, q11, [x0, #160]
    stp q12, q13, [x0, #192]
    stp q14, q15, [x0, #224]
    stp q16, q17, [x0, #256]
    stp q18, q19, [x0, #288]
    stp q20, q21, [x0, #320]
    stp q22, q23, [x0, #352]
    stp q24, q25, [x0, #384]
    stp q26, q27, [x0, #416]
    stp q28, q29, [x0, #448]
    stp q30, q31, [x0, #480]
    mrs x1, fpsr
    str x1, [x0, #512]
    mrs x1, fpcr
    str x1, [x0, #520]
    mov x0, sp
    bl rad_zynqmp_irq_dispatch_frame
    b radix_a53_restore_user

radix_a53_irq_spin:
    wfe
    b radix_a53_irq_spin

// PSCI CPU_ON entry for secondary cores: EL1, MMU off, x0 = core id.
// Minimal asm bring-up, then C (rad_zynqmp_secondary_main) does the
// park/release handshake, MMU, GIC, timer, and enters the worker loop.
.global radix_a53_secondary_start
.type radix_a53_secondary_start, %function
radix_a53_secondary_start:
    msr daifset, #0xf
    mov x19, x0
    ldr x1, =radix_a53_exception_vectors
    msr vbar_el1, x1
    isb
    mrs x1, S3_1_C15_C2_1
    orr x1, x1, #(1 << 6)
    msr S3_1_C15_C2_1, x1
    isb
    ldr x1, =radix_a53_secondary_stack_top
    mov sp, x1
    mov x0, x19
    bl rad_zynqmp_secondary_main
50:
    wfe
    b 50b

.section .bss.boot, "aw", %nobits
.align 3
.global radix_zuboard_boot_argument
radix_zuboard_boot_argument:
    .skip 8

.section .bss.stack, "aw", %nobits
.align 12
.global __stack_bottom
__stack_bottom:
    .skip 65536
.global __stack_top
__stack_top:

.align 12
radix_a53_secondary_stack_bottom:
    .skip 16384
radix_a53_secondary_stack_top:
