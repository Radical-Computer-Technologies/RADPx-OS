#ifndef RAD_X86_64_GRUB_SLINT_USER_H
#define RAD_X86_64_GRUB_SLINT_USER_H

#include <radixkernel/radixkernel.h>

/** x86_64 user-mode execution context used by the RADix process layer. */
typedef struct x86_user_context {
    /** Kernel stack pointer used while handling this process. */
    uint64_t kernel_rsp;
    /** Initial user instruction pointer. */
    uint64_t entry;
    /** Initial top of the user stack. */
    uint64_t stack_top;
    /** Reason code recorded when returning from user mode. */
    uint64_t exit_reason;
    /** RADix process identifier. */
    int32_t pid;
    /** Reserved for ABI alignment. */
    uint32_t reserved;
    /** Saved user instruction pointer for resume. */
    uint64_t resume_rip;
    /** Saved user stack pointer for resume. */
    uint64_t resume_rsp;
    /** Saved user RFLAGS for resume. */
    uint64_t resume_rflags;
    /** Return value injected into the child after fork. */
    uint64_t fork_rax;
    /** Saved callee-preserved register RBX. */
    uint64_t resume_rbx;
    /** Saved callee-preserved register RBP. */
    uint64_t resume_rbp;
    /** Saved callee-preserved register R12. */
    uint64_t resume_r12;
    /** Saved callee-preserved register R13. */
    uint64_t resume_r13;
    /** Saved callee-preserved register R14. */
    uint64_t resume_r14;
    /** Saved callee-preserved register R15. */
    uint64_t resume_r15;
} x86_user_context_t;

/** Register frame captured at x86_64 syscall/trap entry. */
typedef struct x86_user_trap_frame {
    /** Syscall number or return register. */
    uint64_t rax;
    /** Syscall argument 0. */
    uint64_t rdi;
    /** Syscall argument 1. */
    uint64_t rsi;
    /** Syscall argument 2. */
    uint64_t rdx;
    /** Syscall argument 3. */
    uint64_t r10;
    /** Syscall argument 4. */
    uint64_t r8;
    /** Syscall argument 5. */
    uint64_t r9;
    /** Saved RBX. */
    uint64_t rbx;
    /** Saved RBP. */
    uint64_t rbp;
    /** Saved R12. */
    uint64_t r12;
    /** Saved R13. */
    uint64_t r13;
    /** Saved R14. */
    uint64_t r14;
    /** Saved R15. */
    uint64_t r15;
    /** User instruction pointer at trap entry. */
    uint64_t rip;
    /** User code segment selector. */
    uint64_t cs;
    /** User RFLAGS at trap entry. */
    uint64_t rflags;
    /** User stack pointer at trap entry. */
    uint64_t user_rsp;
    /** User stack segment selector. */
    uint64_t ss;
} x86_user_trap_frame_t;

extern "C" int x86_user_run_init(const char *path);
extern "C" rad_status_t x86_user_spawn_process(const char *path, int32_t parent_pid, int32_t *pid_out, rad_task_t *task_out);
extern "C" rad_status_t x86_user_spawn_process_with_stdio(const char *path, int32_t parent_pid, const char *stdio_path, int32_t *pid_out, rad_task_t *task_out);
extern "C" int x86_user_wait_process(int32_t pid);
extern "C" int x86_user_copy_self_test(void);
extern "C" void x86_enter_user_context(x86_user_context_t *context);
extern "C" void x86_user_set_active_context(x86_user_context_t *context);
extern "C" x86_user_context_t *x86_user_active_context(void);
extern "C" long x86_syscall_dispatch_frame(x86_user_trap_frame_t *frame);
extern "C" long x86_syscall_dispatch(unsigned long number, unsigned long arg0, unsigned long arg1,
                                      unsigned long arg2, unsigned long arg3, unsigned long arg4,
                                      unsigned long arg5);

#endif
