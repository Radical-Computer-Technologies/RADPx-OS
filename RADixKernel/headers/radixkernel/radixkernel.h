#ifndef RADIXKERNEL_H
#define RADIXKERNEL_H

/**
 * @file radixkernel.h
 * @brief Portable RADix kernel service ABI for RADLib and RADix-OS targets.
 *
 * RADixKernel exposes a small POSIX-like service contract for bare-metal
 * and simulator backends. The C ABI is the stable boundary; the C++ wrappers are
 * convenience helpers for RADLib applications and examples.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#if defined(__cplusplus) && !defined(RADLIB_EMBEDDED_NO_CPP_WRAPPERS)
#include <string>
#include <vector>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RADEK_VERSION_MAJOR 0 ///< Public constant or ioctl helper.
#define RADEK_VERSION_MINOR 1 ///< Public constant or ioctl helper.
#define RADEK_VERSION_PATCH 2 ///< Public constant or ioctl helper.

#define RAD_IOCTL_NONE 0u ///< Public constant or ioctl helper.
#define RAD_IOCTL_WRITE 1u ///< Public constant or ioctl helper.
#define RAD_IOCTL_READ 2u ///< Public constant or ioctl helper.
#define RAD_IOCTL_READWRITE 3u ///< Public constant or ioctl helper.
#define RAD_IOCTL(dir, type, nr, size) ((((uint32_t)(dir) & 3u) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | (((uint32_t)(type) & 0xffu) << 8) | ((uint32_t)(nr) & 0xffu)) ///< Public constant or ioctl helper.
#define RAD_IO(type, nr) RAD_IOCTL(RAD_IOCTL_NONE, type, nr, 0u) ///< Public constant or ioctl helper.
#define RAD_IOR(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_READ, type, nr, sizeof(type_name)) ///< Public constant or ioctl helper.
#define RAD_IOW(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_WRITE, type, nr, sizeof(type_name)) ///< Public constant or ioctl helper.
#define RAD_IOWR(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_READWRITE, type, nr, sizeof(type_name)) ///< Public constant or ioctl helper.
#define RAD_IOCTL_DIR(request) (((uint32_t)(request) >> 30) & 3u) ///< Public constant or ioctl helper.
#define RAD_IOCTL_SIZE(request) (((uint32_t)(request) >> 16) & 0x3fffu) ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE(request) (((uint32_t)(request) >> 8) & 0xffu) ///< Public constant or ioctl helper.
#define RAD_IOCTL_NR(request) ((uint32_t)(request) & 0xffu) ///< Public constant or ioctl helper.

#define RAD_IOCTL_TYPE_I2C 'I' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_SPI 'S' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_AUDIO 'A' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_SERIAL 'T' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_FRAMEBUFFER 'F' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_INPUT 'K' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_BLOCK 'B' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_NET 'N' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_COMPOSITOR 'C' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_USB 'U' ///< Public constant or ioctl helper.
#define RAD_IOCTL_TYPE_TTY 'Y' ///< Public constant or ioctl helper.

#define RAD_DEVICE_IOCTL_I2C_TRANSFER RAD_IOWR(RAD_IOCTL_TYPE_I2C, 1u, struct rad_i2c_transfer) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_SPI_TRANSFER RAD_IOWR(RAD_IOCTL_TYPE_SPI, 1u, struct rad_spi_transfer) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_AUDIO_CONFIGURE RAD_IOW(RAD_IOCTL_TYPE_AUDIO, 1u, struct rad_audio_format) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_SERIAL_CONFIGURE RAD_IOW(RAD_IOCTL_TYPE_SERIAL, 1u, struct rad_serial_config) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_FRAMEBUFFER_INFO RAD_IOR(RAD_IOCTL_TYPE_FRAMEBUFFER, 1u, struct rad_framebuffer_info) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_FRAMEBUFFER_FLUSH RAD_IOW(RAD_IOCTL_TYPE_FRAMEBUFFER, 2u, struct rad_framebuffer_rect) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_BLOCK_INFO RAD_IOR(RAD_IOCTL_TYPE_BLOCK, 1u, struct rad_block_info) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_BLOCK_READ RAD_IOWR(RAD_IOCTL_TYPE_BLOCK, 2u, struct rad_block_request) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_BLOCK_WRITE RAD_IOWR(RAD_IOCTL_TYPE_BLOCK, 3u, struct rad_block_request) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_BLOCK_FLUSH RAD_IO(RAD_IOCTL_TYPE_BLOCK, 4u) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_NET_LINK_INFO RAD_IOR(RAD_IOCTL_TYPE_NET, 1u, struct rad_net_link_info) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_NET_SEND RAD_IOW(RAD_IOCTL_TYPE_NET, 2u, struct rad_net_packet) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_NET_POLL RAD_IO(RAD_IOCTL_TYPE_NET, 3u) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_NET_RECV RAD_IOWR(RAD_IOCTL_TYPE_NET, 4u, struct rad_net_packet) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_COMPOSITOR_CREATE_SURFACE RAD_IOWR(RAD_IOCTL_TYPE_COMPOSITOR, 1u, struct rad_compositor_ipc_surface) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_COMPOSITOR_QUEUE_DAMAGE RAD_IOW(RAD_IOCTL_TYPE_COMPOSITOR, 2u, struct rad_compositor_ipc_damage) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_FRAMEBUFFER_PRESENT RAD_IOW(RAD_IOCTL_TYPE_FRAMEBUFFER, 3u, struct rad_framebuffer_present) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_USB_HOST_INFO RAD_IOR(RAD_IOCTL_TYPE_USB, 1u, struct rad_usb_host_info) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_TTY_GET_WINSIZE RAD_IOR(RAD_IOCTL_TYPE_TTY, 1u, struct rad_tty_window_size) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_TTY_SET_WINSIZE RAD_IOW(RAD_IOCTL_TYPE_TTY, 2u, struct rad_tty_window_size) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_TTY_GET_MODE RAD_IOR(RAD_IOCTL_TYPE_TTY, 3u, uint32_t) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_TTY_SET_MODE RAD_IOW(RAD_IOCTL_TYPE_TTY, 4u, uint32_t) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_TTY_GET_TERMIOS RAD_IOR(RAD_IOCTL_TYPE_TTY, 5u, struct rad_tty_termios) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_TTY_SET_TERMIOS RAD_IOW(RAD_IOCTL_TYPE_TTY, 6u, struct rad_tty_termios) ///< Public constant or ioctl helper.
#define RAD_DEVICE_IOCTL_TTY_FLUSH RAD_IOW(RAD_IOCTL_TYPE_TTY, 7u, uint32_t) ///< Public constant or ioctl helper.

#define RAD_BOOT_MAX_ARGS 16u ///< Public constant or ioctl helper.
#define RAD_BOOT_MAX_MEMORY_REGIONS 8u ///< Public constant or ioctl helper.
#define RAD_BOOT_MAX_STRING 64u ///< Public constant or ioctl helper.
#define RAD_BOOT_HANDOFF_MAGIC 0x52414448u ///< Public constant or ioctl helper.
#define RAD_BOOT_HANDOFF_VERSION 1u ///< Public constant or ioctl helper.
#define RAD_BOOT_HANDOFF_FLAG_SECONDARIES_PARKED 0x00000001u ///< Public constant or ioctl helper.
#define RAD_BOOT_HANDOFF_FLAG_MMU_DISABLED 0x00000002u ///< Public constant or ioctl helper.
#define RAD_BOOT_HANDOFF_FLAG_DCACHE_DISABLED 0x00000004u ///< Public constant or ioctl helper.
#define RAD_BOOT_HANDOFF_FLAG_ICACHE_INVALIDATED 0x00000008u ///< Public constant or ioctl helper.
#define RAD_BOOT_HANDOFF_FLAG_TLB_INVALIDATED 0x00000010u ///< Public constant or ioctl helper.
#define RAD_BOOT_HANDOFF_FLAG_INTERRUPTS_MASKED 0x00000020u ///< Public constant or ioctl helper.

#define RAD_OVERLAY_MAGIC 0x4f444152u ///< Public constant or ioctl helper.
#define RAD_OVERLAY_VERSION 1u ///< Public constant or ioctl helper.
#define RAD_TREE_MAX_PROPERTY_NAME 64u ///< Public constant or ioctl helper.
#define RAD_TREE_MAX_PATH 96u ///< Public constant or ioctl helper.
#define RAD_TREE_MAX_VALUE 96u ///< Public constant or ioctl helper.
#define RAD_I2C_BUS_NAME_MAX 32u ///< Public constant or ioctl helper.
#define RAD_SPI_BUS_NAME_MAX 32u ///< Public constant or ioctl helper.
#define RAD_DRIVER_NAME_MAX 32u ///< Public constant or ioctl helper.
#define RAD_COMPATIBLE_MAX 64u ///< Public constant or ioctl helper.
#define RAD_MODULE_NAME_MAX 32u ///< Public constant or ioctl helper.
#define RAD_MODULE_MAX_INFO 32u ///< Public constant or ioctl helper.
#define RAD_SERVICE_NAME_MAX 32u ///< Public constant or ioctl helper.
#define RAD_SERVICE_COMPATIBLE_MAX 64u ///< Public constant or ioctl helper.
#define RAD_SERVICE_CAPABILITY_MAX 32u ///< Public constant or ioctl helper.
#define RAD_IRQ_NAME_MAX 32u ///< Public constant or ioctl helper.
#define RAD_IRQ_DOMAIN_NAME_MAX 32u ///< Public constant or ioctl helper.
#define RAD_IRQ_MAX_RESOURCES 4u ///< Public constant or ioctl helper.
#define RAD_FRAMEBUFFER_NAME_MAX 64u ///< Public constant or ioctl helper.
#define RAD_DISPLAY_CONNECTOR_MAX 32u ///< Public constant or ioctl helper.
#define RAD_FRAMEBUFFER_MAX_MODES 8u ///< Public constant or ioctl helper.
#define RAD_SHM_NAME_MAX 64u ///< Public constant or ioctl helper.
#define RAD_SHM_MAX_PAGES 16u ///< Public constant or ioctl helper.
#define RAD_MMAP_PROT_READ 1u ///< Public constant or ioctl helper.
#define RAD_MMAP_PROT_WRITE 2u ///< Public constant or ioctl helper.
#define RAD_MMAP_SHARED 1u ///< Public constant or ioctl helper.
#define RAD_MMAP_PRIVATE 2u ///< Public constant or ioctl helper.
#define RAD_COMPOSITOR_DAMAGE_EXPOSED 1u ///< Public constant or ioctl helper.
#define RAD_AF_INET 2u ///< Public constant or ioctl helper.
#define RAD_SOCK_DGRAM 2u ///< Public constant or ioctl helper.
#define RAD_SOCK_STREAM 1u ///< Public constant or ioctl helper.
#define RAD_IPPROTO_UDP 17u ///< Public constant or ioctl helper.
#define RAD_IPPROTO_TCP 6u ///< Public constant or ioctl helper.
#define RAD_PERF_NAME_MAX 32u ///< Public constant or ioctl helper.
#define RAD_TIMER_NAME_MAX 32u ///< Public constant or ioctl helper.
#define RAD_INPUT_QUEUE_NAME_MAX 64u ///< Public constant or ioctl helper.
#define RAD_WAIT_FOREVER 0xffffffffu ///< Public constant or ioctl helper.
#define RAD_LOG_CATEGORY_MAX 32u ///< Public constant or ioctl helper.
#define RAD_LOG_MESSAGE_MAX 192u ///< Public constant or ioctl helper.

#define RAD_BOOT_MEMORY_USABLE 1u ///< Public constant or ioctl helper.
#define RAD_BOOT_MEMORY_RESERVED 2u ///< Public constant or ioctl helper.
#define RAD_BOOT_MEMORY_PSRAM 3u ///< Public constant or ioctl helper.
#define RAD_BOOT_MEMORY_MMIO 4u ///< Public constant or ioctl helper.

/** @brief Public enumeration for rad_status. */
typedef enum rad_status {
    RAD_STATUS_OK = 0, ///< RAD_STATUS_OK.
    RAD_STATUS_ERROR = -1, ///< RAD_STATUS_ERROR.
    RAD_STATUS_INVALID_ARGUMENT = -2, ///< RAD_STATUS_INVALID_ARGUMENT.
    RAD_STATUS_NOT_FOUND = -3, ///< RAD_STATUS_NOT_FOUND.
    RAD_STATUS_NO_MEMORY = -4, ///< RAD_STATUS_NO_MEMORY.
    RAD_STATUS_TIMEOUT = -5, ///< RAD_STATUS_TIMEOUT.
    RAD_STATUS_NOT_SUPPORTED = -6, ///< RAD_STATUS_NOT_SUPPORTED.
    RAD_STATUS_ALREADY_EXISTS = -7, ///< RAD_STATUS_ALREADY_EXISTS.
    RAD_STATUS_NOT_INITIALIZED = -8 ///< RAD_STATUS_NOT_INITIALIZED.
} rad_status_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_log_level. */
typedef enum rad_log_level {
    RAD_LOG_TRACE = 0, ///< RAD_LOG_TRACE.
    RAD_LOG_DEBUG = 1, ///< RAD_LOG_DEBUG.
    RAD_LOG_INFO = 2, ///< RAD_LOG_INFO.
    RAD_LOG_WARNING = 3, ///< RAD_LOG_WARNING.
    RAD_LOG_ERROR = 4, ///< RAD_LOG_ERROR.
    RAD_LOG_CRITICAL = 5 ///< RAD_LOG_CRITICAL.
} rad_log_level_t; ///< Public typedef alias.

/** @brief Kernel-only printk severity. */
typedef enum rad_kernel_print_level {
    RKERN_DBG = 0, ///< Debug-only kernel detail.
    RKERN_STAT = 1, ///< Normal kernel status.
    RKERN_WARN = 2, ///< Recoverable kernel warning.
    RKERN_ERR = 3 ///< Kernel error.
} rad_kernel_print_level_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_posix_syscall. */
typedef enum rad_posix_syscall {
    RAD_SYSCALL_READ = 0, ///< RAD_SYSCALL_READ.
    RAD_SYSCALL_WRITE = 1, ///< RAD_SYSCALL_WRITE.
    RAD_SYSCALL_OPEN = 2, ///< RAD_SYSCALL_OPEN.
    RAD_SYSCALL_CLOSE = 3, ///< RAD_SYSCALL_CLOSE.
    RAD_SYSCALL_IOCTL = 4, ///< RAD_SYSCALL_IOCTL.
    RAD_SYSCALL_LSEEK = 5, ///< RAD_SYSCALL_LSEEK.
    RAD_SYSCALL_STAT = 6, ///< RAD_SYSCALL_STAT.
    RAD_SYSCALL_FSTAT = 7, ///< RAD_SYSCALL_FSTAT.
    RAD_SYSCALL_GETTIMEOFDAY = 8, ///< RAD_SYSCALL_GETTIMEOFDAY.
    RAD_SYSCALL_NANOSLEEP = 9, ///< RAD_SYSCALL_NANOSLEEP.
    RAD_SYSCALL_EXIT = 10, ///< RAD_SYSCALL_EXIT.
    RAD_SYSCALL_FORK = 11, ///< RAD_SYSCALL_FORK.
    RAD_SYSCALL_EXECVE = 12, ///< RAD_SYSCALL_EXECVE.
    RAD_SYSCALL_WAITPID = 13, ///< RAD_SYSCALL_WAITPID.
    RAD_SYSCALL_GETPID = 14, ///< RAD_SYSCALL_GETPID.
    RAD_SYSCALL_GETPPID = 15, ///< RAD_SYSCALL_GETPPID.
    RAD_SYSCALL_DUP = 16, ///< RAD_SYSCALL_DUP.
    RAD_SYSCALL_DUP2 = 17, ///< RAD_SYSCALL_DUP2.
    RAD_SYSCALL_CHDIR = 18, ///< RAD_SYSCALL_CHDIR.
    RAD_SYSCALL_GETCWD = 19, ///< RAD_SYSCALL_GETCWD.
    RAD_SYSCALL_BRK = 20, ///< RAD_SYSCALL_BRK.
    RAD_SYSCALL_PIPE = 21, ///< RAD_SYSCALL_PIPE.
    RAD_SYSCALL_FCNTL = 22, ///< RAD_SYSCALL_FCNTL.
    RAD_SYSCALL_ACCESS = 23, ///< RAD_SYSCALL_ACCESS.
    RAD_SYSCALL_ISATTY = 24, ///< RAD_SYSCALL_ISATTY.
    RAD_SYSCALL_SOCKET = 25, ///< RAD_SYSCALL_SOCKET.
    RAD_SYSCALL_BIND = 26, ///< RAD_SYSCALL_BIND.
    RAD_SYSCALL_CONNECT = 27, ///< RAD_SYSCALL_CONNECT.
    RAD_SYSCALL_LISTEN = 28, ///< RAD_SYSCALL_LISTEN.
    RAD_SYSCALL_ACCEPT = 29, ///< RAD_SYSCALL_ACCEPT.
    RAD_SYSCALL_SENDTO = 30, ///< RAD_SYSCALL_SENDTO.
    RAD_SYSCALL_RECVFROM = 31, ///< RAD_SYSCALL_RECVFROM.
    RAD_SYSCALL_SHUTDOWN = 32, ///< RAD_SYSCALL_SHUTDOWN.
    RAD_SYSCALL_SETSOCKOPT = 33, ///< RAD_SYSCALL_SETSOCKOPT.
    RAD_SYSCALL_GETSOCKOPT = 34, ///< RAD_SYSCALL_GETSOCKOPT.
    RAD_SYSCALL_MMAP = 35, ///< RAD_SYSCALL_MMAP.
    RAD_SYSCALL_MUNMAP = 36, ///< RAD_SYSCALL_MUNMAP.
    RAD_SYSCALL_SHM_OPEN = 37, ///< RAD_SYSCALL_SHM_OPEN.
    RAD_SYSCALL_SHM_UNLINK = 38, ///< RAD_SYSCALL_SHM_UNLINK.
    RAD_SYSCALL_POLL = 39, ///< RAD_SYSCALL_POLL.
    RAD_SYSCALL_GETDENTS = 1000, ///< RAD_SYSCALL_GETDENTS.
    RAD_SYSCALL_REMOVE = 1001, ///< RAD_SYSCALL_REMOVE.
    RAD_SYSCALL_MKDIR = 1002, ///< RAD_SYSCALL_MKDIR.
    RAD_SYSCALL_RMDIR = 1003, ///< RAD_SYSCALL_RMDIR.
    RAD_SYSCALL_RENAME = 1004, ///< RAD_SYSCALL_RENAME.
    RAD_SYSCALL_TRUNCATE = 1005, ///< RAD_SYSCALL_TRUNCATE.
    RAD_SYSCALL_LOG_READ = 1006, ///< RAD_SYSCALL_LOG_READ.
    RAD_SYSCALL_LOG_FLUSH = 1007, ///< RAD_SYSCALL_LOG_FLUSH.
    RAD_SYSCALL_GETUID = 1008, ///< RAD_SYSCALL_GETUID.
    RAD_SYSCALL_GETEUID = 1009, ///< RAD_SYSCALL_GETEUID.
    RAD_SYSCALL_GETGID = 1010, ///< RAD_SYSCALL_GETGID.
    RAD_SYSCALL_GETEGID = 1011, ///< RAD_SYSCALL_GETEGID.
    RAD_SYSCALL_SETUID = 1012, ///< RAD_SYSCALL_SETUID.
    RAD_SYSCALL_SETGID = 1013, ///< RAD_SYSCALL_SETGID.
    RAD_SYSCALL_CHMOD = 1014, ///< RAD_SYSCALL_CHMOD.
    RAD_SYSCALL_LINK = 1015, ///< RAD_SYSCALL_LINK.
    RAD_SYSCALL_SYMLINK = 1016, ///< RAD_SYSCALL_SYMLINK.
    RAD_SYSCALL_READLINK = 1017, ///< RAD_SYSCALL_READLINK.
    RAD_SYSCALL_FSYNC = 1018 ///< RAD_SYSCALL_FSYNC.
} rad_posix_syscall_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_process_state. */
typedef enum rad_process_state {
    RAD_PROCESS_RUNNING = 1, ///< RAD_PROCESS_RUNNING.
    RAD_PROCESS_WAITING = 2, ///< RAD_PROCESS_WAITING.
    RAD_PROCESS_ZOMBIE = 3 ///< RAD_PROCESS_ZOMBIE.
} rad_process_state_t; ///< Public typedef alias.

#define RAD_WAIT_NOHANG 1u ///< Public constant or ioctl helper.
#define RAD_FD_CLOEXEC 1u ///< Public constant or ioctl helper.
#define RAD_FD_NONBLOCK 256u ///< Public constant or ioctl helper.
#define RAD_FCNTL_GETFD 1u ///< Public constant or ioctl helper.
#define RAD_FCNTL_SETFD 2u ///< Public constant or ioctl helper.
#define RAD_FCNTL_GETFL 3u ///< Public constant or ioctl helper.
#define RAD_FCNTL_SETFL 4u ///< Public constant or ioctl helper.

#define RAD_POLLIN 0x0001 ///< Data other than high-priority data may be read.
#define RAD_POLLOUT 0x0004 ///< Normal data may be written.
#define RAD_POLLERR 0x0008 ///< Error condition.
#define RAD_POLLHUP 0x0010 ///< Hung up.
#define RAD_POLLNVAL 0x0020 ///< Invalid file descriptor.

/** @brief Public enumeration for rad_vfs_open_flags. */
typedef enum rad_vfs_open_flags {
    RAD_VFS_READ = 1u << 0, ///< RAD_VFS_READ.
    RAD_VFS_WRITE = 1u << 1, ///< RAD_VFS_WRITE.
    RAD_VFS_CREATE = 1u << 2, ///< RAD_VFS_CREATE.
    RAD_VFS_TRUNCATE = 1u << 3, ///< RAD_VFS_TRUNCATE.
    RAD_VFS_APPEND = 1u << 4, ///< RAD_VFS_APPEND.
    RAD_VFS_DIRECTORY = 1u << 5 ///< RAD_VFS_DIRECTORY.
} rad_vfs_open_flags_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_seek_origin. */
typedef enum rad_seek_origin {
    RAD_SEEK_SET = 0, ///< RAD_SEEK_SET.
    RAD_SEEK_CUR = 1, ///< RAD_SEEK_CUR.
    RAD_SEEK_END = 2 ///< RAD_SEEK_END.
} rad_seek_origin_t; ///< Public typedef alias.

typedef uint32_t rad_uid_t; ///< POSIX-style user identifier.
typedef uint32_t rad_gid_t; ///< POSIX-style group identifier.

/** @brief Public data structure for process credentials. */
typedef struct rad_credentials {
    rad_uid_t uid; ///< Real user ID.
    rad_uid_t euid; ///< Effective user ID.
    rad_gid_t gid; ///< Real group ID.
    rad_gid_t egid; ///< Effective group ID.
} rad_credentials_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_device_type. */
typedef enum rad_device_type {
    RAD_DEVICE_GENERIC = 0, ///< RAD_DEVICE_GENERIC.
    RAD_DEVICE_I2C = 1, ///< RAD_DEVICE_I2C.
    RAD_DEVICE_SPI = 2, ///< RAD_DEVICE_SPI.
    RAD_DEVICE_AUDIO = 3, ///< RAD_DEVICE_AUDIO.
    RAD_DEVICE_SERIAL = 4, ///< RAD_DEVICE_SERIAL.
    RAD_DEVICE_FRAMEBUFFER = 5, ///< RAD_DEVICE_FRAMEBUFFER.
    RAD_DEVICE_TTY = 6, ///< RAD_DEVICE_TTY.
    RAD_DEVICE_PTY = 7, ///< RAD_DEVICE_PTY.
    RAD_DEVICE_INPUT = 8, ///< RAD_DEVICE_INPUT.
    RAD_DEVICE_BLOCK = 9, ///< RAD_DEVICE_BLOCK.
    RAD_DEVICE_NETWORK = 10, ///< RAD_DEVICE_NETWORK.
    RAD_DEVICE_COMPOSITOR = 11, ///< RAD_DEVICE_COMPOSITOR.
    RAD_DEVICE_USB = 12 ///< RAD_DEVICE_USB.
} rad_device_type_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_usb_controller_type. */
typedef enum rad_usb_controller_type {
    RAD_USB_CONTROLLER_UNKNOWN = 0, ///< RAD_USB_CONTROLLER_UNKNOWN.
    RAD_USB_CONTROLLER_DWC_OTG = 1 ///< RAD_USB_CONTROLLER_DWC_OTG.
} rad_usb_controller_type_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_usb_device_class. */
typedef enum rad_usb_device_class {
    RAD_USB_CLASS_UNKNOWN = 0, ///< RAD_USB_CLASS_UNKNOWN.
    RAD_USB_CLASS_HID_KEYBOARD = 1, ///< RAD_USB_CLASS_HID_KEYBOARD.
    RAD_USB_CLASS_HID_MOUSE = 2, ///< RAD_USB_CLASS_HID_MOUSE.
    RAD_USB_CLASS_MASS_STORAGE = 3 ///< RAD_USB_CLASS_MASS_STORAGE.
} rad_usb_device_class_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_input_event_type. */
typedef enum rad_input_event_type {
    RAD_INPUT_EVENT_NONE = 0, ///< RAD_INPUT_EVENT_NONE.
    RAD_INPUT_EVENT_KEY = 1, ///< RAD_INPUT_EVENT_KEY.
    RAD_INPUT_EVENT_POINTER_MOTION = 2, ///< RAD_INPUT_EVENT_POINTER_MOTION.
    RAD_INPUT_EVENT_POINTER_BUTTON = 3, ///< RAD_INPUT_EVENT_POINTER_BUTTON.
    RAD_INPUT_EVENT_POINTER_SCROLL = 4 ///< RAD_INPUT_EVENT_POINTER_SCROLL.
} rad_input_event_type_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_input_key. */
typedef enum rad_input_key {
    RAD_INPUT_KEY_UNKNOWN = 0, ///< RAD_INPUT_KEY_UNKNOWN.
    RAD_INPUT_KEY_ESCAPE = 1, ///< RAD_INPUT_KEY_ESCAPE.
    RAD_INPUT_KEY_ENTER = 2, ///< RAD_INPUT_KEY_ENTER.
    RAD_INPUT_KEY_BACKSPACE = 3, ///< RAD_INPUT_KEY_BACKSPACE.
    RAD_INPUT_KEY_TAB = 4, ///< RAD_INPUT_KEY_TAB.
    RAD_INPUT_KEY_UP = 5, ///< RAD_INPUT_KEY_UP.
    RAD_INPUT_KEY_DOWN = 6, ///< RAD_INPUT_KEY_DOWN.
    RAD_INPUT_KEY_LEFT = 7, ///< RAD_INPUT_KEY_LEFT.
    RAD_INPUT_KEY_RIGHT = 8, ///< RAD_INPUT_KEY_RIGHT.
    RAD_INPUT_KEY_HOME = 9, ///< RAD_INPUT_KEY_HOME.
    RAD_INPUT_KEY_END = 10, ///< RAD_INPUT_KEY_END.
    RAD_INPUT_KEY_PAGE_UP = 11, ///< RAD_INPUT_KEY_PAGE_UP.
    RAD_INPUT_KEY_PAGE_DOWN = 12, ///< RAD_INPUT_KEY_PAGE_DOWN.
    RAD_INPUT_KEY_INSERT = 13, ///< RAD_INPUT_KEY_INSERT.
    RAD_INPUT_KEY_DELETE = 14, ///< RAD_INPUT_KEY_DELETE.
    RAD_INPUT_KEY_LEFT_SHIFT = 15, ///< RAD_INPUT_KEY_LEFT_SHIFT.
    RAD_INPUT_KEY_RIGHT_SHIFT = 16, ///< RAD_INPUT_KEY_RIGHT_SHIFT.
    RAD_INPUT_KEY_LEFT_CTRL = 17, ///< RAD_INPUT_KEY_LEFT_CTRL.
    RAD_INPUT_KEY_RIGHT_CTRL = 18, ///< RAD_INPUT_KEY_RIGHT_CTRL.
    RAD_INPUT_KEY_LEFT_ALT = 19, ///< RAD_INPUT_KEY_LEFT_ALT.
    RAD_INPUT_KEY_RIGHT_ALT = 20, ///< RAD_INPUT_KEY_RIGHT_ALT.
    RAD_INPUT_KEY_CAPS_LOCK = 21 ///< RAD_INPUT_KEY_CAPS_LOCK.
} rad_input_key_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_input_modifier_flags. */
typedef enum rad_input_modifier_flags {
    RAD_INPUT_MOD_SHIFT = 1u << 0, ///< RAD_INPUT_MOD_SHIFT.
    RAD_INPUT_MOD_CTRL = 1u << 1, ///< RAD_INPUT_MOD_CTRL.
    RAD_INPUT_MOD_ALT = 1u << 2, ///< RAD_INPUT_MOD_ALT.
    RAD_INPUT_MOD_META = 1u << 3, ///< RAD_INPUT_MOD_META.
    RAD_INPUT_MOD_CAPS_LOCK = 1u << 4 ///< RAD_INPUT_MOD_CAPS_LOCK.
} rad_input_modifier_flags_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_input_pointer_button_flags. */
typedef enum rad_input_pointer_button_flags {
    RAD_INPUT_POINTER_BUTTON_LEFT = 1u << 0, ///< RAD_INPUT_POINTER_BUTTON_LEFT.
    RAD_INPUT_POINTER_BUTTON_RIGHT = 1u << 1, ///< RAD_INPUT_POINTER_BUTTON_RIGHT.
    RAD_INPUT_POINTER_BUTTON_MIDDLE = 1u << 2 ///< RAD_INPUT_POINTER_BUTTON_MIDDLE.
} rad_input_pointer_button_flags_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_pixel_format. */
typedef enum rad_pixel_format {
    RAD_PIXEL_FORMAT_RGB565 = 1, ///< RAD_PIXEL_FORMAT_RGB565.
    RAD_PIXEL_FORMAT_XRGB8888 = 2 ///< RAD_PIXEL_FORMAT_XRGB8888.
} rad_pixel_format_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_sd_mode. */
typedef enum rad_sd_mode {
    RAD_SD_MODE_AUTO = 0, ///< RAD_SD_MODE_AUTO.
    RAD_SD_MODE_SPI = 1, ///< RAD_SD_MODE_SPI.
    RAD_SD_MODE_PIO_SDIO = 2 ///< RAD_SD_MODE_PIO_SDIO.
} rad_sd_mode_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_spi_transfer_mode. */
typedef enum rad_spi_transfer_mode {
    RAD_SPI_TRANSFER_MODE_AUTO = 0, ///< RAD_SPI_TRANSFER_MODE_AUTO.
    RAD_SPI_TRANSFER_MODE_PIO = 1, ///< RAD_SPI_TRANSFER_MODE_PIO.
    RAD_SPI_TRANSFER_MODE_DMA = 2 ///< RAD_SPI_TRANSFER_MODE_DMA.
} rad_spi_transfer_mode_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_dma_transfer_type. */
typedef enum rad_dma_transfer_type {
    RAD_DMA_MEMORY_TO_MEMORY = 0, ///< RAD_DMA_MEMORY_TO_MEMORY.
    RAD_DMA_MEMORY_TO_DEVICE = 1, ///< RAD_DMA_MEMORY_TO_DEVICE.
    RAD_DMA_DEVICE_TO_MEMORY = 2 ///< RAD_DMA_DEVICE_TO_MEMORY.
} rad_dma_transfer_type_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_dma_request_id. */
typedef enum rad_dma_request_id {
    RAD_DMA_DREQ_NONE = 0, ///< RAD_DMA_DREQ_NONE.
    RAD_DMA_DREQ_SPI0_TX = 1, ///< RAD_DMA_DREQ_SPI0_TX.
    RAD_DMA_DREQ_SPI0_RX = 2, ///< RAD_DMA_DREQ_SPI0_RX.
    RAD_DMA_DREQ_SPI1_TX = 3, ///< RAD_DMA_DREQ_SPI1_TX.
    RAD_DMA_DREQ_SPI1_RX = 4, ///< RAD_DMA_DREQ_SPI1_RX.
    RAD_DMA_DREQ_RP2350_HSTX_TX = 5 ///< RAD_DMA_DREQ_RP2350_HSTX_TX.
} rad_dma_request_id_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_display_output_type. */
typedef enum rad_display_output_type {
    RAD_DISPLAY_OUTPUT_MEMORY = 0, ///< RAD_DISPLAY_OUTPUT_MEMORY.
    RAD_DISPLAY_OUTPUT_CIRCLE = 1, ///< RAD_DISPLAY_OUTPUT_CIRCLE.
    RAD_DISPLAY_OUTPUT_GRUB = 2, ///< RAD_DISPLAY_OUTPUT_GRUB.
    RAD_DISPLAY_OUTPUT_RP2350_HSTX = 3, ///< RAD_DISPLAY_OUTPUT_RP2350_HSTX.
    RAD_DISPLAY_OUTPUT_SPI_PANEL = 4, ///< RAD_DISPLAY_OUTPUT_SPI_PANEL.
    RAD_DISPLAY_OUTPUT_BCM283X_MAILBOX = 5 ///< RAD_DISPLAY_OUTPUT_BCM283X_MAILBOX.
} rad_display_output_type_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_tty_mode_flags. */
typedef enum rad_tty_mode_flags {
    RAD_TTY_MODE_CANONICAL = 1u << 0, ///< RAD_TTY_MODE_CANONICAL.
    RAD_TTY_MODE_ECHO = 1u << 1, ///< RAD_TTY_MODE_ECHO.
    RAD_TTY_MODE_CRLF = 1u << 2 ///< RAD_TTY_MODE_CRLF.
} rad_tty_mode_flags_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_irq_trigger. */
typedef enum rad_irq_trigger {
    RAD_IRQ_TRIGGER_NONE = 0, ///< RAD_IRQ_TRIGGER_NONE.
    RAD_IRQ_TRIGGER_EDGE_RISING = 1u << 0, ///< RAD_IRQ_TRIGGER_EDGE_RISING.
    RAD_IRQ_TRIGGER_EDGE_FALLING = 1u << 1, ///< RAD_IRQ_TRIGGER_EDGE_FALLING.
    RAD_IRQ_TRIGGER_LEVEL_HIGH = 1u << 2, ///< RAD_IRQ_TRIGGER_LEVEL_HIGH.
    RAD_IRQ_TRIGGER_LEVEL_LOW = 1u << 3 ///< RAD_IRQ_TRIGGER_LEVEL_LOW.
} rad_irq_trigger_t; ///< Public typedef alias.

#define RAD_TASK_CORE_ANY (-1) ///< Public constant or ioctl helper.
#define RAD_TASK_CORE_SERVICE 0 ///< Public constant or ioctl helper.

/** @brief Public enumeration for rad_task_state. */
typedef enum rad_task_state {
    RAD_TASK_NEW = 0, ///< RAD_TASK_NEW.
    RAD_TASK_READY = 1, ///< RAD_TASK_READY.
    RAD_TASK_RUNNING = 2, ///< RAD_TASK_RUNNING.
    RAD_TASK_SLEEPING = 3, ///< RAD_TASK_SLEEPING.
    RAD_TASK_BLOCKED = 4, ///< RAD_TASK_BLOCKED.
    RAD_TASK_FINISHED = 5, ///< RAD_TASK_FINISHED.
    RAD_TASK_DETACHED = 6 ///< RAD_TASK_DETACHED.
} rad_task_state_t; ///< Public typedef alias.

/** @brief Public data structure for rad_kernel_config. */
typedef struct rad_kernel_config {
    const char *backend_name; ///< Public structure field.
    const void *boot_info; ///< Public structure field.
} rad_kernel_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_boot_arg. */
typedef struct rad_boot_arg {
    char key[RAD_BOOT_MAX_STRING]; ///< Public structure field.
    char value[RAD_BOOT_MAX_STRING]; ///< Public structure field.
} rad_boot_arg_t; ///< Public typedef alias.

/** @brief Public data structure for rad_boot_memory_region. */
typedef struct rad_boot_memory_region {
    uint64_t base; ///< Public structure field.
    uint64_t size; ///< Public structure field.
    uint32_t type; ///< Public structure field.
    uint32_t flags; ///< Public structure field.
    char name[RAD_BOOT_MAX_STRING]; ///< Public structure field.
} rad_boot_memory_region_t; ///< Public typedef alias.

/** @brief Public data structure for rad_boot_info. */
typedef struct rad_boot_info {
    uint32_t size; ///< Public structure field.
    uint32_t version; ///< Public structure field.
    char backend[RAD_BOOT_MAX_STRING]; ///< Public structure field.
    char board[RAD_BOOT_MAX_STRING]; ///< Public structure field.
    char console_device[RAD_BOOT_MAX_STRING]; ///< Public structure field.
    char sd_mode[RAD_BOOT_MAX_STRING]; ///< Public structure field.
    uintptr_t fdt_pointer; ///< Public structure field.
    uintptr_t initrd_pointer; ///< Public structure field.
    uint32_t boot_flags; ///< Public structure field.
    uint32_t memory_region_count; ///< Public structure field.
    rad_boot_memory_region_t memory_regions[RAD_BOOT_MAX_MEMORY_REGIONS]; ///< Public structure field.
    uint32_t arg_count; ///< Public structure field.
    rad_boot_arg_t args[RAD_BOOT_MAX_ARGS]; ///< Public structure field.
} rad_boot_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_boot_handoff. */
typedef struct rad_boot_handoff {
    uint32_t magic; ///< Public structure field.
    uint32_t version; ///< Public structure field.
    uint32_t size; ///< Public structure field.
    uint32_t flags; ///< Public structure field.
    rad_boot_info_t boot; ///< Public structure field.
    uintptr_t kernel_image_base; ///< Public structure field.
    uintptr_t kernel_image_size; ///< Public structure field.
    uintptr_t kernel_entry; ///< Public structure field.
    uintptr_t stack_base; ///< Public structure field.
    uintptr_t stack_size; ///< Public structure field.
    uintptr_t fdt_pointer; ///< Public structure field.
    uintptr_t initrd_pointer; ///< Public structure field.
    uintptr_t peripheral_base; ///< Public structure field.
    uintptr_t mailbox_base; ///< Public structure field.
    uintptr_t local_interrupt_base; ///< Public structure field.
    uintptr_t arm_memory_base; ///< Public structure field.
    uintptr_t arm_memory_size; ///< Public structure field.
    uint32_t board_id; ///< Public structure field.
    uint32_t entry_el; ///< Public structure field.
    uint32_t core_count; ///< Public structure field.
    uint32_t parked_core_mask; ///< Public structure field.
    char payload_name[RAD_BOOT_MAX_STRING]; ///< Public structure field.
} rad_boot_handoff_t; ///< Public typedef alias.

/** @brief Public data structure for rad_memory_stats. */
typedef struct rad_memory_stats {
    uint64_t allocations; ///< Public structure field.
    uint64_t frees; ///< Public structure field.
    uint64_t bytes_allocated; ///< Public structure field.
    uint64_t bytes_freed; ///< Public structure field.
    uint64_t bytes_live; ///< Public structure field.
    uint64_t peak_bytes_live; ///< Public structure field.
} rad_memory_stats_t; ///< Public typedef alias.

/** @brief Public data structure for rad_perf_counter_info. */
typedef struct rad_perf_counter_info {
    char name[RAD_PERF_NAME_MAX]; ///< Public structure field.
    uint64_t value; ///< Public structure field.
} rad_perf_counter_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_timer_source_config. */
typedef struct rad_timer_source_config {
    uint32_t size; ///< Public structure field.
    const char *name; ///< Public structure field.
    uint32_t frequency_hz; ///< Public structure field.
    int supports_oneshot; ///< Public structure field.
} rad_timer_source_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_timer_source_ops. */
typedef struct rad_timer_source_ops {
    void *context; ///< Public structure field.
    rad_status_t (*start_periodic)(void *context, uint32_t frequency_hz);///< Public callback slot.
    rad_status_t (*schedule_oneshot)(void *context, uint64_t delay_micros);///< Public callback slot.
    rad_status_t (*cancel_oneshot)(void *context);///< Public callback slot.
} rad_timer_source_ops_t; ///< Public typedef alias.

/** @brief Public data structure for rad_timer_source_info. */
typedef struct rad_timer_source_info {
    char name[RAD_TIMER_NAME_MAX]; ///< Public structure field.
    uint32_t frequency_hz; ///< Public structure field.
    int supports_oneshot; ///< Public structure field.
    int active; ///< Public structure field.
    uint64_t ticks; ///< Public structure field.
} rad_timer_source_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_task_info. */
typedef struct rad_task_info {
    uint64_t id; ///< Public structure field.
    char name[64]; ///< Public structure field.
    int running; ///< Public structure field.
    int finished; ///< Public structure field.
    rad_task_state_t state; ///< Public structure field.
    int target_core; ///< Public structure field.
    int current_core; ///< Public structure field.
    int priority; ///< Public structure field.
    size_t stack_size; ///< Public structure field.
    int detached; ///< Public structure field.
} rad_task_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_task_config. */
typedef struct rad_task_config {
    uint32_t size; ///< Public structure field.
    const char *name; ///< Public structure field.
    size_t stack_size; ///< Public structure field.
    int target_core; ///< Public structure field.
    int priority; ///< Public structure field.
    void *user_context; ///< Public structure field.
} rad_task_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_core_info. */
typedef struct rad_core_info {
    uint32_t detected_cores; ///< Public structure field.
    uint32_t worker_cores; ///< Public structure field.
    uint32_t service_core; ///< Public structure field.
    uint32_t worker_running_mask; ///< Public structure field.
} rad_core_info_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_scheduler_mode. */
typedef enum rad_scheduler_mode {
    RAD_SCHEDULER_COOPERATIVE = 0, ///< RAD_SCHEDULER_COOPERATIVE.
    RAD_SCHEDULER_PREEMPTIVE = 1 ///< RAD_SCHEDULER_PREEMPTIVE.
} rad_scheduler_mode_t; ///< Public typedef alias.

/** @brief Public data structure for rad_scheduler_info. */
typedef struct rad_scheduler_info {
    uint32_t detected_cores; ///< Public structure field.
    uint32_t worker_running_mask; ///< Public structure field.
    uint32_t running_threads; ///< Public structure field.
    uint32_t ready_threads; ///< Public structure field.
    uint32_t blocked_threads; ///< Public structure field.
    uint32_t sleeping_threads; ///< Public structure field.
    uint32_t exited_threads; ///< Public structure field.
    uint32_t process_count; ///< Public structure field.
    int preemption_supported; ///< Public structure field.
    rad_scheduler_mode_t mode; ///< Public structure field.
    char arch[32]; ///< Public structure field.
    uint32_t online_core_mask; ///< Public structure field.
    uint32_t current_core; ///< Public structure field.
    int preemption_enabled; ///< Public structure field.
    uint64_t context_switches; ///< Public structure field.
    uint64_t scheduler_ticks; ///< Public structure field.
} rad_scheduler_info_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_tree_property_type. */
typedef enum rad_tree_property_type {
    RAD_TREE_PROP_BOOL = 1, ///< RAD_TREE_PROP_BOOL.
    RAD_TREE_PROP_U32 = 2, ///< RAD_TREE_PROP_U32.
    RAD_TREE_PROP_STRING = 3, ///< RAD_TREE_PROP_STRING.
    RAD_TREE_PROP_U32_ARRAY = 4, ///< RAD_TREE_PROP_U32_ARRAY.
    RAD_TREE_PROP_STRING_ARRAY = 5 ///< RAD_TREE_PROP_STRING_ARRAY.
} rad_tree_property_type_t; ///< Public typedef alias.

/** @brief Opaque handle type rad_tree_node_t backed by rad_tree_node_handle. */
typedef struct rad_tree_node_handle *rad_tree_node_t;

/** @brief Public data structure for rad_tree_node_info. */
typedef struct rad_tree_node_info {
    char path[RAD_TREE_MAX_PATH]; ///< Public structure field.
    char name[RAD_TREE_MAX_PROPERTY_NAME]; ///< Public structure field.
    char parent[RAD_TREE_MAX_PATH]; ///< Public structure field.
    char module[RAD_TREE_MAX_PROPERTY_NAME]; ///< Public structure field.
    int bound; ///< Public structure field.
} rad_tree_node_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_overlay_info. */
typedef struct rad_overlay_info {
    char name[RAD_TREE_MAX_PROPERTY_NAME]; ///< Public structure field.
    char source[RAD_TREE_MAX_PATH]; ///< Public structure field.
    uint32_t node_count; ///< Public structure field.
    uint32_t property_count; ///< Public structure field.
} rad_overlay_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_vfs_stat. */
typedef struct rad_vfs_stat {
    uint64_t size; ///< Public structure field.
    int is_directory; ///< Public structure field.
    uint32_t mode; ///< Public structure field.
    rad_uid_t uid; ///< Owning user ID.
    rad_gid_t gid; ///< Owning group ID.
    uint64_t mtime_millis; ///< Public structure field.
} rad_vfs_stat_t; ///< Public typedef alias.

/** @brief Public data structure for rad_block_info. */
typedef struct rad_block_info {
    uint32_t size; ///< Public structure field.
    uint32_t sector_size; ///< Public structure field.
    uint64_t sector_count; ///< Public structure field.
    uint32_t flags; ///< Public structure field.
    uint32_t reserved; ///< Public structure field.
} rad_block_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_block_request. */
typedef struct rad_block_request {
    uint32_t size; ///< Public structure field.
    uint64_t sector; ///< Public structure field.
    uint32_t sector_count; ///< Public structure field.
    void *buffer; ///< Public structure field.
} rad_block_request_t; ///< Public typedef alias.

/** @brief Public data structure for rad_usb_host_info. */
typedef struct rad_usb_host_info {
    uint32_t size; ///< Public structure field.
    rad_usb_controller_type_t controller; ///< Public structure field.
    uint32_t device_count; ///< Public structure field.
    uint32_t hid_keyboard_count; ///< Public structure field.
    uint32_t hid_mouse_count; ///< Public structure field.
    uint32_t flags; ///< Public structure field.
} rad_usb_host_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_mac_address. */
typedef struct rad_mac_address {
    uint8_t bytes[6]; ///< Public structure field.
} rad_mac_address_t; ///< Public typedef alias.

/** @brief Public data structure for rad_ipv4_address. */
typedef struct rad_ipv4_address {
    uint8_t bytes[4]; ///< Public structure field.
} rad_ipv4_address_t; ///< Public typedef alias.

/** @brief Public data structure for rad_net_packet. */
typedef struct rad_net_packet {
    uint32_t size; ///< Public structure field.
    void *data; ///< Public structure field.
    size_t length; ///< Public structure field.
} rad_net_packet_t; ///< Public typedef alias.

/** @brief Public data structure for rad_net_link_info. */
typedef struct rad_net_link_info {
    uint32_t size; ///< Public structure field.
    rad_mac_address_t mac; ///< Public structure field.
    uint32_t mtu; ///< Public structure field.
    int link_up; ///< Public structure field.
    uint64_t tx_packets; ///< Public structure field.
    uint64_t rx_packets; ///< Public structure field.
} rad_net_link_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_sockaddr_in. */
typedef struct rad_sockaddr_in {
    uint16_t family; ///< Public structure field.
    uint16_t port; ///< Public structure field.
    rad_ipv4_address_t address; ///< Public structure field.
    uint8_t zero[8]; ///< Public structure field.
} rad_sockaddr_in_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_tcp_state. */
typedef enum rad_tcp_state {
    RAD_TCP_CLOSED = 0, ///< RAD_TCP_CLOSED.
    RAD_TCP_LISTEN = 1, ///< RAD_TCP_LISTEN.
    RAD_TCP_SYN_SENT = 2, ///< RAD_TCP_SYN_SENT.
    RAD_TCP_SYN_RECEIVED = 3, ///< RAD_TCP_SYN_RECEIVED.
    RAD_TCP_ESTABLISHED = 4, ///< RAD_TCP_ESTABLISHED.
    RAD_TCP_FIN_WAIT = 5, ///< RAD_TCP_FIN_WAIT.
    RAD_TCP_CLOSE_WAIT = 6, ///< RAD_TCP_CLOSE_WAIT.
    RAD_TCP_LAST_ACK = 7, ///< RAD_TCP_LAST_ACK.
    RAD_TCP_TIME_WAIT = 8 ///< RAD_TCP_TIME_WAIT.
} rad_tcp_state_t; ///< Public typedef alias.

/** @brief Public data structure for rad_socket_info. */
typedef struct rad_socket_info {
    uint32_t size; ///< Public structure field.
    int domain; ///< Public structure field.
    int type; ///< Public structure field.
    int protocol; ///< Public structure field.
    rad_tcp_state_t tcp_state; ///< Public structure field.
    uint16_t local_port; ///< Public structure field.
    uint16_t remote_port; ///< Public structure field.
    rad_ipv4_address_t local_address; ///< Public structure field.
    rad_ipv4_address_t remote_address; ///< Public structure field.
} rad_socket_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_posix_timeval. */
typedef struct rad_posix_timeval {
    int64_t tv_sec; ///< Public structure field.
    int64_t tv_usec; ///< Public structure field.
} rad_posix_timeval_t; ///< Public typedef alias.

/** @brief POSIX-style poll descriptor used by RADix userland. */
typedef struct rad_pollfd {
    int32_t fd; ///< File descriptor.
    int16_t events; ///< Requested RAD_POLL* event mask.
    int16_t revents; ///< Returned RAD_POLL* event mask.
} rad_pollfd_t; ///< Public typedef alias.

/** @brief Public data structure for rad_process_info. */
typedef struct rad_process_info {
    int32_t pid; ///< Public structure field.
    int32_t parent_pid; ///< Public structure field.
    int32_t exited; ///< Public structure field.
    int32_t exit_code; ///< Public structure field.
    rad_process_state_t state; ///< Public structure field.
    rad_credentials_t credentials; ///< Process credentials.
    char path[128]; ///< Public structure field.
} rad_process_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_process_arch_ops. */
typedef struct rad_process_arch_ops {
    uint32_t size; ///< Public structure field.
    void *context; ///< Public structure field.
    int32_t (*fork_from_frame)(void *context, void *trap_frame);///< Public callback slot.
    void (*process_reaped)(void *context, int32_t pid, int32_t status);///< Public callback slot.
} rad_process_arch_ops_t; ///< Public typedef alias.

/** @brief Public data structure for rad_vfs_dirent. */
typedef struct rad_vfs_dirent {
    char name[96]; ///< Public structure field.
    rad_vfs_stat_t stat; ///< Public structure field.
} rad_vfs_dirent_t; ///< Public typedef alias.

/** @brief Public data structure for userspace directory enumeration. */
typedef struct rad_dirent_user {
    uint8_t type; ///< Public structure field: 1=file, 2=directory.
    char name[256]; ///< Public structure field.
} rad_dirent_user_t; ///< Public typedef alias.

/** @brief Public callback typedef used by the RADix API. */
typedef int (*rad_vfs_list_callback_t)(const char *name, const rad_vfs_stat_t *stat, void *context);

/** @brief Public data structure for rad_vfs_backend_ops. */
typedef struct rad_vfs_backend_ops {
    void *context; ///< Public structure field.
    rad_status_t (*open)(void *context, const char *path, uint32_t flags, void **file);///< Public callback slot.
    rad_status_t (*read)(void *file, void *buffer, size_t size, size_t *bytes_read);///< Public callback slot.
    rad_status_t (*write)(void *file, const void *buffer, size_t size, size_t *bytes_written);///< Public callback slot.
    rad_status_t (*seek)(void *file, int64_t offset, rad_seek_origin_t origin);///< Public callback slot.
    uint64_t (*tell)(void *file);///< Public callback slot.
    void (*close)(void *file);///< Public callback slot.
    rad_status_t (*stat)(void *context, const char *path, rad_vfs_stat_t *stat);///< Public callback slot.
    rad_status_t (*list)(void *context, const char *path, rad_vfs_list_callback_t callback, void *callback_context);///< Public callback slot.
    rad_status_t (*mkdir)(void *context, const char *path);///< Public callback slot.
    rad_status_t (*remove)(void *context, const char *path);///< Public callback slot.
    rad_status_t (*rename)(void *context, const char *old_path, const char *new_path);///< Public callback slot.
    rad_status_t (*rmdir)(void *context, const char *path);///< Public callback slot.
    rad_status_t (*fsync)(void *file);///< Public callback slot.
    rad_status_t (*truncate)(void *context, const char *path, uint64_t size);///< Public callback slot.
    rad_status_t (*readlink)(void *context, const char *path, char *buffer, size_t size);///< Public callback slot.
    rad_status_t (*symlink)(void *context, const char *target, const char *link_path);///< Public callback slot.
    rad_status_t (*link)(void *context, const char *old_path, const char *new_path);///< Public callback slot.
    rad_status_t (*chmod)(void *context, const char *path, uint32_t mode);///< Public callback slot.
} rad_vfs_backend_ops_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_program_state. */
typedef enum rad_program_state {
    RAD_PROGRAM_LOADED = 0, ///< RAD_PROGRAM_LOADED.
    RAD_PROGRAM_RUNNING = 1, ///< RAD_PROGRAM_RUNNING.
    RAD_PROGRAM_FINISHED = 2, ///< RAD_PROGRAM_FINISHED.
    RAD_PROGRAM_FAILED = 3 ///< RAD_PROGRAM_FAILED.
} rad_program_state_t; ///< Public typedef alias.

/** @brief Public data structure for rad_program_info. */
typedef struct rad_program_info {
    uint64_t id; ///< Public structure field.
    char path[128]; ///< Public structure field.
    char name[64]; ///< Public structure field.
    rad_program_state_t state; ///< Public structure field.
    uint64_t task_id; ///< Public structure field.
    int exit_code; ///< Public structure field.
} rad_program_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_device_ops. */
typedef struct rad_device_ops {
    void *context; ///< Public structure field.
    rad_status_t (*read)(void *context, void *buffer, size_t size, size_t *bytes_read);///< Public callback slot.
    rad_status_t (*write)(void *context, const void *buffer, size_t size, size_t *bytes_written);///< Public callback slot.
    rad_status_t (*ioctl)(void *context, uint32_t request, void *argument);///< Public callback slot.
} rad_device_ops_t; ///< Public typedef alias.

/** @brief Public data structure for rad_input_event. */
typedef struct rad_input_event {
    uint32_t size; ///< Public structure field.
    rad_input_event_type_t type; ///< Public structure field.
    uint32_t code; ///< Public structure field.
    uint32_t codepoint; ///< Public structure field.
    uint32_t modifiers; ///< Public structure field.
    uint8_t pressed; ///< Public structure field.
    uint8_t repeat; ///< Public structure field.
    uint16_t reserved; ///< Public structure field.
    int32_t x; ///< Public structure field.
    int32_t y; ///< Public structure field.
    int32_t dx; ///< Public structure field.
    int32_t dy; ///< Public structure field.
    int32_t scroll_x; ///< Public structure field.
    int32_t scroll_y; ///< Public structure field.
    uint32_t buttons; ///< Public structure field.
} rad_input_event_t; ///< Public typedef alias.

/** @brief Public data structure for rad_framebuffer_rect. */
typedef struct rad_framebuffer_rect {
    uint32_t x; ///< Public structure field.
    uint32_t y; ///< Public structure field.
    uint32_t width; ///< Public structure field.
    uint32_t height; ///< Public structure field.
} rad_framebuffer_rect_t; ///< Public typedef alias.

/** @brief Public data structure for rad_framebuffer_info. */
typedef struct rad_framebuffer_info {
    uint32_t size; ///< Public structure field.
    uint32_t width; ///< Public structure field.
    uint32_t height; ///< Public structure field.
    uint32_t stride_bytes; ///< Public structure field.
    rad_pixel_format_t pixel_format; ///< Public structure field.
    void *pixels; ///< Public structure field.
} rad_framebuffer_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_framebuffer_present. */
typedef struct rad_framebuffer_present {
    uint32_t size; ///< Public structure field.
    void *pixels; ///< Public structure field.
    uint32_t stride_bytes; ///< Public structure field.
    rad_framebuffer_rect_t rect; ///< Public structure field.
} rad_framebuffer_present_t; ///< Public typedef alias.

/** @brief Public data structure for rad_display_mode. */
typedef struct rad_display_mode {
    uint32_t width; ///< Public structure field.
    uint32_t height; ///< Public structure field.
    uint32_t refresh_hz; ///< Public structure field.
    uint32_t stride_bytes; ///< Public structure field.
    rad_pixel_format_t pixel_format; ///< Public structure field.
} rad_display_mode_t; ///< Public typedef alias.

/** @brief Public data structure for rad_framebuffer_ops. */
typedef struct rad_framebuffer_ops {
    void *context; ///< Public structure field.
    rad_status_t (*flush)(void *context, const rad_framebuffer_rect_t *rect);///< Public callback slot.
    rad_status_t (*present)(void *context, const rad_framebuffer_present_t *present);///< Public callback slot.
    rad_status_t (*set_mode)(void *context, const rad_display_mode_t *mode);///< Public callback slot.
    rad_status_t (*blank)(void *context, int blanked);///< Public callback slot.
    uint64_t (*get_vsync_counter)(void *context);///< Public callback slot.
} rad_framebuffer_ops_t; ///< Public typedef alias.

/** @brief Opaque handle type rad_framebuffer_t backed by rad_framebuffer_handle. */
typedef struct rad_framebuffer_handle *rad_framebuffer_t;

/** @brief Public data structure for rad_framebuffer_config. */
typedef struct rad_framebuffer_config {
    uint32_t size; ///< Public structure field.
    const char *name; ///< Public structure field.
    rad_framebuffer_info_t info; ///< Public structure field.
    rad_display_output_type_t output_type; ///< Public structure field.
    const char *connector; ///< Public structure field.
    uint32_t mode_count; ///< Public structure field.
    uint32_t preferred_mode; ///< Public structure field.
    rad_display_mode_t modes[RAD_FRAMEBUFFER_MAX_MODES]; ///< Public structure field.
    int primary; ///< Public structure field.
} rad_framebuffer_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_framebuffer_display_info. */
typedef struct rad_framebuffer_display_info {
    uint32_t size; ///< Public structure field.
    char name[RAD_FRAMEBUFFER_NAME_MAX]; ///< Public structure field.
    rad_framebuffer_info_t framebuffer; ///< Public structure field.
    rad_display_output_type_t output_type; ///< Public structure field.
    char connector[RAD_DISPLAY_CONNECTOR_MAX]; ///< Public structure field.
    uint32_t mode_count; ///< Public structure field.
    uint32_t preferred_mode; ///< Public structure field.
    uint32_t current_mode; ///< Public structure field.
    rad_display_mode_t modes[RAD_FRAMEBUFFER_MAX_MODES]; ///< Public structure field.
    int primary; ///< Public structure field.
} rad_framebuffer_display_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_shm_info. */
typedef struct rad_shm_info {
    uint32_t size; ///< Public structure field.
    char name[RAD_SHM_NAME_MAX]; ///< Public structure field.
    size_t byte_size; ///< Public structure field.
    size_t page_count; ///< Public structure field.
} rad_shm_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_compositor_ipc_surface. */
typedef struct rad_compositor_ipc_surface {
    uint32_t size; ///< Public structure field.
    int32_t shm_fd; ///< Public structure field.
    uint32_t width; ///< Public structure field.
    uint32_t height; ///< Public structure field.
    uint32_t stride_pixels; ///< Public structure field.
    int32_t x; ///< Public structure field.
    int32_t y; ///< Public structure field.
    int32_t z; ///< Public structure field.
    uint32_t flags; ///< Public structure field.
    uint32_t surface_id; ///< Public structure field.
} rad_compositor_ipc_surface_t; ///< Public typedef alias.

/** @brief Public data structure for rad_compositor_ipc_damage. */
typedef struct rad_compositor_ipc_damage {
    uint32_t size; ///< Public structure field.
    uint32_t surface_id; ///< Public structure field.
    int32_t x; ///< Public structure field.
    int32_t y; ///< Public structure field.
    int32_t width; ///< Public structure field.
    int32_t height; ///< Public structure field.
    uint32_t flags; ///< Public structure field.
} rad_compositor_ipc_damage_t; ///< Public typedef alias.

/** @brief Public data structure for rad_i2c_transfer. */
typedef struct rad_i2c_transfer {
    uint8_t address; ///< Public structure field.
    const uint8_t *write_data; ///< Public structure field.
    size_t write_size; ///< Public structure field.
    uint8_t *read_data; ///< Public structure field.
    size_t read_size; ///< Public structure field.
} rad_i2c_transfer_t; ///< Public typedef alias.

/** @brief Opaque handle type rad_i2c_bus_t backed by rad_i2c_bus_handle. */
typedef struct rad_i2c_bus_handle *rad_i2c_bus_t;
/** @brief Forward-declared public type rad_i2c_device_t backed by rad_i2c_device. */
typedef struct rad_i2c_device rad_i2c_device_t;

/** @brief Public data structure for rad_i2c_controller_ops. */
typedef struct rad_i2c_controller_ops {
    void *context; ///< Public structure field.
    rad_status_t (*transfer)(void *context, const rad_i2c_transfer_t *transfer);///< Public callback slot.
} rad_i2c_controller_ops_t; ///< Public typedef alias.

/** @brief Public data structure for rad_i2c_controller_config. */
typedef struct rad_i2c_controller_config {
    uint32_t size; ///< Public structure field.
    uint32_t bus_id; ///< Public structure field.
    const char *name; ///< Public structure field.
    const char *tree_path; ///< Public structure field.
    uint32_t clock_hz; ///< Public structure field.
    uint8_t sda_gpio; ///< Public structure field.
    uint8_t scl_gpio; ///< Public structure field.
} rad_i2c_controller_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_i2c_driver. */
typedef struct rad_i2c_driver {
    uint32_t size; ///< Public structure field.
    const char *name; ///< Public structure field.
    const char *compatible; ///< Public structure field.
    void *context; ///< Public structure field.
    rad_status_t (*probe)(void *context, rad_i2c_device_t *device);///< Public callback slot.
    void (*remove)(void *context, rad_i2c_device_t *device);///< Public callback slot.
} rad_i2c_driver_t; ///< Public typedef alias.

/** @brief Public data structure for rad_spi_transfer. */
typedef struct rad_spi_transfer {
    const uint8_t *tx_data; ///< Public structure field.
    uint8_t *rx_data; ///< Public structure field.
    size_t size; ///< Public structure field.
    uint32_t speed_hz; ///< Public structure field.
    uint8_t mode; ///< Public structure field.
    uint8_t bits_per_word; ///< Public structure field.
    uint8_t cs; ///< Public structure field.
    uint8_t transfer_mode; ///< Public structure field.
} rad_spi_transfer_t; ///< Public typedef alias.

/** @brief Opaque handle type rad_spi_bus_t backed by rad_spi_bus_handle. */
typedef struct rad_spi_bus_handle *rad_spi_bus_t;
/** @brief Forward-declared public type rad_spi_device_t backed by rad_spi_device. */
typedef struct rad_spi_device rad_spi_device_t;

/** @brief Opaque handle type rad_dma_channel_t backed by rad_dma_channel_handle. */
typedef struct rad_dma_channel_handle *rad_dma_channel_t;

/** @brief Public data structure for rad_dma_transfer. */
typedef struct rad_dma_transfer {
    uint32_t size; ///< Public structure field.
    rad_dma_transfer_type_t type; ///< Public structure field.
    rad_dma_request_id_t request_id; ///< Public structure field.
    const void *source; ///< Public structure field.
    void *destination; ///< Public structure field.
    uintptr_t peripheral_address; ///< Public structure field.
    uint32_t flags; ///< Public structure field.
} rad_dma_transfer_t; ///< Public typedef alias.

/** @brief Public data structure for rad_dma_backend_ops. */
typedef struct rad_dma_backend_ops {
    void *context; ///< Public structure field.
    rad_status_t (*request)(void *context, rad_dma_request_id_t request_id, void **backend_channel);///< Public callback slot.
    void (*release)(void *context, void *backend_channel);///< Public callback slot.
    rad_status_t (*submit)(void *context, void *backend_channel, const rad_dma_transfer_t *transfer);///< Public callback slot.
    rad_status_t (*wait)(void *context, void *backend_channel, uint32_t timeout_ms);///< Public callback slot.
    rad_status_t (*cancel)(void *context, void *backend_channel);///< Public callback slot.
} rad_dma_backend_ops_t; ///< Public typedef alias.

/** @brief Public data structure for rad_dma_controller_config. */
typedef struct rad_dma_controller_config {
    uint32_t size; ///< Public structure field.
    const char *name; ///< Public structure field.
    uint32_t channel_count; ///< Public structure field.
} rad_dma_controller_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_spi_controller_ops. */
typedef struct rad_spi_controller_ops {
    void *context; ///< Public structure field.
    rad_status_t (*transfer)(void *context, const rad_spi_device_t *device, const rad_spi_transfer_t *transfer);///< Public callback slot.
    rad_status_t (*transfer_dma)(void *context, const rad_spi_device_t *device, const rad_spi_transfer_t *transfer, rad_dma_channel_t tx_channel, rad_dma_channel_t rx_channel);///< Public callback slot.
} rad_spi_controller_ops_t; ///< Public typedef alias.

/** @brief Public data structure for rad_spi_controller_config. */
typedef struct rad_spi_controller_config {
    uint32_t size; ///< Public structure field.
    uint32_t bus_id; ///< Public structure field.
    const char *name; ///< Public structure field.
    const char *tree_path; ///< Public structure field.
    uint32_t clock_hz; ///< Public structure field.
    uint8_t sck_gpio; ///< Public structure field.
    uint8_t tx_gpio; ///< Public structure field.
    uint8_t rx_gpio; ///< Public structure field.
    uint8_t cs_gpio; ///< Public structure field.
} rad_spi_controller_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_spi_driver. */
typedef struct rad_spi_driver {
    uint32_t size; ///< Public structure field.
    const char *name; ///< Public structure field.
    const char *compatible; ///< Public structure field.
    void *context; ///< Public structure field.
    rad_status_t (*probe)(void *context, rad_spi_device_t *device);///< Public callback slot.
    void (*remove)(void *context, rad_spi_device_t *device);///< Public callback slot.
} rad_spi_driver_t; ///< Public typedef alias.

/** @brief Public data structure for rad_module_descriptor. */
typedef struct rad_module_descriptor {
    uint32_t size; ///< Public structure field.
    const char *name; ///< Public structure field.
    const char *bus; ///< Public structure field.
    const char *compatible; ///< Public structure field.
    rad_status_t (*init)(void *context);///< Public callback slot.
    void (*exit)(void *context);///< Public callback slot.
    void *context; ///< Public structure field.
} rad_module_descriptor_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_module_state. */
typedef enum rad_module_state {
    RAD_MODULE_REGISTERED = 1, ///< RAD_MODULE_REGISTERED.
    RAD_MODULE_INITIALIZED = 2, ///< RAD_MODULE_INITIALIZED.
    RAD_MODULE_FAILED = 3, ///< RAD_MODULE_FAILED.
    RAD_MODULE_EXITED = 4 ///< RAD_MODULE_EXITED.
} rad_module_state_t; ///< Public typedef alias.

/** @brief Public enumeration for rad_service_state. */
typedef enum rad_service_state {
    RAD_SERVICE_REGISTERED = 1, ///< RAD_SERVICE_REGISTERED.
    RAD_SERVICE_CONFIGURED = 2, ///< RAD_SERVICE_CONFIGURED.
    RAD_SERVICE_RUNNING = 3, ///< RAD_SERVICE_RUNNING.
    RAD_SERVICE_FAILED = 4, ///< RAD_SERVICE_FAILED.
    RAD_SERVICE_STOPPED = 5 ///< RAD_SERVICE_STOPPED.
} rad_service_state_t; ///< Public typedef alias.

/** @brief Public data structure for rad_module_info. */
typedef struct rad_module_info {
    char name[RAD_MODULE_NAME_MAX]; ///< Public structure field.
    char bus[RAD_DRIVER_NAME_MAX]; ///< Public structure field.
    char compatible[RAD_COMPATIBLE_MAX]; ///< Public structure field.
    rad_module_state_t state; ///< Public structure field.
    int32_t last_status; ///< Public structure field.
} rad_module_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_service_config. */
typedef struct rad_service_config {
    uint32_t size; ///< Public structure field.
    const char *tree_path; ///< Public structure field.
    const char *backend; ///< Public structure field.
    const char *display; ///< Public structure field.
    const char *keyboard; ///< Public structure field.
    const char *pointer; ///< Public structure field.
    const char *terminal; ///< Public structure field.
    int autostart; ///< Public structure field.
    int order; ///< Public structure field.
} rad_service_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_service_descriptor. */
typedef struct rad_service_descriptor {
    uint32_t size; ///< Public structure field.
    const char *name; ///< Public structure field.
    const char *compatible; ///< Public structure field.
    const char *capability; ///< Public structure field.
    int default_order; ///< Public structure field.
    rad_status_t (*start)(void *context, const rad_service_config_t *config);///< Public callback slot.
    void (*stop)(void *context);///< Public callback slot.
    rad_status_t (*poll)(void *context);///< Public callback slot.
    void *context; ///< Public structure field.
} rad_service_descriptor_t; ///< Public typedef alias.

/** @brief Public data structure for rad_service_info. */
typedef struct rad_service_info {
    char name[RAD_SERVICE_NAME_MAX]; ///< Public structure field.
    char compatible[RAD_SERVICE_COMPATIBLE_MAX]; ///< Public structure field.
    char capability[RAD_SERVICE_CAPABILITY_MAX]; ///< Public structure field.
    char tree_path[RAD_TREE_MAX_PATH]; ///< Public structure field.
    char backend[RAD_TREE_MAX_VALUE]; ///< Public structure field.
    char display[RAD_TREE_MAX_VALUE]; ///< Public structure field.
    char keyboard[RAD_TREE_MAX_VALUE]; ///< Public structure field.
    char pointer[RAD_TREE_MAX_VALUE]; ///< Public structure field.
    char terminal[RAD_TREE_MAX_VALUE]; ///< Public structure field.
    rad_service_state_t state; ///< Public structure field.
    int autostart; ///< Public structure field.
    int order; ///< Public structure field.
    int32_t last_status; ///< Public structure field.
} rad_service_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_driver_info. */
typedef struct rad_driver_info {
    char name[RAD_DRIVER_NAME_MAX]; ///< Public structure field.
    char bus[RAD_DRIVER_NAME_MAX]; ///< Public structure field.
    char compatible[RAD_COMPATIBLE_MAX]; ///< Public structure field.
    char role[RAD_DRIVER_NAME_MAX]; ///< Public structure field.
} rad_driver_info_t; ///< Public typedef alias.

/** @brief Opaque handle type rad_irq_domain_t backed by rad_irq_domain_handle. */
typedef struct rad_irq_domain_handle *rad_irq_domain_t;

/** @brief Public data structure for rad_irq_domain_config. */
typedef struct rad_irq_domain_config {
    uint32_t size; ///< Public structure field.
    const char *name; ///< Public structure field.
    const char *tree_path; ///< Public structure field.
    uint32_t interrupt_base; ///< Public structure field.
    uint32_t line_count; ///< Public structure field.
    uint32_t interrupt_cells; ///< Public structure field.
} rad_irq_domain_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_irq_resource. */
typedef struct rad_irq_resource {
    uint32_t irq; ///< Public structure field.
    uint32_t line; ///< Public structure field.
    uint32_t flags; ///< Public structure field.
    rad_irq_trigger_t trigger; ///< Public structure field.
    char domain[RAD_IRQ_DOMAIN_NAME_MAX]; ///< Public structure field.
    char controller_path[RAD_TREE_MAX_PATH]; ///< Public structure field.
} rad_irq_resource_t; ///< Public typedef alias.

/** @brief Public data structure for rad_i2c_device_info. */
typedef struct rad_i2c_device_info {
    uint32_t bus_id; ///< Public structure field.
    uint8_t address; ///< Public structure field.
    uint32_t irq_count; ///< Public structure field.
    rad_irq_resource_t irqs[RAD_IRQ_MAX_RESOURCES]; ///< Public structure field.
    char path[RAD_TREE_MAX_PATH]; ///< Public structure field.
    char compatible[RAD_COMPATIBLE_MAX]; ///< Public structure field.
    char driver[RAD_DRIVER_NAME_MAX]; ///< Public structure field.
    int bound; ///< Public structure field.
} rad_i2c_device_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_spi_device_info. */
typedef struct rad_spi_device_info {
    uint32_t bus_id; ///< Public structure field.
    uint8_t cs; ///< Public structure field.
    uint32_t clock_hz; ///< Public structure field.
    uint8_t mode; ///< Public structure field.
    uint8_t bits_per_word; ///< Public structure field.
    uint8_t transfer_mode; ///< Public structure field.
    uint32_t irq_count; ///< Public structure field.
    rad_irq_resource_t irqs[RAD_IRQ_MAX_RESOURCES]; ///< Public structure field.
    char path[RAD_TREE_MAX_PATH]; ///< Public structure field.
    char compatible[RAD_COMPATIBLE_MAX]; ///< Public structure field.
    char driver[RAD_DRIVER_NAME_MAX]; ///< Public structure field.
    int bound; ///< Public structure field.
} rad_spi_device_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_dma_channel_info. */
typedef struct rad_dma_channel_info {
    char controller[RAD_DRIVER_NAME_MAX]; ///< Public structure field.
    uint32_t index; ///< Public structure field.
    uint32_t request_id; ///< Public structure field.
    int allocated; ///< Public structure field.
    int active; ///< Public structure field.
} rad_dma_channel_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_irq_info. */
typedef struct rad_irq_info {
    uint32_t irq; ///< Public structure field.
    char name[RAD_IRQ_NAME_MAX]; ///< Public structure field.
    int registered; ///< Public structure field.
    int enabled; ///< Public structure field.
    uint64_t count; ///< Public structure field.
    uint64_t unhandled_count; ///< Public structure field.
} rad_irq_info_t; ///< Public typedef alias.

/** @brief Public data structure for rad_audio_format. */
typedef struct rad_audio_format {
    uint32_t sample_rate; ///< Public structure field.
    uint16_t channels; ///< Public structure field.
    uint16_t bits_per_sample; ///< Public structure field.
} rad_audio_format_t; ///< Public typedef alias.

/** @brief Public data structure for rad_serial_config. */
typedef struct rad_serial_config {
    uint32_t baud_rate; ///< Public structure field.
    uint8_t data_bits; ///< Public structure field.
    uint8_t stop_bits; ///< Public structure field.
    uint8_t parity; ///< Public structure field.
    uint8_t flow_control; ///< Public structure field.
} rad_serial_config_t; ///< Public typedef alias.

/** @brief Public data structure for rad_sd_config. */
typedef struct rad_sd_config {
    rad_sd_mode_t mode; ///< Public structure field.
    const char *device_name; ///< Public structure field.
    const char *mount_point; ///< Public structure field.
    uint8_t spi_instance; ///< Public structure field.
    uint8_t sck_gpio; ///< Public structure field.
    uint8_t tx_gpio; ///< Public structure field.
    uint8_t rx_gpio; ///< Public structure field.
    uint8_t cs_gpio; ///< Public structure field.
    uint8_t pio_instance; ///< Public structure field.
    uint8_t sdio_clk_gpio; ///< Public structure field.
    uint8_t sdio_cmd_gpio; ///< Public structure field.
    uint8_t sdio_dat0_gpio; ///< Public structure field.
    uint8_t card_detect_gpio; ///< Public structure field.
} rad_sd_config_t; ///< Public typedef alias.

/** @brief Public callback typedef used by the RADix API. */
typedef void (*rad_task_entry_t)(void *context);
/** @brief Public callback typedef used by the RADix API. */
typedef void (*rad_terminal_write_t)(const char *text, void *context);
/** @brief Public callback typedef used by the RADix API. */
typedef rad_status_t (*rad_terminal_handler_t)(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void *command_context);
/** @brief Public callback typedef used by the RADix API. */
typedef void (*rad_tty_output_t)(const void *data, size_t size, void *context);
/** @brief Public callback typedef used by the RADix API. */
typedef void (*rad_irq_handler_t)(uint32_t irq, void *context);
/** @brief Public callback typedef used by the RADix API. */
typedef void (*rad_work_handler_t)(void *context);

/** @brief Opaque handle type rad_task_t backed by rad_task_handle. */
typedef struct rad_task_handle *rad_task_t;
/** @brief Opaque handle type rad_mutex_t backed by rad_mutex_handle. */
typedef struct rad_mutex_handle *rad_mutex_t;
/** @brief Opaque handle type rad_event_t backed by rad_event_handle. */
typedef struct rad_event_handle *rad_event_t;
/** @brief Opaque handle type rad_wait_queue_t backed by rad_wait_queue_handle. */
typedef struct rad_wait_queue_handle *rad_wait_queue_t;
/** @brief Opaque handle type rad_input_queue_t backed by rad_input_queue_handle. */
typedef struct rad_input_queue_handle *rad_input_queue_t;
/** @brief Opaque handle type rad_file_t backed by rad_file_handle. */
typedef struct rad_file_handle *rad_file_t;
/** @brief Opaque handle type rad_dir_t backed by rad_dir_handle. */
typedef struct rad_dir_handle *rad_dir_t;
/** @brief Opaque handle type rad_device_t backed by rad_device_handle. */
typedef struct rad_device_handle *rad_device_t;
/** @brief Opaque handle type rad_program_t backed by rad_program_handle. */
typedef struct rad_program_handle *rad_program_t;
/** @brief Opaque handle type rad_tty_t backed by rad_tty_handle. */
typedef struct rad_tty_handle *rad_tty_t;
/** @brief Opaque handle type rad_pty_t backed by rad_pty_handle. */
typedef struct rad_pty_handle *rad_pty_t;

/** @brief Public data structure for rad_tty_window_size. */
typedef struct rad_tty_window_size {
    uint16_t rows; ///< Public structure field.
    uint16_t columns; ///< Public structure field.
    uint16_t x_pixels; ///< Public structure field.
    uint16_t y_pixels; ///< Public structure field.
} rad_tty_window_size_t; ///< Public typedef alias.

#define RAD_TTY_NCCS 32u
#define RAD_TTY_FLUSH_INPUT 1u
#define RAD_TTY_FLUSH_OUTPUT 2u

/** @brief POSIX-shaped terminal attributes stored by RADix TTY devices. */
typedef struct rad_tty_termios {
    uint32_t input_flags; ///< Public structure field.
    uint32_t output_flags; ///< Public structure field.
    uint32_t control_flags; ///< Public structure field.
    uint32_t local_flags; ///< Public structure field.
    uint32_t input_speed; ///< Public structure field.
    uint32_t output_speed; ///< Public structure field.
    uint8_t control_chars[RAD_TTY_NCCS]; ///< Public structure field.
} rad_tty_termios_t; ///< Public typedef alias.

/** @brief Public data structure for rad_log_entry. */
typedef struct rad_log_entry {
    uint64_t sequence; ///< Public structure field.
    uint64_t time_millis; ///< Public structure field.
    rad_log_level_t level; ///< Public structure field.
    char category[RAD_LOG_CATEGORY_MAX]; ///< Public structure field.
    char message[RAD_LOG_MESSAGE_MAX]; ///< Public structure field.
} rad_log_entry_t; ///< Public typedef alias.

rad_status_t rad_kernel_init(const rad_kernel_config_t *config); ///< Public RADix kernel API entry point.
void rad_kernel_shutdown(void); ///< Public RADix kernel API entry point.
int rad_kernel_is_initialized(void); ///< Public RADix kernel API entry point.
const char *rad_kernel_backend_name(void); ///< Return the active backend name string.
const char *rad_kernel_version_string(void); ///< Return the kernel version string.
rad_status_t rad_kernel_poll(void); ///< Public RADix kernel API entry point.
rad_status_t rad_kernel_run(void); ///< Public RADix kernel API entry point.
void rad_kernel_request_shutdown(void); ///< Public RADix kernel API entry point.
int rad_kernel_is_shutdown_requested(void); ///< Public RADix kernel API entry point.
int rad_vprintk(const char *format, va_list args); ///< Public RADix kernel API entry point.
int rad_printk(const char *format, ...); ///< Public RADix kernel API entry point.
int rad_vkprintk(rad_kernel_print_level_t level, const char *format, va_list args); ///< Public RADix kernel API entry point.
int rad_kprintk(rad_kernel_print_level_t level, const char *format, ...); ///< Public RADix kernel API entry point.
int rad_early_vprintk(const char *format, va_list args); ///< Public RADix kernel API entry point.
int rad_early_printk(const char *format, ...); ///< Public RADix kernel API entry point.
void rad_debug_marker(const char *marker); ///< Public RADix kernel API entry point.
rad_status_t rad_log_write(rad_log_level_t level, const char *category, const char *message); ///< Public RADix kernel API entry point.
size_t rad_log_read(rad_log_entry_t *entries, size_t capacity, uint64_t after_sequence); ///< Public RADix kernel API entry point.
rad_status_t rad_log_flush_to_path(const char *path); ///< Public RADix kernel API entry point.
rad_status_t rad_cpu_interrupts_enable(void); ///< Public RADix kernel API entry point.
rad_status_t rad_cpu_interrupts_disable(void); ///< Public RADix kernel API entry point.
int rad_cpu_interrupts_enabled(void); ///< Public RADix kernel API entry point.
void rad_cpu_idle(void); ///< Public RADix kernel API entry point.
void rad_cpu_halt_forever(void); ///< Public RADix kernel API entry point.
rad_status_t rad_boot_info_set(const rad_boot_info_t *boot_info); ///< Public RADix kernel API entry point.
rad_status_t rad_boot_info_get(rad_boot_info_t *boot_info); ///< Public RADix kernel API entry point.
const char *rad_boot_arg_get(const char *key); ///< Return a boot argument value by key, or nullptr when absent.

uint64_t rad_time_micros(void); ///< Public RADix kernel API entry point.
uint64_t rad_time_millis(void); ///< Public RADix kernel API entry point.
void rad_sleep_ms(uint32_t milliseconds); ///< Public RADix kernel API entry point.
void rad_sleep_us(uint32_t microseconds); ///< Public RADix kernel API entry point.
uint64_t rad_perf_now_cycles(void); ///< Public RADix kernel API entry point.
void rad_perf_counter_add(const char *name, uint64_t delta); ///< Public RADix kernel API entry point.
size_t rad_perf_counter_list(rad_perf_counter_info_t *counters, size_t capacity); ///< Public RADix kernel API entry point.
rad_status_t rad_work_submit(const char *name, rad_work_handler_t handler, void *context); ///< Public RADix kernel API entry point.
rad_status_t rad_work_poll(size_t budget, size_t *ran); ///< Public RADix kernel API entry point.
rad_status_t rad_wait_queue_create(rad_wait_queue_t *queue); ///< Public RADix kernel API entry point.
void rad_wait_queue_destroy(rad_wait_queue_t queue); ///< Public RADix kernel API entry point.
rad_status_t rad_wait_queue_wait(rad_wait_queue_t queue, uint32_t timeout_ms); ///< Public RADix kernel API entry point.
rad_status_t rad_wait_queue_wake_one(rad_wait_queue_t queue); ///< Public RADix kernel API entry point.
rad_status_t rad_wait_queue_wake_all(rad_wait_queue_t queue); ///< Public RADix kernel API entry point.
rad_status_t rad_timer_source_register(const rad_timer_source_config_t *config, const rad_timer_source_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_timer_source_unregister(const char *name); ///< Public RADix kernel API entry point.
void rad_timer_tick(uint64_t elapsed_micros); ///< Public RADix kernel API entry point.
rad_status_t rad_timer_schedule_oneshot(uint64_t delay_micros); ///< Public RADix kernel API entry point.
rad_status_t rad_timer_cancel_oneshot(void); ///< Public RADix kernel API entry point.
size_t rad_timer_list(rad_timer_source_info_t *timers, size_t capacity); ///< Public RADix kernel API entry point.

rad_status_t rad_task_create(rad_task_t *task, const char *name, rad_task_entry_t entry, void *context, size_t stack_size); ///< Public RADix kernel API entry point.
rad_status_t rad_task_create_config(rad_task_t *task, const rad_task_config_t *config, rad_task_entry_t entry, void *context); ///< Public RADix kernel API entry point.
rad_status_t rad_task_join(rad_task_t task); ///< Public RADix kernel API entry point.
void rad_task_detach(rad_task_t task); ///< Public RADix kernel API entry point.
uint64_t rad_task_current_id(void); ///< Public RADix kernel API entry point.
int rad_task_current_core(void); ///< Public RADix kernel API entry point.
void rad_task_yield(void); ///< Public RADix kernel API entry point.
void rad_task_sleep_ms(uint32_t milliseconds); ///< Public RADix kernel API entry point.
void rad_task_sleep_us(uint32_t microseconds); ///< Public RADix kernel API entry point.
size_t rad_task_list(rad_task_info_t *tasks, size_t capacity); ///< Public RADix kernel API entry point.
rad_status_t rad_core_info_get(rad_core_info_t *info); ///< Public RADix kernel API entry point.
rad_status_t rad_scheduler_info_get(rad_scheduler_info_t *info); ///< Public RADix kernel API entry point.
void rad_scheduler_yield_from_irq(void); ///< Public RADix kernel API entry point.
void rad_scheduler_set_preemption_enabled(int enabled); ///< Public RADix kernel API entry point.
uint32_t rad_scheduler_current_core(void); ///< Public RADix kernel API entry point.
uint32_t rad_scheduler_online_core_mask(void); ///< Public RADix kernel API entry point.

rad_status_t rad_mutex_create(rad_mutex_t *mutex); ///< Public RADix kernel API entry point.
void rad_mutex_destroy(rad_mutex_t mutex); ///< Public RADix kernel API entry point.
rad_status_t rad_mutex_lock(rad_mutex_t mutex); ///< Public RADix kernel API entry point.
rad_status_t rad_mutex_unlock(rad_mutex_t mutex); ///< Public RADix kernel API entry point.

rad_status_t rad_event_create(rad_event_t *event, int initially_signaled); ///< Public RADix kernel API entry point.
void rad_event_destroy(rad_event_t event); ///< Public RADix kernel API entry point.
rad_status_t rad_event_wait(rad_event_t event, uint32_t timeout_ms); ///< Public RADix kernel API entry point.
rad_status_t rad_event_signal(rad_event_t event); ///< Public RADix kernel API entry point.
rad_status_t rad_event_reset(rad_event_t event); ///< Public RADix kernel API entry point.

void *rad_memory_alloc(size_t size); ///< Allocate kernel-managed memory.
void rad_memory_free(void *pointer); ///< Public RADix kernel API entry point.
void rad_memory_get_stats(rad_memory_stats_t *stats); ///< Public RADix kernel API entry point.

rad_status_t rad_vfs_mount_host(const char *mount_point, const char *host_path); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_mount_sd(const rad_sd_config_t *config); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_mount_provider(const char *mount_point, const rad_vfs_backend_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_unmount(const char *mount_point); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_open(const char *path, uint32_t flags, rad_file_t *file); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_read(rad_file_t file, void *buffer, size_t size, size_t *bytes_read); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_write(rad_file_t file, const void *buffer, size_t size, size_t *bytes_written); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_seek(rad_file_t file, int64_t offset, rad_seek_origin_t origin); ///< Public RADix kernel API entry point.
uint64_t rad_vfs_tell(rad_file_t file); ///< Public RADix kernel API entry point.
void rad_vfs_close(rad_file_t file); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_stat(const char *path, rad_vfs_stat_t *stat); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_list(const char *path, rad_vfs_list_callback_t callback, void *context); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_mkdir(const char *path); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_remove(const char *path); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_rename(const char *old_path, const char *new_path); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_rmdir(const char *path); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_fsync(rad_file_t file); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_truncate(const char *path, uint64_t size); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_readlink(const char *path, char *buffer, size_t size); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_symlink(const char *target, const char *link_path); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_link(const char *old_path, const char *new_path); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_chmod(const char *path, uint32_t mode); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_chdir(const char *path); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_getcwd(char *buffer, size_t size); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_opendir(const char *path, rad_dir_t *dir); ///< Public RADix kernel API entry point.
rad_status_t rad_vfs_readdir(rad_dir_t dir, rad_vfs_dirent_t *entry); ///< Public RADix kernel API entry point.
void rad_vfs_closedir(rad_dir_t dir); ///< Public RADix kernel API entry point.

int32_t rad_process_current_pid(void); ///< Public RADix kernel API entry point.
int32_t rad_process_parent_pid(void); ///< Public RADix kernel API entry point.
int32_t rad_process_fork(void); ///< Public RADix kernel API entry point.
int32_t rad_process_execve(const char *path, const char *const argv[]); ///< Public RADix kernel API entry point.
int32_t rad_process_waitpid(int32_t pid, int32_t *status, uint32_t options); ///< Public RADix kernel API entry point.
void rad_process_exit(int32_t status); ///< Public RADix kernel API entry point.
size_t rad_process_list(rad_process_info_t *processes, size_t capacity); ///< Public RADix kernel API entry point.
int32_t rad_process_create(const char *path, int32_t parent_pid); ///< Public RADix kernel API entry point.
rad_status_t rad_process_attach_task(int32_t pid, rad_task_t task); ///< Public RADix kernel API entry point.
void rad_process_set_current_pid(int32_t pid); ///< Public RADix kernel API entry point.
rad_status_t rad_process_mark_exec(int32_t pid, const char *path); ///< Public RADix kernel API entry point.
rad_status_t rad_process_clone_fds(int32_t parent_pid, int32_t child_pid); ///< Public RADix kernel API entry point.
rad_status_t rad_process_arch_register(const rad_process_arch_ops_t *ops); ///< Public RADix kernel API entry point.
int32_t rad_process_fork_from_arch_frame(void *trap_frame); ///< Public RADix kernel API entry point.
int32_t rad_process_reap(int32_t pid, int32_t *status); ///< Public RADix kernel API entry point.
rad_status_t rad_process_get_credentials(int32_t pid, rad_credentials_t *credentials); ///< Public RADix kernel API entry point.
rad_status_t rad_process_set_credentials(int32_t pid, const rad_credentials_t *credentials); ///< Public RADix kernel API entry point.
rad_uid_t rad_process_getuid(void); ///< Public RADix kernel API entry point.
rad_uid_t rad_process_geteuid(void); ///< Public RADix kernel API entry point.
rad_gid_t rad_process_getgid(void); ///< Public RADix kernel API entry point.
rad_gid_t rad_process_getegid(void); ///< Public RADix kernel API entry point.
rad_status_t rad_process_setuid(rad_uid_t uid); ///< Public RADix kernel API entry point.
rad_status_t rad_process_setgid(rad_gid_t gid); ///< Public RADix kernel API entry point.

rad_status_t rad_module_register(const rad_module_descriptor_t *descriptor); ///< Public RADix kernel API entry point.
rad_status_t rad_module_unregister(const char *name); ///< Public RADix kernel API entry point.
size_t rad_module_list(rad_module_info_t *modules, size_t capacity); ///< Public RADix kernel API entry point.
rad_status_t rad_service_register(const rad_service_descriptor_t *descriptor); ///< Public RADix kernel API entry point.
rad_status_t rad_service_unregister(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_service_configure(const char *name, const rad_service_config_t *config); ///< Public RADix kernel API entry point.
rad_status_t rad_service_configure_tree(rad_tree_node_t node); ///< Public RADix kernel API entry point.
rad_status_t rad_service_start(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_service_stop(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_service_poll_all(void); ///< Public RADix kernel API entry point.
size_t rad_service_list(rad_service_info_t *services, size_t capacity); ///< Public RADix kernel API entry point.

rad_status_t rad_irq_register(uint32_t irq, const char *name, rad_irq_handler_t handler, void *context); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_unregister(uint32_t irq); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_enable(uint32_t irq); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_disable(uint32_t irq); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_dispatch(uint32_t irq); ///< Public RADix kernel API entry point.
size_t rad_irq_list(rad_irq_info_t *irqs, size_t capacity); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_domain_register(const rad_irq_domain_config_t *config, rad_irq_domain_t *domain); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_domain_unregister(const char *tree_path); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_domain_find(const char *tree_path, rad_irq_domain_t *domain); ///< Public RADix kernel API entry point.
size_t rad_irq_resolve_tree(rad_tree_node_t node, rad_irq_resource_t *resources, size_t capacity); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_resource_enable(const rad_irq_resource_t *resource); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_resource_disable(const rad_irq_resource_t *resource); ///< Public RADix kernel API entry point.
rad_status_t rad_irq_resource_register_handler(const rad_irq_resource_t *resource, const char *name, rad_irq_handler_t handler, void *context); ///< Public RADix kernel API entry point.

int32_t rad_fd_open(const char *path, uint32_t flags); ///< Public RADix kernel API entry point.
int32_t rad_fd_close(int32_t fd); ///< Public RADix kernel API entry point.
intptr_t rad_fd_read(int32_t fd, void *buffer, size_t size); ///< Public RADix kernel API entry point.
intptr_t rad_fd_write(int32_t fd, const void *buffer, size_t size); ///< Public RADix kernel API entry point.
int64_t rad_fd_lseek(int32_t fd, int64_t offset, rad_seek_origin_t origin); ///< Public RADix kernel API entry point.
int32_t rad_fd_ioctl(int32_t fd, uint32_t request, void *argument); ///< Public RADix kernel API entry point.
intptr_t rad_fd_getdents(int32_t fd, rad_dirent_user_t *entries, size_t capacity); ///< Public RADix kernel API entry point.
int32_t rad_fd_stat(const char *path, rad_vfs_stat_t *stat); ///< Public RADix kernel API entry point.
int32_t rad_fd_fstat(int32_t fd, rad_vfs_stat_t *stat); ///< Public RADix kernel API entry point.
int32_t rad_fd_dup(int32_t fd); ///< Public RADix kernel API entry point.
int32_t rad_fd_dup2(int32_t old_fd, int32_t new_fd); ///< Public RADix kernel API entry point.
int32_t rad_fd_fcntl(int32_t fd, uint32_t command, uintptr_t argument); ///< Public RADix kernel API entry point.
int32_t rad_fd_poll(rad_pollfd_t *fds, size_t count, int32_t timeout_ms); ///< Public RADix kernel API entry point.
int32_t rad_pipe_create(int32_t pipefd[2]); ///< Public RADix kernel API entry point.
int32_t rad_shm_open(const char *name, size_t byte_size, uint32_t flags); ///< Public RADix kernel API entry point.
int32_t rad_shm_unlink(const char *name); ///< Public RADix kernel API entry point.
int32_t rad_shm_get_info(int32_t fd, rad_shm_info_t *info); ///< Public RADix kernel API entry point.
int32_t rad_shm_set_page(int32_t fd, size_t page_index, uintptr_t page_token); ///< Public RADix kernel API entry point.
int32_t rad_shm_get_page(int32_t fd, size_t page_index, uintptr_t *page_token); ///< Public RADix kernel API entry point.
void *rad_shm_kernel_pointer(int32_t fd); ///< Public RADix kernel API entry point.

intptr_t rad_syscall_dispatch(uintptr_t number, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5); ///< Public RADix kernel API entry point.

rad_status_t rad_program_load(const char *path, rad_program_t *program); ///< Public RADix kernel API entry point.
rad_status_t rad_program_spawn(rad_program_t program, int argc, const char **argv, rad_task_t *task); ///< Public RADix kernel API entry point.
void rad_program_unload(rad_program_t program); ///< Public RADix kernel API entry point.
size_t rad_program_list(rad_program_info_t *programs, size_t capacity); ///< Public RADix kernel API entry point.

rad_status_t rad_overlay_load_memory(const void *data, size_t size); ///< Public RADix kernel API entry point.
rad_status_t rad_overlay_load_file(const char *path); ///< Public RADix kernel API entry point.
rad_status_t rad_overlay_apply_boot(void); ///< Public RADix kernel API entry point.
size_t rad_overlay_list(rad_overlay_info_t *overlays, size_t capacity); ///< Public RADix kernel API entry point.
size_t rad_tree_list(rad_tree_node_info_t *nodes, size_t capacity); ///< Public RADix kernel API entry point.
rad_tree_node_t rad_tree_find(const char *path); ///< Public RADix kernel API entry point.
rad_status_t rad_tree_get_property_u32(rad_tree_node_t node, const char *name, uint32_t *value); ///< Public RADix kernel API entry point.
size_t rad_tree_get_property_u32_array(rad_tree_node_t node, const char *name, uint32_t *values, size_t capacity); ///< Public RADix kernel API entry point.
rad_status_t rad_tree_get_property_bool(rad_tree_node_t node, const char *name, int *value); ///< Public RADix kernel API entry point.
rad_status_t rad_tree_get_property_string(rad_tree_node_t node, const char *name, char *buffer, size_t size); ///< Public RADix kernel API entry point.

rad_status_t rad_device_register(const char *name, rad_device_type_t type, const rad_device_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_device_unregister(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_device_open(const char *name, rad_device_t *device); ///< Public RADix kernel API entry point.
void rad_device_close(rad_device_t device); ///< Public RADix kernel API entry point.
rad_status_t rad_device_read(rad_device_t device, void *buffer, size_t size, size_t *bytes_read); ///< Public RADix kernel API entry point.
rad_status_t rad_device_write(rad_device_t device, const void *buffer, size_t size, size_t *bytes_written); ///< Public RADix kernel API entry point.
rad_status_t rad_device_ioctl(rad_device_t device, uint32_t request, void *argument); ///< Public RADix kernel API entry point.
size_t rad_device_list(char names[][64], rad_device_type_t *types, size_t capacity); ///< Public RADix kernel API entry point.

rad_status_t rad_input_device_register(const char *name, const rad_device_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_input_device_unregister(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_input_open(const char *name, rad_device_t *device); ///< Public RADix kernel API entry point.
rad_status_t rad_input_read_event(rad_device_t device, rad_input_event_t *event); ///< Public RADix kernel API entry point.
rad_status_t rad_input_queue_create(const char *name, size_t capacity, rad_input_queue_t *queue); ///< Public RADix kernel API entry point.
void rad_input_queue_destroy(rad_input_queue_t queue); ///< Public RADix kernel API entry point.
rad_status_t rad_input_queue_push(rad_input_queue_t queue, const rad_input_event_t *event); ///< Public RADix kernel API entry point.
rad_status_t rad_input_queue_read(rad_input_queue_t queue, rad_input_event_t *events, size_t capacity, uint32_t timeout_ms, size_t *count); ///< Public RADix kernel API entry point.
rad_status_t rad_input_device_register_queue(const char *name, rad_input_queue_t queue); ///< Public RADix kernel API entry point.

rad_status_t rad_block_device_register(const char *name, const rad_device_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_block_open(const char *name, rad_device_t *device); ///< Public RADix kernel API entry point.
rad_status_t rad_block_info(rad_device_t device, rad_block_info_t *info); ///< Public RADix kernel API entry point.
rad_status_t rad_block_read(rad_device_t device, uint64_t sector, uint32_t sector_count, void *buffer); ///< Public RADix kernel API entry point.
rad_status_t rad_block_write(rad_device_t device, uint64_t sector, uint32_t sector_count, const void *buffer); ///< Public RADix kernel API entry point.
rad_status_t rad_block_flush(rad_device_t device); ///< Public RADix kernel API entry point.
rad_status_t rad_net_device_register(const char *name, const rad_device_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_net_open(const char *name, rad_device_t *device); ///< Public RADix kernel API entry point.
rad_status_t rad_net_link_info(rad_device_t device, rad_net_link_info_t *info); ///< Public RADix kernel API entry point.
rad_status_t rad_net_send(rad_device_t device, const void *data, size_t length); ///< Public RADix kernel API entry point.
intptr_t rad_net_receive(rad_device_t device, void *data, size_t length); ///< Public RADix kernel API entry point.
rad_status_t rad_net_poll(rad_device_t device); ///< Public RADix kernel API entry point.
int32_t rad_socket_create(int domain, int type, int protocol); ///< Public RADix kernel API entry point.
int32_t rad_socket_bind(int32_t fd, const rad_sockaddr_in_t *address, size_t address_length); ///< Public RADix kernel API entry point.
int32_t rad_socket_listen(int32_t fd, int backlog); ///< Public RADix kernel API entry point.
int32_t rad_socket_accept(int32_t fd, rad_sockaddr_in_t *address, size_t *address_length); ///< Public RADix kernel API entry point.
int32_t rad_socket_connect(int32_t fd, const rad_sockaddr_in_t *address, size_t address_length); ///< Public RADix kernel API entry point.
intptr_t rad_socket_sendto(int32_t fd, const void *buffer, size_t size, uint32_t flags, const rad_sockaddr_in_t *address, size_t address_length); ///< Public RADix kernel API entry point.
intptr_t rad_socket_recvfrom(int32_t fd, void *buffer, size_t size, uint32_t flags, rad_sockaddr_in_t *address, size_t *address_length); ///< Public RADix kernel API entry point.
intptr_t rad_socket_send(int32_t fd, const void *buffer, size_t size, uint32_t flags); ///< Public RADix kernel API entry point.
intptr_t rad_socket_recv(int32_t fd, void *buffer, size_t size, uint32_t flags); ///< Public RADix kernel API entry point.
int32_t rad_socket_shutdown(int32_t fd, int how); ///< Public RADix kernel API entry point.
int32_t rad_socket_get_info(int32_t fd, rad_socket_info_t *info); ///< Public RADix kernel API entry point.
int32_t rad_socket_setsockopt(int32_t fd, int level, int option, const void *value, size_t value_length); ///< Public RADix kernel API entry point.
int32_t rad_socket_getsockopt(int32_t fd, int level, int option, void *value, size_t *value_length); ///< Public RADix kernel API entry point.

rad_status_t rad_framebuffer_register(const char *name, const rad_framebuffer_info_t *info, const rad_framebuffer_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_register_ex(const rad_framebuffer_config_t *config, const rad_framebuffer_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_unregister(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_open(const char *name, rad_framebuffer_t *framebuffer); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_open_primary(rad_framebuffer_t *framebuffer); ///< Public RADix kernel API entry point.
void rad_framebuffer_close(rad_framebuffer_t framebuffer); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_get_info(rad_framebuffer_t framebuffer, rad_framebuffer_info_t *info); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_get_display_info(rad_framebuffer_t framebuffer, rad_framebuffer_display_info_t *info); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_set_primary(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_set_mode(rad_framebuffer_t framebuffer, uint32_t mode_index); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_blank(rad_framebuffer_t framebuffer, int blanked); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_get_vsync_counter(rad_framebuffer_t framebuffer, uint64_t *counter); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_clear(rad_framebuffer_t framebuffer, uint32_t color); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_draw_text(rad_framebuffer_t framebuffer, uint32_t cell_x, uint32_t cell_y, const char *text, uint32_t foreground, uint32_t background); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_flush(rad_framebuffer_t framebuffer, const rad_framebuffer_rect_t *rect); ///< Public RADix kernel API entry point.
rad_status_t rad_framebuffer_present(rad_framebuffer_t framebuffer, const rad_framebuffer_present_t *present); ///< Public RADix kernel API entry point.
size_t rad_framebuffer_list(rad_framebuffer_info_t *framebuffers, char names[][64], size_t capacity); ///< Public RADix kernel API entry point.
size_t rad_framebuffer_list_ex(rad_framebuffer_display_info_t *framebuffers, size_t capacity); ///< Public RADix kernel API entry point.

rad_status_t rad_i2c_controller_register(const rad_i2c_controller_config_t *config, const rad_i2c_controller_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_controller_unregister(uint32_t bus_id); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_bus_open(uint32_t bus_id, rad_i2c_bus_t *bus); ///< Public RADix kernel API entry point.
void rad_i2c_bus_close(rad_i2c_bus_t bus); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_transfer(rad_i2c_bus_t bus, const rad_i2c_transfer_t *transfer); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_bind_tree(void); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_driver_register(const rad_i2c_driver_t *driver); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_driver_unregister(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_device_open(uint32_t bus_id, uint8_t address, rad_i2c_device_t **device); ///< Public RADix kernel API entry point.
void rad_i2c_device_close(rad_i2c_device_t *device); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_device_transfer(rad_i2c_device_t *device, const rad_i2c_transfer_t *transfer); ///< Public RADix kernel API entry point.
size_t rad_i2c_device_irq_count(const rad_i2c_device_t *device); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_device_get_irq(const rad_i2c_device_t *device, size_t index, rad_irq_resource_t *resource); ///< Public RADix kernel API entry point.
size_t rad_i2c_list_devices(rad_i2c_device_info_t *devices, size_t capacity); ///< Public RADix kernel API entry point.
rad_status_t rad_i2c_transfer_device(rad_device_t device, const rad_i2c_transfer_t *transfer); ///< Public RADix kernel API entry point.

rad_status_t rad_spi_controller_register(const rad_spi_controller_config_t *config, const rad_spi_controller_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_spi_controller_unregister(uint32_t bus_id); ///< Public RADix kernel API entry point.
rad_status_t rad_spi_bus_open(uint32_t bus_id, rad_spi_bus_t *bus); ///< Public RADix kernel API entry point.
void rad_spi_bus_close(rad_spi_bus_t bus); ///< Public RADix kernel API entry point.
rad_status_t rad_spi_transfer(rad_spi_bus_t bus, const rad_spi_transfer_t *transfer); ///< Public RADix kernel API entry point.
rad_status_t rad_spi_driver_register(const rad_spi_driver_t *driver); ///< Public RADix kernel API entry point.
rad_status_t rad_spi_driver_unregister(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_spi_device_open(uint32_t bus_id, uint8_t cs, rad_spi_device_t **device); ///< Public RADix kernel API entry point.
void rad_spi_device_close(rad_spi_device_t *device); ///< Public RADix kernel API entry point.
rad_status_t rad_spi_device_transfer(rad_spi_device_t *device, const rad_spi_transfer_t *transfer); ///< Public RADix kernel API entry point.
size_t rad_spi_device_irq_count(const rad_spi_device_t *device); ///< Public RADix kernel API entry point.
rad_status_t rad_spi_device_get_irq(const rad_spi_device_t *device, size_t index, rad_irq_resource_t *resource); ///< Public RADix kernel API entry point.
size_t rad_spi_list_devices(rad_spi_device_info_t *devices, size_t capacity); ///< Public RADix kernel API entry point.
rad_status_t rad_spi_transfer_device(rad_device_t device, const rad_spi_transfer_t *transfer); ///< Public RADix kernel API entry point.

rad_status_t rad_dma_controller_register(const rad_dma_controller_config_t *config, const rad_dma_backend_ops_t *ops); ///< Public RADix kernel API entry point.
rad_status_t rad_dma_controller_unregister(const char *name); ///< Public RADix kernel API entry point.
rad_status_t rad_dma_channel_request(rad_dma_request_id_t request_id, rad_dma_channel_t *channel); ///< Public RADix kernel API entry point.
void rad_dma_channel_release(rad_dma_channel_t channel); ///< Public RADix kernel API entry point.
rad_status_t rad_dma_submit(rad_dma_channel_t channel, const rad_dma_transfer_t *transfer); ///< Public RADix kernel API entry point.
rad_status_t rad_dma_wait(rad_dma_channel_t channel, uint32_t timeout_ms); ///< Public RADix kernel API entry point.
rad_status_t rad_dma_cancel(rad_dma_channel_t channel); ///< Public RADix kernel API entry point.
size_t rad_dma_list_channels(rad_dma_channel_info_t *channels, size_t capacity); ///< Public RADix kernel API entry point.

size_t rad_driver_list(rad_driver_info_t *drivers, size_t capacity); ///< Public RADix kernel API entry point.
rad_status_t rad_audio_configure(rad_device_t device, const rad_audio_format_t *format); ///< Public RADix kernel API entry point.
rad_status_t rad_serial_configure(rad_device_t device, const rad_serial_config_t *config); ///< Public RADix kernel API entry point.

rad_status_t rad_tty_open(const char *name, rad_tty_t *tty); ///< Public RADix kernel API entry point.
void rad_tty_close(rad_tty_t tty); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_read(rad_tty_t tty, void *buffer, size_t size, size_t *bytes_read); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_write(rad_tty_t tty, const void *buffer, size_t size, size_t *bytes_written); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_push_input(rad_tty_t tty, const void *buffer, size_t size, size_t *bytes_consumed); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_set_output_callback(rad_tty_t tty, rad_tty_output_t output, void *context); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_set_window_size(rad_tty_t tty, const rad_tty_window_size_t *size); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_get_window_size(rad_tty_t tty, rad_tty_window_size_t *size); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_set_mode(rad_tty_t tty, uint32_t mode); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_get_mode(rad_tty_t tty, uint32_t *mode); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_set_termios(rad_tty_t tty, const rad_tty_termios_t *termios); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_get_termios(rad_tty_t tty, rad_tty_termios_t *termios); ///< Public RADix kernel API entry point.
rad_status_t rad_tty_flush(rad_tty_t tty, uint32_t queues); ///< Public RADix kernel API entry point.

rad_status_t rad_pty_open_pair(const char *name, rad_pty_t *pty); ///< Public RADix kernel API entry point.
void rad_pty_close(rad_pty_t pty); ///< Public RADix kernel API entry point.
rad_status_t rad_pty_read_master(rad_pty_t pty, void *buffer, size_t size, size_t *bytes_read); ///< Public RADix kernel API entry point.
rad_status_t rad_pty_write_master(rad_pty_t pty, const void *buffer, size_t size, size_t *bytes_written); ///< Public RADix kernel API entry point.
rad_status_t rad_pty_read_slave(rad_pty_t pty, void *buffer, size_t size, size_t *bytes_read); ///< Public RADix kernel API entry point.
rad_status_t rad_pty_write_slave(rad_pty_t pty, const void *buffer, size_t size, size_t *bytes_written); ///< Public RADix kernel API entry point.
rad_status_t rad_pty_resize(rad_pty_t pty, const rad_tty_window_size_t *size); ///< Public RADix kernel API entry point.
rad_status_t rad_pty_slave_name(rad_pty_t pty, char *buffer, size_t size); ///< Public RADix kernel API entry point.

rad_status_t rad_terminal_register_command(const char *name, const char *description, rad_terminal_handler_t handler, void *context); ///< Public RADix kernel API entry point.
rad_status_t rad_terminal_execute(const char *line, rad_terminal_write_t write, void *write_context); ///< Public RADix kernel API entry point.
rad_status_t rad_terminal_poll_tty(rad_tty_t tty); ///< Public RADix kernel API entry point.
rad_status_t rad_terminal_attach_device(const char *device_name); ///< Public RADix kernel API entry point.
rad_status_t rad_terminal_poll_attached(void); ///< Public RADix kernel API entry point.
size_t rad_terminal_command_count(void); ///< Public RADix kernel API entry point.

#if defined(__cplusplus)
}

#ifndef RADLIB_EMBEDDED_NO_CPP_WRAPPERS
namespace RADEmbedded {

class Kernel {
public:
    static rad_status_t init(const char *backend = "linux_sim") {
        rad_kernel_config_t config{};
        config.backend_name = backend;
        return rad_kernel_init(&config); //< Public RADix kernel API entry point.
    }
    static void shutdown() { rad_kernel_shutdown(); }
    static bool initialized() { return rad_kernel_is_initialized() != 0; }
    static std::string backendName() { return rad_kernel_backend_name(); }
    static std::string version() { return rad_kernel_version_string(); }
};

class Mutex {
public:
    Mutex() { rad_mutex_create(&handle_); }
    ~Mutex() { rad_mutex_destroy(handle_); }
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    rad_status_t lock() { return rad_mutex_lock(handle_); }
    rad_status_t unlock() { return rad_mutex_unlock(handle_); }
    rad_mutex_t native() const { return handle_; }
private:
    rad_mutex_t handle_ = nullptr;
};

class Event {
public:
    explicit Event(bool signaled = false) { rad_event_create(&handle_, signaled ? 1 : 0); }
    ~Event() { rad_event_destroy(handle_); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    rad_status_t wait(uint32_t timeoutMs) { return rad_event_wait(handle_, timeoutMs); }
    rad_status_t signal() { return rad_event_signal(handle_); }
    rad_status_t reset() { return rad_event_reset(handle_); }
private:
    rad_event_t handle_ = nullptr;
};

class File {
public:
    File() = default;
    ~File() { close(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    bool open(const char *path, uint32_t flags) { close(); return rad_vfs_open(path, flags, &handle_) == RAD_STATUS_OK; }
    size_t read(void *buffer, size_t size) { size_t done = 0; rad_vfs_read(handle_, buffer, size, &done); return done; }
    size_t write(const void *buffer, size_t size) { size_t done = 0; rad_vfs_write(handle_, buffer, size, &done); return done; }
    void close() { if (handle_) { rad_vfs_close(handle_); handle_ = nullptr; } }
private:
    rad_file_t handle_ = nullptr;
};

} // namespace RADEmbedded
#endif
#endif

#endif
