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
    ldr x1, =rad_pi_boot_argument
    str x19, [x1]
    ldr x30, =__stack_top
    mov sp, x30
    mov x0, x19
    bl rad_pi_entry

5:
    wfe
    b 5b

.align 11
.global rad_a53_exception_vectors
.type rad_a53_exception_vectors, %function
rad_a53_exception_vectors:
    b rad_a53_sync_common      // 0x000 curEL SP0 sync
    .align 7
    b rad_a53_irq_spin         // 0x080 curEL SP0 irq
    .align 7
    b rad_a53_irq_spin         // 0x100 curEL SP0 fiq
    .align 7
    b rad_a53_irq_spin         // 0x180 curEL SP0 serror
    .align 7
    b rad_a53_sync_common      // 0x200 curEL SPx sync
    .align 7
    b rad_a53_irq_common       // 0x280 curEL SPx irq (kernel/task interrupted)
    .align 7
    b rad_a53_irq_spin         // 0x300 curEL SPx fiq
    .align 7
    b rad_a53_irq_spin         // 0x380 curEL SPx serror
    .align 7
    b rad_a53_sync_common      // 0x400 lowerEL AArch64 sync
    .align 7
    b rad_a53_irq_common       // 0x480 lowerEL AArch64 irq (EL0 interrupted)
    .align 7
    b rad_a53_irq_spin         // 0x500 lowerEL AArch64 fiq
    .align 7
    b rad_a53_irq_spin         // 0x580 lowerEL AArch64 serror
    .align 7
    b rad_a53_sync_common      // 0x600 lowerEL AArch32 sync
    .align 7
    b rad_a53_irq_spin
    .align 7
    b rad_a53_irq_spin
    .align 7
    b rad_a53_irq_spin
    .align 7

.global rad_a53_install_exception_vectors
.type rad_a53_install_exception_vectors, %function
rad_a53_install_exception_vectors:
    ldr x0, =rad_a53_exception_vectors
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

.global rad_a53_enter_user_context
.type rad_a53_enter_user_context, %function
rad_a53_enter_user_context:
    mov x10, sp
    str x10, [x0, #0]
    // Save the kernel callee-saved state into A53UserContext.ksave (offset 832)
    // so the syscall exit/exec path in sync_common can unwind by simply returning
    // from this call (restoring x19..x30 + sp). Contract shared with the ZuBoard.
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
    mrs x11, hcr_el2
    orr x11, x11, #(1 << 31)
    msr hcr_el2, x11
    ldr x11, =rad_a53_enter_user_context_el1
    msr elr_el2, x11
    mov x11, #0x3c5
    msr spsr_el2, x11
    eret

rad_a53_enter_user_context_el1:
    ldr x11, =rad_a53_exception_vectors
    msr vbar_el1, x11
    isb
    add x30, x0, #16
    ldr x10, [x30, #248]
    msr sp_el0, x10
    ldr x10, [x30, #256]
    msr elr_el1, x10
    ldr x10, [x30, #264]
    msr spsr_el1, x10
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

rad_a53_restore_user:
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
    add sp, sp, #288
    eret

rad_a53_sync_common:
    sub sp, sp, #288
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
    mov x0, sp
    bl rad_a53_exception_dispatch
    // Contract: 0 = resume the interrupted user context from this stack frame;
    // otherwise x0 = A53UserContext* -> unwind to the kernel caller of
    // rad_a53_enter_user_context (restore its callee-saved regs + stack, return
    // the exit_reason). This drives exit/execve/fault back into user_process_task.
    cbz x0, rad_a53_restore_user
    ldr x9, [x0, #0]          // kernel_sp
    add x10, x0, #832         // ksave (x19..x30)
    ldp x19, x20, [x10, #0]
    ldp x21, x22, [x10, #16]
    ldp x23, x24, [x10, #32]
    ldp x25, x26, [x10, #48]
    ldp x27, x28, [x10, #64]
    ldp x29, x30, [x10, #80]
    mov sp, x9
    ldr x0, [x0, #8]          // exit_reason -> return value of enter_user_context
    ret

// IRQ from EL0 (0x480) or a preempted kernel/task at SPx (0x280). Saves the same
// 288-byte frame as sync_common (GP + sp_el0 + elr/spsr for the current EL), then
// hands the frame to the bcm283x QA7 dispatch, which ticks the timer and -- if the
// tick interrupted EL0 -- yields to the scheduler. Resume through the shared
// restore path (may eret into a different task after a preemptive switch).
rad_a53_irq_common:
    sub sp, sp, #288
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
    b.ne 50f
    mrs x0, elr_el2
    str x0, [sp, #256]
    mrs x0, spsr_el2
    str x0, [sp, #264]
    b 51f
50:
    mrs x0, elr_el1
    str x0, [sp, #256]
    mrs x0, spsr_el1
    str x0, [sp, #264]
51:
    mov x0, sp
    bl rad_bcm283x_irq_dispatch_frame
    b rad_a53_restore_user

rad_a53_irq_spin:
    wfe
    b rad_a53_irq_spin

.section .bss.boot, "aw", %nobits
.align 3
.global rad_pi_boot_argument
rad_pi_boot_argument:
    .skip 8

.section .bss.stack, "aw", %nobits
.align 12
.global __stack_bottom
__stack_bottom:
    .skip 65536
.global __stack_top
__stack_top:
