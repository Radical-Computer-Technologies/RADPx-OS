#include <radkernel/radkernel.h>
#include <radboot.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void rad_hal_sleep_us(uint32_t microseconds);
extern "C" rad_status_t rad_zynqmp_gem_init(void);

namespace {
constexpr uintptr_t DefaultUartBase = 0xff000000u;
#ifndef RAD_ZYNQMP_SDHCI_BASE
#define RAD_ZYNQMP_SDHCI_BASE 0xff170000u
#endif
constexpr uintptr_t DefaultSdhciBase = RAD_ZYNQMP_SDHCI_BASE;
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
    rad_debug_marker("RAD_ZUBOARD_SD_OK");
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
        if (block->size < sizeof(*block)) return RAD_STATUS_INVALID_ARGUMENT;
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
        if (block->size < sizeof(*block)) return RAD_STATUS_INVALID_ARGUMENT;
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
    if (g_partitions[0].ready) rad_debug_marker("RAD_ZUBOARD_PARTITION_BOOT_OK");
    if (g_partitions[1].ready) rad_debug_marker("RAD_ZUBOARD_PARTITION_ROOT_OK");
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

// Serialize UART output against both the other core and IRQ-context prints on
// the same core (the timer tick prints markers): mask IRQs locally, then take
// a cross-core test-and-set lock. Without this, marker lines interleave
// mid-text and the ordered smoke gate cannot match them.
volatile int g_uart_lock = 0;

rad_status_t zynqmp_serial_write(void*, const void *buffer, size_t size, size_t *bytes_written) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    zynqmp_uart_init_once();
    // Serialize output against the other core and same-core IRQ prints (the timer
    // handler emits markers): mask IRQs locally, then take a cross-core lock.
#if defined(__aarch64__)
    uint64_t saved_daif = 0;
    asm volatile("mrs %0, daif\n\tmsr daifset, #2" : "=r"(saved_daif):: "memory");
#endif
    while (__atomic_test_and_set(const_cast<int*>(&g_uart_lock), __ATOMIC_ACQUIRE)) {
    }
    const auto *bytes = static_cast<const uint8_t*>(buffer);
    for (size_t i = 0; i < size; ++i) {
        if (bytes[i] == '\n') {
            while (read32(uart(0x2cu)) & CadenceUartSrTxFull) {}
            write32(uart(0x30u), '\r');
        }
        while (read32(uart(0x2cu)) & CadenceUartSrTxFull) {}
        write32(uart(0x30u), bytes[i]);
    }
    __atomic_clear(const_cast<int*>(&g_uart_lock), __ATOMIC_RELEASE);
#if defined(__aarch64__)
    asm volatile("msr daif, %0" :: "r"(saved_daif) : "memory");
#endif
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

// --- GICv2 + EL1 physical generic timer (CNTP, PPI INTID 30) ---------------
namespace {

constexpr uint32_t GicIntidTimer = 30u;
constexpr uint32_t GicIntidSpurious = 1020u;
constexpr uint32_t TimerHz = 250u; // 4 ms tick: TCG-friendly

uintptr_t gicd(uintptr_t offset) { return g_zynqmp.gic_distributor_base + offset; }
uintptr_t gicc(uintptr_t offset) { return g_zynqmp.gic_cpu_base + offset; }

uint64_t g_timer_interval_ticks = 0;
volatile uint32_t g_timer_irq_seen = 0;
volatile uint32_t g_ap_timer_irq_seen = 0;

void zynqmp_timer_arm(void) {
#if defined(__aarch64__)
    if (!g_timer_interval_ticks) {
        uint64_t frequency = 0;
        asm volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
        g_timer_interval_ticks = frequency / TimerHz;
    }
    asm volatile("msr cntp_tval_el0, %0" :: "r"(g_timer_interval_ticks));
    asm volatile("msr cntp_ctl_el0, %0" :: "r"(1ull)); // ENABLE=1, IMASK=0
#endif
}

void zynqmp_timer_irq(uint32_t, void *) {
    zynqmp_timer_arm(); // re-arm first: deasserts the level-triggered line
    rad_timer_tick(1000000u / TimerHz);
    if (!g_timer_irq_seen) {
        g_timer_irq_seen = 1;
        rad_debug_marker("RAD_TIMER_IRQ_OK");
    }
    uint32_t core = 0;
#if defined(__aarch64__)
    uint64_t mpidr = 0;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    core = static_cast<uint32_t>(mpidr & 0xffu);
#endif
    if (core > 0 && !g_ap_timer_irq_seen) {
        g_ap_timer_irq_seen = 1;
        rad_debug_marker("RAD_AP_TIMER_IRQ_OK");
    }
}

// Banked per-core CPU-interface init (PPIs/SGIs are per-core in the GIC).
void zynqmp_gic_cpu_init(void) {
    write32(gicc(0x04u), 0xf8u); // GICC_PMR: allow all configured priorities
    write32(gicc(0x08u), 0x00u); // GICC_BPR
    write32(gicc(0x00u), 0x01u); // GICC_CTLR: enable group 0/1 signalling
}

} // namespace

extern "C" rad_status_t rad_hal_irq_enable(uint32_t irq) {
    if (irq >= 1020u) return RAD_STATUS_INVALID_ARGUMENT;
    // Banked for INTID < 32: must execute on the core that owns the PPI.
    write32(gicd(0x100u + 4u * (irq / 32u)), 1u << (irq % 32u));
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_irq_disable(uint32_t irq) {
    if (irq >= 1020u) return RAD_STATUS_INVALID_ARGUMENT;
    write32(gicd(0x180u + 4u * (irq / 32u)), 1u << (irq % 32u));
    return RAD_STATUS_OK;
}

// Configure a shared peripheral interrupt (INTID >= 32) in the GICv2 distributor:
// priority, CPU target mask, and level-sensitive trigger. SGIs/PPIs (INTID < 32)
// are banked and configured with the timer; this is for SPIs such as the Cadence
// GEM (INTID 57 on QEMU's xlnx-zynqmp). rad_irq_enable() still writes ISENABLER.
extern "C" rad_status_t rad_zynqmp_gic_configure_spi(uint32_t intid, uint8_t priority, uint8_t cpu_target) {
    if (intid < 32u || intid >= 1020u) return RAD_STATUS_INVALID_ARGUMENT;
    write8(gicd(0x400u + intid), priority);   // GICD_IPRIORITYR[intid]
    write8(gicd(0x800u + intid), cpu_target);  // GICD_ITARGETSR[intid]  (CPU mask)
    // GICD_ICFGR: 2 bits per INTID; clearing the high bit selects level-sensitive.
    const uintptr_t icfgr = gicd(0xc00u + 4u * (intid / 16u));
    const uint32_t shift = 2u * (intid % 16u);
    write32(icfgr, read32(icfgr) & ~(0x2u << shift));
    return RAD_STATUS_OK;
}

// Strong scheduler hooks: this target preempts (bcm283x must not claim this,
// so these live in the zynqmp HAL rather than the shared a53 platform file).
extern "C" int rad_arch_preemption_supported(void) {
    return 1;
}

extern "C" const char *rad_arch_scheduler_name(void) {
    return "a53-gicv2-preemptive";
}

extern "C" void rad_arch_scheduler_tick(uint32_t core) {
    (void)core;
    rad_scheduler_yield_from_irq();
}

extern "C" void rad_a53_boot_stall_check(const void *frame);

// IRQ dispatch entered from boot.S's rad_a53_irq_common with the saved
// exception frame; the frame itself is only needed by the asm resume path.
extern "C" void rad_zynqmp_irq_dispatch_frame(void *frame) {
    // Saved SPSR_EL1 lives at offset 264 of the 288-byte boot.S trap frame.
    // Preempt ONLY when the IRQ interrupted EL0 (SPSR.M[3:0] == 0), mirroring
    // x86's ring-3 rule: a tick landing inside the scheduler's own dispatch
    // window (current-task id set, arch context not yet initialized/switched)
    // makes rad_task_yield save into an uninitialized task context and corrupts
    // the dispatch -- observed as vanishing AP markers and stuck stress tasks.
    uint64_t interrupted_spsr = 0x5u; // default: treat as EL1 (no preempt)
    if (frame) interrupted_spsr = *reinterpret_cast<const uint64_t *>(static_cast<const uint8_t *>(frame) + 264u);
    for (;;) {
        const uint32_t iar = read32(gicc(0x0cu));
        const uint32_t intid = iar & 0x3ffu;
        if (intid >= GicIntidSpurious) break;
        rad_irq_dispatch(intid);
        write32(gicc(0x10u), iar); // EOIR before any context switch away
        if (intid == GicIntidTimer && (interrupted_spsr & 0xfu) == 0u) {
            rad_arch_scheduler_tick(rad_hal_current_core());
        }
    }
    // #17: boot-session stall watchdog (dumps + halts if the first user EL0 entry
    // hung). Cheap deadline check; no-op once the boot reaches zombie-reap.
    if (frame) rad_a53_boot_stall_check(frame);
}

// Distributor + core-0 CPU interface + timer registration. Called by the
// board entry after exception vectors and the MMU are live.
extern "C" rad_status_t rad_zynqmp_preempt_init(void) {
    write32(gicd(0x000u), 0u);           // GICD_CTLR off during setup
    write32(gicd(0x180u), 0xffffffffu);  // mask banked SGI/PPI
    write8(gicd(0x400u + GicIntidTimer), 0x80u); // IPRIORITYR byte for INTID 30
    write32(gicd(0x000u), 1u);           // distributor on
    zynqmp_gic_cpu_init();
    const rad_status_t registered = rad_irq_register(GicIntidTimer, "cntp-timer", zynqmp_timer_irq, nullptr);
    if (registered != RAD_STATUS_OK) return registered;
    (void)rad_irq_enable(GicIntidTimer);
    zynqmp_timer_arm();
    rad_hal_interrupts_enable();
    // Bounded wait for the first tick so RAD_TIMER_IRQ_OK has a stable
    // position in the serial log (three periods of margin).
    const uint64_t deadline = rad_hal_time_micros() + 3u * (1000000u / TimerHz);
    while (!g_timer_irq_seen && rad_hal_time_micros() < deadline) {
    }
    return g_timer_irq_seen ? RAD_STATUS_OK : RAD_STATUS_TIMEOUT;
}

// --- PSCI SMP bring-up ------------------------------------------------------
namespace {

volatile uint32_t g_secondary_up = 0;
volatile uint32_t g_secondary_release = 0;
volatile uint32_t g_secondary_ready = 0;
void (*g_worker_entry)(uint32_t) = nullptr;

uint64_t zynqmp_smc(uint64_t fn, uint64_t a1, uint64_t a2, uint64_t a3) {
#if defined(__aarch64__)
    register uint64_t x0 asm("x0") = fn;
    register uint64_t x1 asm("x1") = a1;
    register uint64_t x2 asm("x2") = a2;
    register uint64_t x3 asm("x3") = a3;
    asm volatile("smc #0"
        : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
        :
        : "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "memory");
    return x0;
#else
    (void)fn; (void)a1; (void)a2; (void)a3;
    return ~0ull;
#endif
}

// The secondary polls these flags with the MMU (and therefore caches) off;
// core 0 runs cached, so shared lines must be cleaned/invalidated to PoC.
void flush_flag(const volatile uint32_t *flag) {
#if defined(__aarch64__)
    asm volatile("dc civac, %0\n\tdsb sy" :: "r"(flag) : "memory");
#else
    (void)flag;
#endif
}

} // namespace

extern "C" void rad_a53_secondary_start(void);
extern "C" rad_status_t rad_a53_mmu_enable_current_core(void);

extern "C" void rad_zynqmp_secondary_main(uint32_t core) {
    __atomic_store_n(&g_secondary_up, 1u, __ATOMIC_RELEASE);
#if defined(__aarch64__)
    asm volatile("dsb sy\n\tsev" ::: "memory");
    while (!__atomic_load_n(&g_secondary_release, __ATOMIC_ACQUIRE)) {
        asm volatile("wfe");
    }
#endif
    rad_a53_mmu_enable_current_core();
    zynqmp_gic_cpu_init();
    (void)rad_hal_irq_enable(GicIntidTimer); // banked enable on this core
    zynqmp_timer_arm();
#if defined(__aarch64__)
    asm volatile("msr daifclr, #2" ::: "memory");
#endif
    const uint64_t deadline = rad_hal_time_micros() + 3u * (1000000u / TimerHz);
    while (!g_ap_timer_irq_seen && rad_hal_time_micros() < deadline) {
    }
    rad_debug_marker("RAD_AP_START_OK");
    __atomic_store_n(&g_secondary_ready, 1u, __ATOMIC_RELEASE);
    if (g_worker_entry) g_worker_entry(core);
    rad_hal_cpu_halt_forever();
}

extern "C" rad_status_t rad_hal_start_worker_core(uint32_t core, void (*entry)(uint32_t core)) {
    // core_count reflects the actual topology: 2 on hardware, the real -smp
    // under QEMU (passed via the RSMP scratch word) -- so this never issues
    // CPU_ON for an absent core (which destabilizes core 0 under -smp 1).
    if (core == 0u || core >= g_zynqmp.core_count) return RAD_STATUS_INVALID_ARGUMENT;
    g_worker_entry = entry;
    flush_flag(reinterpret_cast<volatile uint32_t*>(&g_worker_entry));
    const uint64_t result = zynqmp_smc(0xC4000003u, core,
        reinterpret_cast<uint64_t>(&rad_a53_secondary_start), core);
    if (result != 0u) {
        rad_debug_marker("RAD_ZUBOARD_CPU_ON_FAIL");
        return RAD_STATUS_NOT_SUPPORTED;
    }
    // CPU_ON success is authoritative: PSCI accepted the wake, so the core WILL
    // run. Do not wait for a check-in here -- the generic counter follows host
    // time under TCG (no -icount), so any fixed window races host load (the
    // secondary's first schedule was observed missing even 2 s occasionally).
    // rad_zynqmp_smp_release() below is the single synchronization point.
    return RAD_STATUS_OK;
}

// Called by the board entry once vectors/MMU/GIC exist. Sets the release flag
// UNCONDITIONALLY: under QEMU -smp 1 a phantom cpu1 may start late (the CPU
// object exists even without a vCPU thread) and would otherwise spin forever in
// the pre-release wait, starving core 0 via single-thread TCG round-robin. With
// the flag always set, any secondary that ever runs proceeds into worker_loop
// and idles harmlessly if there is no work. Returns OK only if a secondary
// actually checked in and reached readiness (a genuine second core).
extern "C" rad_status_t rad_zynqmp_smp_release(void) {
    __atomic_store_n(&g_secondary_release, 1u, __ATOMIC_RELEASE);
    flush_flag(&g_secondary_release);
#if defined(__aarch64__)
    asm volatile("sev" ::: "memory");
#endif
    if (!g_worker_entry) return RAD_STATUS_NOT_INITIALIZED; // CPU_ON never issued
    // Single synchronization point for secondary bring-up; exits the moment the
    // core reaches readiness. The generic counter follows HOST wall-clock under
    // TCG (no -icount) while guest progress depends on host CPU share, so under
    // host load N virtual seconds can pass with almost no guest instructions
    // executed -- no in-guest deadline is sound there. Size the window near the
    // SMP smoke's 90 s wall budget; on hardware a live core arrives in
    // microseconds, so expiry means it is genuinely dead.
    const uint64_t deadline = rad_hal_time_micros() + 45000000u;
    while (!__atomic_load_n(&g_secondary_ready, __ATOMIC_ACQUIRE)) {
        flush_flag(&g_secondary_ready);
        if (rad_hal_time_micros() >= deadline) {
            rad_debug_marker("RAD_ZUBOARD_SMP_READY_TIMEOUT");
            return RAD_STATUS_TIMEOUT;
        }
    }
    return RAD_STATUS_OK;
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
        rad_debug_marker("RAD_ZUBOARD_MMCBLK0_OK");
        register_mbr_partitions();
    } else {
        rad_debug_marker("RAD_ZUBOARD_SD_FAIL");
    }

    // Cadence GEM (Ethernet) -> "/dev/net0" for the portable net stack. Best
    // effort: a probe failure must not block console/storage bring-up.
    rad_zynqmp_gem_init();

    // The core runtime already registered "/dev/console" as a TTY device, so
    // the serial alias above silently lost that name (ALREADY_EXISTS) and the
    // is-serial check in the attach calls would fail against it. Attach through
    // "/dev/serial0", which this HAL actually owns, so tty0 output reaches the
    // Cadence UART and the poll loop pumps typed input back into tty0.
    rad_terminal_attach_device("/dev/serial0");
    rad_terminal_attach_tty("/dev/serial0", "/dev/tty0");
    rad_debug_marker("RAD_ZYNQMP_HAL_OK");
    rad_debug_marker("RAD_ZUBOARD_UART_OK");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_mount_sd(const rad_sd_config_t *config) {
    (void)config;
    return RAD_STATUS_NOT_SUPPORTED;
}
