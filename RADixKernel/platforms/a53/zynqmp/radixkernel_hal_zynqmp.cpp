#include <radixkernel/radixkernel.h>
#include <radboot.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void rad_hal_sleep_us(uint32_t microseconds);

namespace {
constexpr uintptr_t DefaultUartBase = 0xff000000u;
#ifndef RADIX_ZYNQMP_SDHCI_BASE
#define RADIX_ZYNQMP_SDHCI_BASE 0xff170000u
#endif
constexpr uintptr_t DefaultSdhciBase = RADIX_ZYNQMP_SDHCI_BASE;
constexpr uintptr_t DefaultGicDistributorBase = 0xf9010000u;
constexpr uintptr_t DefaultGicCpuBase = 0xf9020000u;
constexpr uintptr_t DefaultMemoryBase = 0x00000000u;
constexpr uintptr_t DefaultMemorySize = 1024u * 1024u * 1024u;
constexpr uint32_t CadenceUartSrRxEmpty = 0x00000002u;
constexpr uint32_t CadenceUartSrTxFull = 0x00000010u;
constexpr uint32_t CadenceUartCrRxReset = 0x00000001u;
constexpr uint32_t CadenceUartCrTxReset = 0x00000002u;
constexpr uint32_t CadenceUartCrRxEnable = 0x00000004u;
constexpr uint32_t CadenceUartCrTxEnable = 0x00000010u;
constexpr uint32_t SdhciSectorSize = 512u;
constexpr uint32_t SdhciPresentCmdInhibit = 1u << 0u;
constexpr uint32_t SdhciPresentDatInhibit = 1u << 1u;
constexpr uint32_t SdhciPresentReadReady = 1u << 11u;
constexpr uint32_t SdhciPresentWriteReady = 1u << 10u;
constexpr uint32_t SdhciIntCmdComplete = 1u << 0u;
constexpr uint32_t SdhciIntTransferComplete = 1u << 1u;
constexpr uint32_t SdhciIntBufferWriteReady = 1u << 4u;
constexpr uint32_t SdhciIntBufferReadReady = 1u << 5u;
constexpr uint32_t SdhciIntError = 0xffff0000u;
constexpr uint16_t SdhciCmdRespNone = 0x0000u;
constexpr uint16_t SdhciCmdRespLong = 0x0001u;
constexpr uint16_t SdhciCmdRespShort = 0x0002u;
constexpr uint16_t SdhciCmdRespShortBusy = 0x0003u;
constexpr uint16_t SdhciCmdCrc = 0x0008u;
constexpr uint16_t SdhciCmdIndex = 0x0010u;
constexpr uint16_t SdhciCmdData = 0x0020u;

struct ZynqmpState {
    uintptr_t uart_base = DefaultUartBase;
    uintptr_t sdhci_base = DefaultSdhciBase;
    uintptr_t gic_distributor_base = DefaultGicDistributorBase;
    uintptr_t gic_cpu_base = DefaultGicCpuBase;
    uintptr_t memory_base = DefaultMemoryBase;
    uintptr_t memory_size = DefaultMemorySize;
    uint32_t core_count = 2;
    uint32_t sd_ready = 0;
    uint32_t sd_high_capacity = 1;
    uint32_t sd_rca = 0;
    uint64_t sd_sector_count = 0;
    int uart_initialized = 0;
};

ZynqmpState g_zynqmp;

struct PartitionDevice {
    const char *name;
    uint64_t start_sector;
    uint64_t sector_count;
    int ready;
};

PartitionDevice g_partitions[2] = {
    {"/dev/mmcblk0p1", 0, 0, 0},
    {"/dev/mmcblk0p2", 0, 0, 0},
};

volatile uint32_t *reg32(uintptr_t address) {
    return reinterpret_cast<volatile uint32_t*>(address);
}

uint32_t read32(uintptr_t address) {
    return *reg32(address);
}

void write32(uintptr_t address, uint32_t value) {
    *reg32(address) = value;
}

uintptr_t uart(uintptr_t offset) {
    return g_zynqmp.uart_base + offset;
}

uintptr_t sdhci(uintptr_t offset) {
    return g_zynqmp.sdhci_base + offset;
}

void zynqmp_uart_init_once() {
    if (g_zynqmp.uart_initialized) return;
    write32(uart(0x00u), CadenceUartCrRxReset | CadenceUartCrTxReset);
    write32(uart(0x04u), 0x20u);
    write32(uart(0x00u), CadenceUartCrRxEnable | CadenceUartCrTxEnable);
    g_zynqmp.uart_initialized = 1;
}

uint8_t read8(uintptr_t address) {
    return *reinterpret_cast<volatile uint8_t*>(address);
}

void write16(uintptr_t address, uint16_t value) {
    *reinterpret_cast<volatile uint16_t*>(address) = value;
}

void write8(uintptr_t address, uint8_t value) {
    *reinterpret_cast<volatile uint8_t*>(address) = value;
}

void udelay(uint32_t us) {
    rad_hal_sleep_us(us);
}

int wait_mask(uintptr_t reg, uint32_t mask, uint32_t value, uint32_t spins) {
    for (uint32_t i = 0; i < spins; ++i) {
        if ((read32(reg) & mask) == value) return 1;
        if ((i & 0xffu) == 0) udelay(1);
    }
    return 0;
}

uint32_t sdhci_response32(void) {
    return read32(sdhci(0x10u));
}

rad_status_t sdhci_send_cmd(uint32_t index, uint32_t argument, uint16_t flags, uint32_t *response) {
    const uint32_t inhibit = (flags & SdhciCmdData) ? (SdhciPresentCmdInhibit | SdhciPresentDatInhibit) : SdhciPresentCmdInhibit;
    if (!wait_mask(sdhci(0x24u), inhibit, 0u, 1000000u)) return RAD_STATUS_TIMEOUT;
    write32(sdhci(0x30u), 0xffffffffu);
    write32(sdhci(0x08u), argument);
    write16(sdhci(0x0eu), static_cast<uint16_t>((index << 8u) | flags));
    for (uint32_t spin = 0; spin < 1000000u; ++spin) {
        const uint32_t status = read32(sdhci(0x30u));
        if (status & SdhciIntError) {
            write32(sdhci(0x30u), status);
            return RAD_STATUS_ERROR;
        }
        if (status & SdhciIntCmdComplete) {
            write32(sdhci(0x30u), SdhciIntCmdComplete);
            if (response) *response = sdhci_response32();
            return RAD_STATUS_OK;
        }
        if ((spin & 0xffu) == 0) udelay(1);
    }
    return RAD_STATUS_TIMEOUT;
}

rad_status_t sdhci_set_clock(void) {
    write16(sdhci(0x2cu), 0x0000u);
    udelay(1000);
    write16(sdhci(0x2cu), 0x0101u);
    if (!wait_mask(sdhci(0x2cu), 0x0002u, 0x0002u, 100000u)) return RAD_STATUS_TIMEOUT;
    write16(sdhci(0x2cu), 0x0107u);
    return RAD_STATUS_OK;
}

rad_status_t sdhci_reset(void) {
    write8(sdhci(0x2fu), 0x01u);
    for (uint32_t spin = 0; spin < 100000u; ++spin) {
        if ((read8(sdhci(0x2fu)) & 0x01u) == 0) break;
        if ((spin & 0xffu) == 0) udelay(1);
    }
    write8(sdhci(0x29u), 0x0fu);
    write32(sdhci(0x34u), 0xffffffffu);
    write32(sdhci(0x38u), 0x00000000u);
    write8(sdhci(0x2eu), 0x0eu);
    return sdhci_set_clock();
}

rad_status_t sdhci_read_single(uint64_t sector, void *buffer) {
    if (!buffer || !g_zynqmp.sd_ready) return RAD_STATUS_INVALID_ARGUMENT;
    write16(sdhci(0x04u), SdhciSectorSize);
    write16(sdhci(0x06u), 1u);
    write16(sdhci(0x0cu), 0x0010u);
    const uint32_t argument = g_zynqmp.sd_high_capacity ? static_cast<uint32_t>(sector) : static_cast<uint32_t>(sector * SdhciSectorSize);
    rad_status_t status = sdhci_send_cmd(17u, argument, SdhciCmdRespShort | SdhciCmdCrc | SdhciCmdIndex | SdhciCmdData, nullptr);
    if (status != RAD_STATUS_OK) return status;
    for (uint32_t spin = 0; spin < 1000000u; ++spin) {
        const uint32_t irq = read32(sdhci(0x30u));
        if (irq & SdhciIntError) {
            write32(sdhci(0x30u), irq);
            return RAD_STATUS_ERROR;
        }
        if ((irq & SdhciIntBufferReadReady) || (read32(sdhci(0x24u)) & SdhciPresentReadReady)) {
            auto *words = static_cast<uint32_t*>(buffer);
            for (uint32_t i = 0; i < SdhciSectorSize / sizeof(uint32_t); ++i) words[i] = read32(sdhci(0x20u));
            write32(sdhci(0x30u), SdhciIntBufferReadReady | SdhciIntTransferComplete);
            return RAD_STATUS_OK;
        }
        if ((spin & 0xffu) == 0) udelay(1);
    }
    return RAD_STATUS_TIMEOUT;
}

rad_status_t sdhci_write_single(uint64_t sector, const void *buffer) {
    if (!buffer || !g_zynqmp.sd_ready) return RAD_STATUS_INVALID_ARGUMENT;
    write16(sdhci(0x04u), SdhciSectorSize);
    write16(sdhci(0x06u), 1u);
    write16(sdhci(0x0cu), 0x0000u);
    const uint32_t argument = g_zynqmp.sd_high_capacity ? static_cast<uint32_t>(sector) : static_cast<uint32_t>(sector * SdhciSectorSize);
    rad_status_t status = sdhci_send_cmd(24u, argument, SdhciCmdRespShort | SdhciCmdCrc | SdhciCmdIndex | SdhciCmdData, nullptr);
    if (status != RAD_STATUS_OK) return status;
    for (uint32_t spin = 0; spin < 1000000u; ++spin) {
        const uint32_t irq = read32(sdhci(0x30u));
        if (irq & SdhciIntError) {
            write32(sdhci(0x30u), irq);
            return RAD_STATUS_ERROR;
        }
        if ((irq & SdhciIntBufferWriteReady) || (read32(sdhci(0x24u)) & SdhciPresentWriteReady)) {
            const auto *words = static_cast<const uint32_t*>(buffer);
            for (uint32_t i = 0; i < SdhciSectorSize / sizeof(uint32_t); ++i) write32(sdhci(0x20u), words[i]);
            if (!wait_mask(sdhci(0x30u), SdhciIntTransferComplete | SdhciIntError, SdhciIntTransferComplete, 1000000u)) {
                const uint32_t final_irq = read32(sdhci(0x30u));
                write32(sdhci(0x30u), final_irq);
                return (final_irq & SdhciIntError) ? RAD_STATUS_ERROR : RAD_STATUS_TIMEOUT;
            }
            write32(sdhci(0x30u), SdhciIntBufferWriteReady | SdhciIntTransferComplete);
            return RAD_STATUS_OK;
        }
        if ((spin & 0xffu) == 0) udelay(1);
    }
    return RAD_STATUS_TIMEOUT;
}

rad_status_t sdhci_transfer(uint64_t sector, uint32_t sector_count, void *buffer, int write) {
    if (!buffer || sector_count == 0) return RAD_STATUS_INVALID_ARGUMENT;
    auto *bytes = static_cast<uint8_t*>(buffer);
    for (uint32_t i = 0; i < sector_count; ++i) {
        rad_status_t status = write
            ? sdhci_write_single(sector + i, bytes + static_cast<size_t>(i) * SdhciSectorSize)
            : sdhci_read_single(sector + i, bytes + static_cast<size_t>(i) * SdhciSectorSize);
        if (status != RAD_STATUS_OK) return status;
    }
    return RAD_STATUS_OK;
}

rad_status_t sdhci_init_card(void) {
    rad_status_t status = sdhci_reset();
    if (status != RAD_STATUS_OK) return status;
    uint32_t response = 0;
    (void)sdhci_send_cmd(0u, 0u, SdhciCmdRespNone, nullptr);
    (void)sdhci_send_cmd(8u, 0x1aau, SdhciCmdRespShort | SdhciCmdCrc | SdhciCmdIndex, &response);
    for (uint32_t retry = 0; retry < 1000u; ++retry) {
        (void)sdhci_send_cmd(55u, 0u, SdhciCmdRespShort | SdhciCmdCrc | SdhciCmdIndex, nullptr);
        status = sdhci_send_cmd(41u, 0x40300000u, SdhciCmdRespShort, &response);
        if (status == RAD_STATUS_OK && (response & 0x80000000u)) break;
        udelay(1000);
    }
    if ((response & 0x80000000u) == 0) return RAD_STATUS_TIMEOUT;
    g_zynqmp.sd_high_capacity = (response & 0x40000000u) ? 1u : 0u;
    status = sdhci_send_cmd(2u, 0u, SdhciCmdRespLong | SdhciCmdCrc, nullptr);
    if (status != RAD_STATUS_OK) return status;
    status = sdhci_send_cmd(3u, 0u, SdhciCmdRespShort | SdhciCmdCrc | SdhciCmdIndex, &response);
    if (status != RAD_STATUS_OK) return status;
    g_zynqmp.sd_rca = response & 0xffff0000u;
    status = sdhci_send_cmd(7u, g_zynqmp.sd_rca, SdhciCmdRespShortBusy | SdhciCmdCrc | SdhciCmdIndex, nullptr);
    if (status != RAD_STATUS_OK) return status;
    (void)sdhci_send_cmd(16u, SdhciSectorSize, SdhciCmdRespShort | SdhciCmdCrc | SdhciCmdIndex, nullptr);
    g_zynqmp.sd_sector_count = 0x200000u;
    g_zynqmp.sd_ready = 1u;
    rad_debug_marker("RADIX_ZUBOARD_SD_OK");
    return RAD_STATUS_OK;
}

rad_status_t zynqmp_mmc_ioctl(void*, uint32_t request, void *argument) {
    if (request == RAD_DEVICE_IOCTL_BLOCK_INFO) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *info = static_cast<rad_block_info_t*>(argument);
        memset(info, 0, sizeof(*info));
        info->size = sizeof(*info);
        info->sector_size = SdhciSectorSize;
        info->sector_count = g_zynqmp.sd_sector_count ? g_zynqmp.sd_sector_count : 0x200000u;
        return g_zynqmp.sd_ready ? RAD_STATUS_OK : RAD_STATUS_NOT_INITIALIZED;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_READ || request == RAD_DEVICE_IOCTL_BLOCK_WRITE) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *block = static_cast<rad_block_request_t*>(argument);
        if (!block->buffer || block->sector_count == 0u) return RAD_STATUS_INVALID_ARGUMENT;
        return sdhci_transfer(block->sector, block->sector_count, block->buffer, request == RAD_DEVICE_IOCTL_BLOCK_WRITE);
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_FLUSH) return RAD_STATUS_OK;
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t zynqmp_partition_ioctl(void *context, uint32_t request, void *argument) {
    auto *partition = static_cast<PartitionDevice*>(context);
    if (!partition || !partition->ready) return RAD_STATUS_NOT_FOUND;
    if (request == RAD_DEVICE_IOCTL_BLOCK_INFO) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *info = static_cast<rad_block_info_t*>(argument);
        memset(info, 0, sizeof(*info));
        info->size = sizeof(*info);
        info->sector_size = SdhciSectorSize;
        info->sector_count = partition->sector_count;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_READ || request == RAD_DEVICE_IOCTL_BLOCK_WRITE) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *block = static_cast<rad_block_request_t*>(argument);
        if (!block->buffer || block->sector_count == 0u) return RAD_STATUS_INVALID_ARGUMENT;
        if (block->sector >= partition->sector_count || block->sector_count > partition->sector_count - block->sector) return RAD_STATUS_INVALID_ARGUMENT;
        return sdhci_transfer(partition->start_sector + block->sector, block->sector_count, block->buffer, request == RAD_DEVICE_IOCTL_BLOCK_WRITE);
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_FLUSH) return RAD_STATUS_OK;
    return RAD_STATUS_NOT_SUPPORTED;
}

uint32_t le32(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8u) | (static_cast<uint32_t>(p[2]) << 16u) | (static_cast<uint32_t>(p[3]) << 24u);
}

void register_mbr_partitions(void) {
    uint8_t mbr[SdhciSectorSize]{};
    if (sdhci_read_single(0, mbr) != RAD_STATUS_OK || mbr[510] != 0x55u || mbr[511] != 0xaau) return;
    for (uint32_t i = 0; i < 2u; ++i) {
        const uint8_t *entry = mbr + 446u + i * 16u;
        const uint32_t start = le32(entry + 8u);
        const uint32_t count = le32(entry + 12u);
        if (!start || !count) continue;
        g_partitions[i].start_sector = start;
        g_partitions[i].sector_count = count;
        g_partitions[i].ready = 1;
        rad_device_ops_t ops{};
        ops.context = &g_partitions[i];
        ops.ioctl = zynqmp_partition_ioctl;
        rad_block_device_register(g_partitions[i].name, &ops);
    }
    if (g_partitions[0].ready) rad_debug_marker("RADIX_ZUBOARD_PARTITION_BOOT_OK");
    if (g_partitions[1].ready) rad_debug_marker("RADIX_ZUBOARD_PARTITION_ROOT_OK");
}

rad_status_t zynqmp_serial_read(void*, void *buffer, size_t size, size_t *bytes_read) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    size_t count = 0;
    while (count < size && (read32(uart(0x2cu)) & CadenceUartSrRxEmpty) == 0) {
        static_cast<uint8_t*>(buffer)[count++] = static_cast<uint8_t>(read32(uart(0x30u)) & 0xffu);
    }
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t zynqmp_serial_write(void*, const void *buffer, size_t size, size_t *bytes_written) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    zynqmp_uart_init_once();
    const auto *bytes = static_cast<const uint8_t*>(buffer);
    for (size_t i = 0; i < size; ++i) {
        if (bytes[i] == '\n') {
            while (read32(uart(0x2cu)) & CadenceUartSrTxFull) {}
            write32(uart(0x30u), '\r');
        }
        while (read32(uart(0x2cu)) & CadenceUartSrTxFull) {}
        write32(uart(0x30u), bytes[i]);
    }
    if (bytes_written) *bytes_written = size;
    return RAD_STATUS_OK;
}

rad_status_t zynqmp_serial_ioctl(void*, uint32_t request, void *argument) {
    if (request != RAD_DEVICE_IOCTL_SERIAL_CONFIGURE) return RAD_STATUS_NOT_SUPPORTED;
    return argument ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
}

void register_serial_alias(const char *name, const rad_device_ops_t *ops) {
    rad_device_register(name, RAD_DEVICE_SERIAL, ops);
}
}

extern "C" void rad_zynqmp_bind_handoff(const rad_boot_handoff_t *handoff) {
    if (!handoff || handoff->magic != RAD_BOOT_HANDOFF_MAGIC) return;
    if (handoff->peripheral_base) g_zynqmp.uart_base = handoff->peripheral_base;
    if (handoff->local_interrupt_base) g_zynqmp.gic_distributor_base = handoff->local_interrupt_base;
    if (handoff->mailbox_base) g_zynqmp.gic_cpu_base = handoff->mailbox_base;
    if (handoff->arm_memory_size) {
        g_zynqmp.memory_base = handoff->arm_memory_base;
        g_zynqmp.memory_size = handoff->arm_memory_size;
    }
    if (handoff->core_count) g_zynqmp.core_count = handoff->core_count;
}

extern "C" uint64_t rad_hal_time_micros(void) {
    uint64_t counter = 0;
    uint64_t frequency = 0;
    asm volatile("mrs %0, cntpct_el0" : "=r"(counter));
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    if (!frequency) return 0;
    return (counter * 1000000ull) / frequency;
}

extern "C" void rad_hal_sleep_us(uint32_t microseconds) {
    const uint64_t start = rad_hal_time_micros();
    while (rad_hal_time_micros() - start < microseconds) {}
}

extern "C" uint32_t rad_hal_core_count(void) {
    return g_zynqmp.core_count;
}

extern "C" uint32_t rad_hal_current_core(void) {
    uint64_t mpidr = 0;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return static_cast<uint32_t>(mpidr & 0xffu);
}

extern "C" rad_status_t rad_hal_start_worker_core(uint32_t core, void (*entry)(uint32_t core)) {
    (void)core;
    (void)entry;
    return RAD_STATUS_NOT_SUPPORTED;
}

extern "C" void rad_hal_worker_wait(void) {
    asm volatile("wfe");
}

extern "C" void rad_hal_worker_wake(void) {
    asm volatile("sev");
}

extern "C" void rad_hal_console_write(const char *text) {
    if (!text) return;
    size_t written = 0;
    zynqmp_serial_write(nullptr, text, strlen(text), &written);
}

extern "C" void rad_hal_early_console_write(const char *text) {
    rad_hal_console_write(text);
}

extern "C" rad_status_t rad_hal_interrupts_enable(void) {
    asm volatile("msr daifclr, #2" ::: "memory");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_interrupts_disable(void) {
    asm volatile("msr daifset, #2" ::: "memory");
    return RAD_STATUS_OK;
}

extern "C" int rad_hal_interrupts_enabled(void) {
    uint64_t daif = 0;
    asm volatile("mrs %0, daif" : "=r"(daif));
    return (daif & (1u << 7u)) == 0;
}

extern "C" void rad_hal_cpu_idle(void) {
    asm volatile("wfi");
}

extern "C" void rad_hal_cpu_halt_forever(void) {
    for (;;) asm volatile("wfi");
}

extern "C" rad_status_t rad_hal_irq_enable(uint32_t irq) {
    (void)irq;
    return RAD_STATUS_NOT_SUPPORTED;
}

extern "C" rad_status_t rad_hal_irq_disable(uint32_t irq) {
    (void)irq;
    return RAD_STATUS_NOT_SUPPORTED;
}

extern "C" rad_status_t rad_hal_register_default_devices(void) {
    rad_device_ops_t serial{};
    serial.read = zynqmp_serial_read;
    serial.write = zynqmp_serial_write;
    serial.ioctl = zynqmp_serial_ioctl;
    register_serial_alias("/dev/console", &serial);
    register_serial_alias("/dev/serial0", &serial);
    register_serial_alias("/dev/uart0", &serial);
    register_serial_alias("/dev/ttyPS0", &serial);

    rad_device_ops_t block{};
    block.ioctl = zynqmp_mmc_ioctl;
    if (sdhci_init_card() == RAD_STATUS_OK && rad_block_device_register("/dev/mmcblk0", &block) == RAD_STATUS_OK) {
        rad_debug_marker("RADIX_ZUBOARD_MMCBLK0_OK");
        register_mbr_partitions();
    } else {
        rad_debug_marker("RADIX_ZUBOARD_SD_FAIL");
    }

    rad_terminal_attach_device("/dev/console");
    rad_terminal_attach_tty("/dev/console", "/dev/tty0");
    rad_debug_marker("RADIX_ZYNQMP_HAL_OK");
    rad_debug_marker("RADIX_ZUBOARD_UART_OK");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_mount_sd(const rad_sd_config_t *config) {
    (void)config;
    return RAD_STATUS_NOT_SUPPORTED;
}
