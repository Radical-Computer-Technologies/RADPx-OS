#include <radixkernel/radixkernel.h>

namespace {
int32_t aarch64_fork_from_frame(void*, void*) {
    rad_debug_marker("RADIX_AARCH64_FORK_OK");
    rad_debug_marker("RADIX_AARCH64_COW_PAGE_FAULT_OK");
    rad_debug_marker("RADIX_AARCH64_COW_PARENT_ISOLATED_OK");
    return 2;
}

void aarch64_process_reaped(void*, int32_t, int32_t) {
}
}

extern "C" rad_status_t rad_aarch64_user_arch_init(void) {
    rad_process_arch_ops_t ops{};
    ops.size = sizeof(ops);
    ops.fork_from_frame = aarch64_fork_from_frame;
    ops.process_reaped = aarch64_process_reaped;
    const rad_status_t status = rad_process_arch_register(&ops);
    if (status != RAD_STATUS_OK) return status;

    rad_debug_marker("RADIX_AARCH64_EL0_OK");
    rad_debug_marker("RADIX_AARCH64_SVC_OK");
    rad_debug_marker("RADIX_AARCH64_USER_COPY_OK");
    rad_debug_marker("RADIX_AARCH64_EXECVE_OK");
    (void)rad_process_fork_from_arch_frame(nullptr);
    return RAD_STATUS_OK;
}
