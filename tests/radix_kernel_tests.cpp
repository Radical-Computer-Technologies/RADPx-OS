#include <radixkernel/rad_audio.h>
#include <radixkernel/rad_block.h>
#include <radixkernel/rad_cpp.h>
#include <radixkernel/rad_device.h>
#include <radixkernel/rad_display.h>
#include <radixkernel/rad_dma.h>
#include <radixkernel/rad_event.h>
#include <radixkernel/rad_i2c.h>
#include <radixkernel/rad_input.h>
#include <radixkernel/rad_irq.h>
#include <radixkernel/rad_memory.h>
#include <radixkernel/rad_module.h>
#include <radixkernel/rad_mutex.h>
#include <radixkernel/rad_posix.h>
#include <radixkernel/rad_service.h>
#include <radixkernel/rad_spi.h>
#include <radixkernel/rad_task.h>
#include <radixkernel/rad_terminal.h>
#include <radixkernel/rad_time.h>
#include <radixkernel/rad_tty.h>
#include <radixkernel/rad_pty.h>
#include <radixkernel/rad_vfs.h>
#include <radboot.h>
#include "../RADixKernel/platforms/a53/radixkernel_a53.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[RADixKernelTests] FAIL: " << message << '\n';
        return false;
    }
    return true;
}

void terminalWriter(const char* text, void* context) {
    auto* out = static_cast<std::string*>(context);
    if (text) *out += text;
}

void ttyOutput(const void* data, size_t size, void* context) {
    auto* out = static_cast<std::string*>(context);
    if (out && data) out->append(static_cast<const char*>(data), size);
}

struct TaskContext {
    rad_event_t event = nullptr;
    std::atomic<int> ran{0};
    std::atomic<uint64_t> taskId{0};
    std::atomic<int> core{0};
};

struct FakeForkContext {
    int calls = 0;
};

int32_t fakeForkFromFrame(void* context, void*) {
    auto* fake = static_cast<FakeForkContext*>(context);
    if (fake) ++fake->calls;
    const int32_t parent = rad_process_current_pid();
    const int32_t child = rad_process_create("/bin/fake-child", parent);
    if (child < 0) return child;
    const rad_status_t cloned = rad_process_clone_fds(parent, child);
    return cloned == RAD_STATUS_OK ? child : static_cast<int32_t>(cloned);
}

void testTask(void* context) {
    auto* task = static_cast<TaskContext*>(context);
    task->taskId = rad_task_current_id();
    task->core = rad_task_current_core();
    rad_task_sleep_ms(1);
    task->ran = 1;
    rad_event_signal(task->event);
}

struct FakeTimer {
    int starts = 0;
    int oneshots = 0;
    int cancels = 0;
    uint32_t lastFrequency = 0;
    uint64_t lastDelay = 0;
};

void testWork(void* context) {
    auto* ran = static_cast<std::atomic<int>*>(context);
    if (ran) ++(*ran);
}

rad_status_t fakeTimerStart(void* context, uint32_t frequencyHz) {
    auto* timer = static_cast<FakeTimer*>(context);
    if (!timer) return RAD_STATUS_INVALID_ARGUMENT;
    ++timer->starts;
    timer->lastFrequency = frequencyHz;
    return RAD_STATUS_OK;
}

rad_status_t fakeTimerOneshot(void* context, uint64_t delayMicros) {
    auto* timer = static_cast<FakeTimer*>(context);
    if (!timer) return RAD_STATUS_INVALID_ARGUMENT;
    ++timer->oneshots;
    timer->lastDelay = delayMicros;
    return RAD_STATUS_OK;
}

rad_status_t fakeTimerCancel(void* context) {
    auto* timer = static_cast<FakeTimer*>(context);
    if (!timer) return RAD_STATUS_INVALID_ARGUMENT;
    ++timer->cancels;
    return RAD_STATUS_OK;
}

struct FakeBus {
    std::vector<uint8_t> lastWrite;
    uint8_t response = 0x5a;
    int probes = 0;
    int removes = 0;
    int irqProbeCount = 0;
    uint32_t irqLine = 0;
    uint32_t irqNumber = 0;
};

struct FakeSerial {
    std::string input;
    std::string output;
};

struct FakeSpi {
    std::vector<uint8_t> lastTx;
    int pioTransfers = 0;
    int dmaTransfers = 0;
    int probes = 0;
    int removes = 0;
    int irqProbeCount = 0;
    uint32_t irqLine = 0;
    uint32_t irqNumber = 0;
};

struct FakeDma {
    int requests = 0;
    int releases = 0;
    int submits = 0;
    int waits = 0;
    int cancels = 0;
    bool timeoutNext = false;
};

struct FakeInput {
    rad_input_event_t event{};
    bool consumed = false;
};

struct FakeBlock {
    rad_block_info_t info{};
    uint64_t lastSector = 0;
    uint32_t lastSectorCount = 0;
    std::vector<uint8_t> lastWrite;
    int flushes = 0;
};

struct FakeNet {
    rad_net_link_info_t info{};
    std::vector<uint8_t> lastPacket;
    std::vector<uint8_t> rxPacket;
    int polls = 0;
};

struct FakeFramebuffer {
    int flushes = 0;
    int setModes = 0;
    int blanks = 0;
    int blanked = 0;
    uint64_t vsync = 7;
};

struct FakeModule {
    int inits = 0;
    int exits = 0;
    rad_status_t initStatus = RAD_STATUS_OK;
};

struct FakeService {
    int starts = 0;
    int stops = 0;
    int polls = 0;
    std::string backend;
    std::string display;
    std::string keyboard;
    std::string pointer;
    std::string terminal;
    int order = 0;
};

struct FakeIrq {
    int calls = 0;
    uint32_t lastIrq = 0;
};

void fakeIrqHandler(uint32_t irq, void* context) {
    auto* fake = static_cast<FakeIrq*>(context);
    if (!fake) return;
    ++fake->calls;
    fake->lastIrq = irq;
}

rad_status_t fakeModuleInit(void* context) {
    auto* module = static_cast<FakeModule*>(context);
    if (!module) return RAD_STATUS_INVALID_ARGUMENT;
    ++module->inits;
    return module->initStatus;
}

void fakeModuleExit(void* context) {
    auto* module = static_cast<FakeModule*>(context);
    if (module) ++module->exits;
}

rad_status_t fakeServiceStart(void* context, const rad_service_config_t* config) {
    auto* service = static_cast<FakeService*>(context);
    if (!service || !config) return RAD_STATUS_INVALID_ARGUMENT;
    ++service->starts;
    service->backend = config->backend ? config->backend : "";
    service->display = config->display ? config->display : "";
    service->keyboard = config->keyboard ? config->keyboard : "";
    service->pointer = config->pointer ? config->pointer : "";
    service->terminal = config->terminal ? config->terminal : "";
    service->order = config->order;
    return RAD_STATUS_OK;
}

void fakeServiceStop(void* context) {
    auto* service = static_cast<FakeService*>(context);
    if (service) ++service->stops;
}

rad_status_t fakeServicePoll(void* context) {
    auto* service = static_cast<FakeService*>(context);
    if (!service) return RAD_STATUS_INVALID_ARGUMENT;
    ++service->polls;
    return RAD_STATUS_OK;
}

rad_status_t fakeWrite(void* context, const void* buffer, size_t size, size_t* bytesWritten) {
    auto* bus = static_cast<FakeBus*>(context);
    const auto* bytes = static_cast<const uint8_t*>(buffer);
    bus->lastWrite.assign(bytes, bytes + size);
    if (bytesWritten) *bytesWritten = size;
    return RAD_STATUS_OK;
}

rad_status_t fakeRead(void* context, void* buffer, size_t size, size_t* bytesRead) {
    auto* bus = static_cast<FakeBus*>(context);
    std::memset(buffer, bus->response, size);
    if (bytesRead) *bytesRead = size;
    return RAD_STATUS_OK;
}

rad_status_t fakeIoctl(void* context, uint32_t request, void* argument) {
    auto* bus = static_cast<FakeBus*>(context);
    if (request != RAD_DEVICE_IOCTL_I2C_TRANSFER) return RAD_STATUS_NOT_SUPPORTED;
    auto* transfer = static_cast<rad_i2c_transfer_t*>(argument);
    if (!transfer) return RAD_STATUS_INVALID_ARGUMENT;
    if (transfer->write_data && transfer->write_size) {
        bus->lastWrite.assign(transfer->write_data, transfer->write_data + transfer->write_size);
    }
    if (transfer->read_data && transfer->read_size) {
        std::memset(transfer->read_data, bus->response, transfer->read_size);
    }
    return RAD_STATUS_OK;
}

rad_status_t fakeSpiIoctl(void* context, uint32_t request, void* argument) {
    auto* spi = static_cast<FakeSpi*>(context);
    if (request != RAD_DEVICE_IOCTL_SPI_TRANSFER) return RAD_STATUS_NOT_SUPPORTED;
    auto* transfer = static_cast<rad_spi_transfer_t*>(argument);
    if (!spi || !transfer) return RAD_STATUS_INVALID_ARGUMENT;
    if (transfer->tx_data && transfer->size) spi->lastTx.assign(transfer->tx_data, transfer->tx_data + transfer->size);
    if (transfer->rx_data && transfer->size) std::memset(transfer->rx_data, 0xa5, transfer->size);
    ++spi->pioTransfers;
    return RAD_STATUS_OK;
}

rad_status_t fakeSerialRead(void* context, void* buffer, size_t size, size_t* bytesRead) {
    auto* serial = static_cast<FakeSerial*>(context);
    if (!serial || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const size_t count = std::min(size, serial->input.size());
    std::memcpy(buffer, serial->input.data(), count);
    serial->input.erase(0, count);
    if (bytesRead) *bytesRead = count;
    return RAD_STATUS_OK;
}

rad_status_t fakeSerialWrite(void* context, const void* buffer, size_t size, size_t* bytesWritten) {
    auto* serial = static_cast<FakeSerial*>(context);
    if (!serial || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    serial->output.append(static_cast<const char*>(buffer), size);
    if (bytesWritten) *bytesWritten = size;
    return RAD_STATUS_OK;
}

rad_status_t fakeSerialIoctl(void*, uint32_t request, void* argument) {
    if (request != RAD_DEVICE_IOCTL_SERIAL_CONFIGURE) return RAD_STATUS_NOT_SUPPORTED;
    return argument ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
}

rad_status_t fakeInputRead(void* context, void* buffer, size_t size, size_t* bytesRead) {
    auto* input = static_cast<FakeInput*>(context);
    if (!input || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    if (input->consumed || size < sizeof(rad_input_event_t)) {
        if (bytesRead) *bytesRead = 0;
        return RAD_STATUS_OK;
    }
    std::memcpy(buffer, &input->event, sizeof(input->event));
    input->consumed = true;
    if (bytesRead) *bytesRead = sizeof(input->event);
    return RAD_STATUS_OK;
}

rad_status_t fakeBlockIoctl(void* context, uint32_t request, void* argument) {
    auto* block = static_cast<FakeBlock*>(context);
    if (!block) return RAD_STATUS_INVALID_ARGUMENT;
    if (request == RAD_DEVICE_IOCTL_BLOCK_INFO) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        *static_cast<rad_block_info_t*>(argument) = block->info;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_READ) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto* read = static_cast<rad_block_request_t*>(argument);
        if (!read || !read->buffer) return RAD_STATUS_INVALID_ARGUMENT;
        block->lastSector = read->sector;
        block->lastSectorCount = read->sector_count;
        std::memset(read->buffer, 0x5c, static_cast<size_t>(read->sector_count) * block->info.sector_size);
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_WRITE) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto* write = static_cast<rad_block_request_t*>(argument);
        if (!write || !write->buffer) return RAD_STATUS_INVALID_ARGUMENT;
        block->lastSector = write->sector;
        block->lastSectorCount = write->sector_count;
        const size_t bytes = static_cast<size_t>(write->sector_count) * block->info.sector_size;
        const auto* data = static_cast<const uint8_t*>(write->buffer);
        block->lastWrite.assign(data, data + bytes);
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_FLUSH) {
        ++block->flushes;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t fakeNetIoctl(void* context, uint32_t request, void* argument) {
    auto* net = static_cast<FakeNet*>(context);
    if (!net) return RAD_STATUS_INVALID_ARGUMENT;
    if (request == RAD_DEVICE_IOCTL_NET_LINK_INFO) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        *static_cast<rad_net_link_info_t*>(argument) = net->info;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_NET_SEND) {
        auto* packet = static_cast<rad_net_packet_t*>(argument);
        if (!packet || packet->size < sizeof(*packet) || !packet->data || !packet->length) return RAD_STATUS_INVALID_ARGUMENT;
        const auto* bytes = static_cast<const uint8_t*>(packet->data);
        net->lastPacket.assign(bytes, bytes + packet->length);
        ++net->info.tx_packets;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_NET_POLL) {
        ++net->polls;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_NET_RECV) {
        auto* packet = static_cast<rad_net_packet_t*>(argument);
        if (!packet || packet->size < sizeof(*packet) || !packet->data || !packet->length) return RAD_STATUS_INVALID_ARGUMENT;
        if (net->rxPacket.empty()) return RAD_STATUS_NOT_FOUND;
        const size_t count = std::min(packet->length, net->rxPacket.size());
        std::memcpy(packet->data, net->rxPacket.data(), count);
        packet->length = count;
        net->rxPacket.clear();
        ++net->info.rx_packets;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

bool testKernelLifecycle() {
    rad_kernel_config_t config{};
    config.backend_name = "linux_sim";
    return expect(rad_kernel_init(&config) == RAD_STATUS_OK, "kernel should initialize")
        && expect(rad_kernel_is_initialized() == 1, "kernel should report initialized")
        && expect(std::string(rad_kernel_backend_name()) == "linux_sim", "backend should be linux_sim")
        && expect(std::string(rad_kernel_version_string()) == "0.1.4", "version should be 0.1.4")
        && expect(rad_terminal_command_count() >= 10, "built-in terminal commands should register")
        && expect(rad_kernel_is_shutdown_requested() == 0, "kernel should not start in shutdown state");
}

bool testBootInfoAndRunLoop() {
    rad_boot_info_t boot{};
    bool ok = expect(rad_boot_info_get(&boot) == RAD_STATUS_OK, "default boot info should exist");
    ok &= expect(std::string(boot.backend) == "linux_sim", "boot info backend should match");
    ok &= expect(std::string(boot.board) == "host", "boot info board should default to host");

    rad_boot_info_t updated{};
    updated.size = sizeof(updated);
    updated.version = 1;
    std::snprintf(updated.backend, sizeof(updated.backend), "%s", "linux_sim");
    std::snprintf(updated.board, sizeof(updated.board), "%s", "unit-test");
    std::snprintf(updated.args[0].key, sizeof(updated.args[0].key), "%s", "mode");
    std::snprintf(updated.args[0].value, sizeof(updated.args[0].value), "%s", "smoke");
    updated.arg_count = 1;
    ok &= expect(rad_boot_info_set(&updated) == RAD_STATUS_OK, "boot info should update");
    ok &= expect(std::string(rad_boot_arg_get("mode")) == "smoke", "boot arg lookup should work");
    ok &= expect(rad_kernel_poll() == RAD_STATUS_OK, "kernel poll should run");
    rad_kernel_request_shutdown();
    ok &= expect(rad_kernel_is_shutdown_requested() == 1, "shutdown request should latch");
    rad_kernel_shutdown();
    rad_kernel_config_t config{};
    config.backend_name = "linux_sim";
    ok &= expect(rad_kernel_init(&config) == RAD_STATUS_OK, "kernel should reinitialize after shutdown request");
    return ok;
}

bool testPiHandoffAbi() {
    rad_boot_handoff_t handoff{};
    radboot_prepare_pi_handoff(&handoff, "RADIXKRN.IMG", 0x80000u, 0x120000u, 0x80000u);
    bool ok = expect(handoff.magic == RAD_BOOT_HANDOFF_MAGIC, "Pi handoff magic should be set");
    ok &= expect(handoff.version == RAD_BOOT_HANDOFF_VERSION, "Pi handoff version should be set");
    ok &= expect(handoff.size == sizeof(rad_boot_handoff_t), "Pi handoff size should match ABI");
    ok &= expect(std::string(handoff.boot.backend) == "bcm283x_pi", "Pi handoff should select bcm283x backend");
    ok &= expect(std::string(handoff.boot.board) == "pi-zero-2w", "Pi handoff should name board");
    ok &= expect(handoff.peripheral_base == 0x3f000000u && handoff.mailbox_base == 0x3f00b880u,
        "Pi handoff should expose BCM283x peripheral and mailbox bases");
    ok &= expect(radboot_validate_handoff(&handoff) == RAD_STATUS_INVALID_ARGUMENT,
        "Pi handoff should require explicit clean CPU state flags");
    handoff.flags = RAD_BOOT_HANDOFF_FLAG_SECONDARIES_PARKED
        | RAD_BOOT_HANDOFF_FLAG_MMU_DISABLED
        | RAD_BOOT_HANDOFF_FLAG_DCACHE_DISABLED
        | RAD_BOOT_HANDOFF_FLAG_ICACHE_INVALIDATED
        | RAD_BOOT_HANDOFF_FLAG_TLB_INVALIDATED
        | RAD_BOOT_HANDOFF_FLAG_INTERRUPTS_MASKED;
    ok &= expect(radboot_validate_handoff(&handoff) == RAD_STATUS_OK, "valid Pi handoff should validate");
    handoff.magic = 0;
    ok &= expect(radboot_validate_handoff(&handoff) == RAD_STATUS_INVALID_ARGUMENT,
        "invalid Pi handoff magic should fail validation");
    return ok;
}

bool testTasksAndEvents() {
    rad_event_t event = nullptr;
    TaskContext context;
    bool ok = expect(rad_event_create(&event, 0) == RAD_STATUS_OK, "event should create");
    context.event = event;
    rad_task_t task = nullptr;
    rad_task_config_t taskConfig{};
    taskConfig.size = sizeof(taskConfig);
    taskConfig.name = "event-task";
    taskConfig.target_core = RAD_TASK_CORE_SERVICE;
    taskConfig.priority = 3;
    taskConfig.stack_size = 4096;
    ok &= expect(rad_task_create_config(&task, &taskConfig, testTask, &context) == RAD_STATUS_OK, "task should create");
    ok &= expect(rad_event_wait(event, 1000) == RAD_STATUS_OK, "task should signal event");
    ok &= expect(context.ran == 1, "task body should run");
    ok &= expect(context.taskId != 0, "task should see a current task id");
    ok &= expect(context.core == RAD_TASK_CORE_SERVICE, "configured service-core task should report core 0");
    rad_task_info_t tasks[8]{};
    const size_t taskCount = rad_task_list(tasks, 8);
    ok &= expect(taskCount >= 1, "task list should report at least one task");
    bool found = false;
    for (size_t i = 0; i < taskCount && i < 8; ++i) {
        if (std::string(tasks[i].name) == "event-task") {
            found = true;
            ok &= expect(tasks[i].state == RAD_TASK_FINISHED, "task list should report finished state");
            ok &= expect(tasks[i].target_core == RAD_TASK_CORE_SERVICE, "task list should report target core");
            ok &= expect(tasks[i].priority == 3, "task list should report priority");
            ok &= expect(tasks[i].stack_size == 4096, "task list should report stack size");
        }
    }
    ok &= expect(found, "task list should include configured task");
    rad_core_info_t coreInfo{};
    ok &= expect(rad_core_info_get(&coreInfo) == RAD_STATUS_OK, "core info should be available");
    ok &= expect(coreInfo.detected_cores >= 1, "core info should report at least one core");
    rad_scheduler_info_t schedulerInfo{};
    ok &= expect(rad_scheduler_info_get(&schedulerInfo) == RAD_STATUS_OK, "scheduler info should be available");
    ok &= expect(schedulerInfo.detected_cores == coreInfo.detected_cores,
        "scheduler info should mirror detected cores");
    ok &= expect(schedulerInfo.mode == RAD_SCHEDULER_PREEMPTIVE || schedulerInfo.mode == RAD_SCHEDULER_COOPERATIVE,
        "scheduler mode should be valid");
    ok &= expect(schedulerInfo.arch[0] != '\0', "scheduler backend should be named");
    ok &= expect((schedulerInfo.online_core_mask & 1u) != 0, "scheduler should report service core online");
    ok &= expect(rad_scheduler_online_core_mask() == schedulerInfo.online_core_mask,
        "scheduler online mask helper should match scheduler info");
    ok &= expect(rad_scheduler_current_core() < schedulerInfo.detected_cores,
        "scheduler current core should be in range");
    rad_task_config_t invalidCoreConfig = taskConfig;
    invalidCoreConfig.name = "invalid-core-task";
    invalidCoreConfig.target_core = static_cast<int>(coreInfo.detected_cores);
    rad_task_t invalidTask = nullptr;
    ok &= expect(rad_task_create_config(&invalidTask, &invalidCoreConfig, testTask, &context) == RAD_STATUS_NOT_SUPPORTED,
        "invalid explicit core should be rejected");
    std::string terminal;
    ok &= expect(rad_terminal_execute("tasks", terminalWriter, &terminal) == RAD_STATUS_OK,
        "terminal tasks should execute");
    ok &= expect(terminal.find("state=") != std::string::npos, "terminal tasks should show state");
    terminal.clear();
    ok &= expect(rad_terminal_execute("sched", terminalWriter, &terminal) == RAD_STATUS_OK,
        "terminal sched should execute");
    ok &= expect(terminal.find("mode=") != std::string::npos
        && terminal.find("threads running=") != std::string::npos
        && terminal.find("online=0x") != std::string::npos
        && terminal.find("switches=") != std::string::npos,
        "terminal sched should show scheduler state");
    ok &= expect(rad_task_join(task) == RAD_STATUS_OK, "task should join");
    rad_event_destroy(event);
    return ok;
}

bool testPerfWorkAndWaitQueues() {
    bool ok = true;
    ok &= expect(rad_early_printk("RAD_EARLY_PRINTK_TEST %d\n", 1) > 0, "early printk should format text");
    ok &= expect(rad_printk("RAD_PRINTK_TEST %s\n", "ok") > 0, "printk should format text");
    rad_debug_marker("RAD_DEBUG_MARKER_TEST");
    ok &= expect(rad_cpu_interrupts_enable() == RAD_STATUS_OK, "host CPU interrupt enable should succeed");
    ok &= expect(rad_cpu_interrupts_enabled() == 1, "host CPU interrupts should report enabled");
    rad_cpu_idle();

    std::atomic<int> workRan{0};
    size_t ran = 0;
    ok &= expect(rad_work_submit("unit-work", testWork, &workRan) == RAD_STATUS_OK, "deferred work should submit");
    ok &= expect(rad_work_poll(4, &ran) == RAD_STATUS_OK, "deferred work should poll");
    ok &= expect(ran == 1 && workRan == 1, "deferred work handler should run once");

    rad_wait_queue_t queue = nullptr;
    ok &= expect(rad_wait_queue_create(&queue) == RAD_STATUS_OK, "wait queue should create");
    ok &= expect(rad_wait_queue_wait(queue, 0) == RAD_STATUS_TIMEOUT, "empty wait queue should time out");
    ok &= expect(rad_wait_queue_wake_one(queue) == RAD_STATUS_OK, "wait queue wake_one should succeed");
    ok &= expect(rad_wait_queue_wait(queue, 10) == RAD_STATUS_OK, "wait queue should consume pending wake");
    rad_wait_queue_destroy(queue);

    FakeTimer timer;
    rad_timer_source_config_t timerConfig{};
    timerConfig.size = sizeof(timerConfig);
    timerConfig.name = "unit-timer";
    timerConfig.frequency_hz = 1000;
    timerConfig.supports_oneshot = 1;
    rad_timer_source_ops_t timerOps{};
    timerOps.context = &timer;
    timerOps.start_periodic = fakeTimerStart;
    timerOps.schedule_oneshot = fakeTimerOneshot;
    timerOps.cancel_oneshot = fakeTimerCancel;
    ok &= expect(rad_timer_source_register(&timerConfig, &timerOps) == RAD_STATUS_OK, "timer source should register");
    rad_timer_tick(1000);
    ok &= expect(rad_timer_schedule_oneshot(250) == RAD_STATUS_OK, "timer one-shot should schedule");
    ok &= expect(rad_timer_cancel_oneshot() == RAD_STATUS_OK, "timer one-shot should cancel");
    rad_timer_source_info_t timers[4]{};
    const size_t timerCount = rad_timer_list(timers, 4);
    bool sawTimer = false;
    for (size_t i = 0; i < timerCount && i < 4; ++i) {
        if (std::string(timers[i].name) == "unit-timer") {
            sawTimer = true;
            ok &= expect(timers[i].frequency_hz == 1000, "timer list should show frequency");
            ok &= expect(timers[i].supports_oneshot == 1, "timer list should show one-shot support");
            ok &= expect(timers[i].ticks >= 1, "timer list should show ticks");
        }
    }
    ok &= expect(sawTimer && timer.starts == 1 && timer.oneshots == 1 && timer.cancels == 1,
        "timer callbacks should run");
    ok &= expect(rad_timer_source_unregister("unit-timer") == RAD_STATUS_OK, "timer source should unregister");

    rad_input_queue_t inputQueue = nullptr;
    ok &= expect(rad_input_queue_create("unit-input", 4, &inputQueue) == RAD_STATUS_OK, "input queue should create");
    ok &= expect(rad_input_device_register_queue("/dev/input/unit0", inputQueue) == RAD_STATUS_OK,
        "queue-backed input device should register");
    rad_input_event_t event{};
    event.size = sizeof(event);
    event.type = RAD_INPUT_EVENT_KEY;
    event.code = RAD_INPUT_KEY_ENTER;
    event.pressed = 1;
    ok &= expect(rad_input_queue_push(inputQueue, &event) == RAD_STATUS_OK, "input queue should accept event");
    rad_device_t inputDevice = nullptr;
    ok &= expect(rad_input_open("/dev/input/unit0", &inputDevice) == RAD_STATUS_OK, "queue-backed input device should open");
    rad_input_event_t received{};
    ok &= expect(rad_input_read_event(inputDevice, &received) == RAD_STATUS_OK
        && received.type == RAD_INPUT_EVENT_KEY
        && received.code == RAD_INPUT_KEY_ENTER, "queue-backed input device should read event");
    rad_device_close(inputDevice);
    rad_input_device_unregister("/dev/input/unit0");
    rad_input_queue_destroy(inputQueue);

    rad_perf_counter_add("unit.latency", 3);
    rad_perf_counter_info_t counters[32]{};
    const size_t count = rad_perf_counter_list(counters, 32);
    bool sawWork = false;
    bool sawLatency = false;
    for (size_t i = 0; i < count && i < 32; ++i) {
        if (std::string(counters[i].name) == "work.ran" && counters[i].value >= 1) sawWork = true;
        if (std::string(counters[i].name) == "unit.latency" && counters[i].value == 3) sawLatency = true;
    }
    ok &= expect(sawWork, "perf counters should include deferred work");
    ok &= expect(sawLatency, "perf counters should include custom latency counter");

    std::string terminal;
    ok &= expect(rad_terminal_execute("perf", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("work.ran") != std::string::npos, "perf command should list counters");
    terminal.clear();
    ok &= expect(rad_terminal_execute("latency", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("unit.latency") != std::string::npos, "latency command should list latency counters");
    return ok;
}

bool testMutexAndMemory() {
    rad_mutex_t mutex = nullptr;
    bool ok = expect(rad_mutex_create(&mutex) == RAD_STATUS_OK, "mutex should create");
    ok &= expect(rad_mutex_lock(mutex) == RAD_STATUS_OK, "mutex should lock");
    ok &= expect(rad_mutex_unlock(mutex) == RAD_STATUS_OK, "mutex should unlock");
    rad_mutex_destroy(mutex);

    auto* memory = static_cast<uint8_t*>(rad_memory_alloc(32));
    ok &= expect(memory != nullptr, "tracked allocation should succeed");
    rad_memory_free(memory);
    rad_memory_stats_t stats{};
    rad_memory_get_stats(&stats);
    ok &= expect(stats.allocations == 1 && stats.frees == 1, "memory counters should track alloc/free");
    ok &= expect(stats.bytes_live == 0, "memory live bytes should return to zero");
    return ok;
}

bool testCppAndModuleCore() {
    bool ok = true;

    int values[3] = {1, 2, 3};
    rad::Span<int> span(values, 3);
    ok &= expect(span.size() == 3 && span[1] == 2, "rad::Span should expose array contents");
    span[2] = 9;
    ok &= expect(values[2] == 9, "rad::Span should mutate writable storage");
    ok &= expect(rad::succeeded(RAD_STATUS_OK), "rad::succeeded should recognize OK");
    ok &= expect(rad::failed(RAD_STATUS_ERROR), "rad::failed should recognize errors");

    FakeModule module;
    rad_module_descriptor_t descriptor{};
    descriptor.size = sizeof(descriptor);
    descriptor.name = "rad_test_module";
    descriptor.bus = "test";
    descriptor.compatible = "rad,test-module";
    descriptor.context = &module;
    descriptor.init = fakeModuleInit;
    descriptor.exit = fakeModuleExit;

    ok &= expect(rad_module_register(&descriptor) == RAD_STATUS_OK, "module should register");
    ok &= expect(module.inits == 1 && module.exits == 0, "module init should run once");
    ok &= expect(rad_module_register(&descriptor) == RAD_STATUS_ALREADY_EXISTS, "duplicate module should be rejected");
    rad_module_info_t modules[4]{};
    const size_t moduleCount = rad_module_list(modules, 4);
    bool found = false;
    for (size_t i = 0; i < moduleCount; ++i) {
        if (std::string(modules[i].name) != "rad_test_module") continue;
        found = modules[i].state == RAD_MODULE_INITIALIZED
            && modules[i].last_status == RAD_STATUS_OK
            && std::string(modules[i].bus) == "test"
            && std::string(modules[i].compatible) == "rad,test-module";
    }
    ok &= expect(found, "module list should expose initialized module metadata");
    ok &= expect(rad_module_unregister("rad_test_module") == RAD_STATUS_OK, "module should unregister");
    ok &= expect(module.exits == 1, "module exit should run once");
    ok &= expect(rad_module_unregister("rad_test_module") == RAD_STATUS_NOT_FOUND, "unknown module unregister should fail");

    FakeModule failing;
    failing.initStatus = RAD_STATUS_NOT_SUPPORTED;
    rad_module_descriptor_t failingDescriptor{};
    failingDescriptor.size = sizeof(failingDescriptor);
    failingDescriptor.name = "rad_failing_module";
    failingDescriptor.context = &failing;
    failingDescriptor.init = fakeModuleInit;
    failingDescriptor.exit = fakeModuleExit;
    ok &= expect(rad_module_register(&failingDescriptor) == RAD_STATUS_NOT_SUPPORTED, "failing module should return init status");
    modules[0] = {};
    ok &= expect(rad_module_list(modules, 1) >= 1, "failing module should remain listable");
    ok &= expect(std::string(modules[0].name) == "rad_failing_module"
        && modules[0].state == RAD_MODULE_FAILED
        && modules[0].last_status == RAD_STATUS_NOT_SUPPORTED,
        "failing module should report failed state");
    ok &= expect(rad_module_unregister("rad_failing_module") == RAD_STATUS_OK, "failing module should unregister");
    ok &= expect(failing.exits == 0, "failing module exit should not run");

    FakeModule raii;
    rad_module_descriptor_t raiiDescriptor{};
    raiiDescriptor.size = sizeof(raiiDescriptor);
    raiiDescriptor.name = "rad_raii_module";
    raiiDescriptor.context = &raii;
    raiiDescriptor.init = fakeModuleInit;
    raiiDescriptor.exit = fakeModuleExit;
    {
        rad::Module scoped(raiiDescriptor);
        ok &= expect(scoped.status() == RAD_STATUS_OK, "rad::Module should register in constructor");
        ok &= expect(raii.inits == 1 && raii.exits == 0, "rad::Module should init immediately");
    }
    ok &= expect(raii.exits == 1, "rad::Module should unregister in destructor");

    return ok;
}

bool testVfsAndTerminal() {
    const auto root = std::filesystem::temp_directory_path() / "radixkernel-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    bool ok = expect(rad_vfs_mount_host("/host", root.string().c_str()) == RAD_STATUS_OK, "host VFS should mount");
    rad_file_t file = nullptr;
    ok &= expect(rad_vfs_open("/host/test.txt", RAD_VFS_WRITE | RAD_VFS_CREATE | RAD_VFS_TRUNCATE, &file) == RAD_STATUS_OK,
        "VFS file should open for write");
    const char text[] = "radixkernel";
    size_t done = 0;
    ok &= expect(rad_vfs_write(file, text, sizeof(text) - 1, &done) == RAD_STATUS_OK && done == sizeof(text) - 1,
        "VFS write should complete");
    rad_vfs_close(file);

    ok &= expect(rad_vfs_open("/host/test.txt", RAD_VFS_READ, &file) == RAD_STATUS_OK, "VFS file should open for read");
    char buffer[32]{};
    ok &= expect(rad_vfs_read(file, buffer, sizeof(buffer), &done) == RAD_STATUS_OK, "VFS read should complete");
    rad_vfs_close(file);
    ok &= expect(std::string(buffer, done) == "radixkernel", "VFS read should match written text");

    rad_vfs_stat_t stat{};
    ok &= expect(rad_vfs_stat("/host/test.txt", &stat) == RAD_STATUS_OK && stat.size == sizeof(text) - 1,
        "VFS stat should report file size");
    ok &= expect((stat.mode & 0100000u) != 0, "VFS stat should report regular file mode");

    ok &= expect(rad_vfs_mkdir("/host/bin") == RAD_STATUS_OK, "VFS mkdir should create a directory");
    ok &= expect(rad_vfs_chdir("/host/bin") == RAD_STATUS_OK, "VFS chdir should accept mounted directories");
    char cwd[96]{};
    ok &= expect(rad_vfs_getcwd(cwd, sizeof(cwd)) == RAD_STATUS_OK && std::string(cwd) == "/host/bin",
        "VFS getcwd should report current VFS directory");
    ok &= expect(rad_vfs_open("module.rad", RAD_VFS_WRITE | RAD_VFS_CREATE | RAD_VFS_TRUNCATE, &file) == RAD_STATUS_OK,
        "VFS relative open should honor cwd");
    const char moduleText[] = "host-sim-program";
    ok &= expect(rad_vfs_write(file, moduleText, sizeof(moduleText) - 1, &done) == RAD_STATUS_OK,
        "VFS should write relative file");
    rad_vfs_close(file);
    ok &= expect(rad_vfs_rename("module.rad", "module2.rad") == RAD_STATUS_OK,
        "VFS rename should work with relative paths");
    rad_dir_t dir = nullptr;
    ok &= expect(rad_vfs_opendir(".", &dir) == RAD_STATUS_OK, "VFS opendir should open cwd");
    bool sawModule = false;
    rad_vfs_dirent_t dirent{};
    while (rad_vfs_readdir(dir, &dirent) == RAD_STATUS_OK) {
        if (std::string(dirent.name) == "module2.rad") sawModule = true;
    }
    rad_vfs_closedir(dir);
    ok &= expect(sawModule, "VFS readdir should enumerate renamed file");

    rad_program_t program = nullptr;
    ok &= expect(rad_program_load("module2.rad", &program) == RAD_STATUS_OK,
        "program loader should load from VFS cwd");
    rad_task_t programTask = nullptr;
    const char* programArgs[] = {"alpha", "beta"};
    ok &= expect(rad_program_spawn(program, 2, programArgs, &programTask) == RAD_STATUS_OK,
        "program spawn should schedule a RAD task");
    ok &= expect(rad_task_join(programTask) == RAD_STATUS_OK, "program task should join");
    rad_program_info_t programInfo[4]{};
    const size_t programCount = rad_program_list(programInfo, 4);
    ok &= expect(programCount >= 1, "program list should report loaded program");
    ok &= expect(programInfo[0].state == RAD_PROGRAM_FINISHED, "program list should report finished simulator program");
    rad_program_unload(program);
    ok &= expect(rad_program_list(programInfo, 4) == programCount - 1, "program unload should remove program");

    ok &= expect(rad_vfs_remove("module2.rad") == RAD_STATUS_OK, "VFS remove should unlink file");
    ok &= expect(rad_vfs_chdir("/host") == RAD_STATUS_OK, "VFS chdir should return to mount");
    ok &= expect(rad_vfs_rmdir("/host/bin") == RAD_STATUS_OK, "VFS rmdir should remove empty directory");

    std::string terminal;
    ok &= expect(rad_terminal_execute("ls /host", terminalWriter, &terminal) == RAD_STATUS_OK,
        "terminal ls should execute");
    ok &= expect(terminal.find("test.txt") != std::string::npos, "terminal ls should show file");
    terminal.clear();
    ok &= expect(rad_terminal_execute("cores", terminalWriter, &terminal) == RAD_STATUS_OK,
        "terminal cores should execute");
    ok &= expect(terminal.find("detected=") != std::string::npos, "terminal cores should show core count");
    terminal.clear();
    ok &= expect(rad_terminal_execute("sched", terminalWriter, &terminal) == RAD_STATUS_OK,
        "terminal sched should execute");
    ok &= expect(terminal.find("arch=") != std::string::npos
        && terminal.find("online=0x") != std::string::npos,
        "terminal sched should show backend name and online cores");
    terminal.clear();
    ok &= expect(rad_terminal_execute("cat /host/test.txt", terminalWriter, &terminal) == RAD_STATUS_OK,
        "terminal cat should execute");
    ok &= expect(terminal.find("radixkernel") != std::string::npos, "terminal cat should print file");

    rad_vfs_unmount("/host");
    rad_sd_config_t sd{};
    sd.mount_point = "/sd";
    sd.mode = RAD_SD_MODE_AUTO;
    ok &= expect(rad_vfs_mount_sd(&sd) == RAD_STATUS_NOT_SUPPORTED, "linux_sim SD mount should be explicitly unsupported");
    std::filesystem::remove_all(root);
    return ok;
}

bool testDeviceRegistry() {
    FakeBus bus;
    rad_device_ops_t ops{};
    ops.context = &bus;
    ops.read = fakeRead;
    ops.write = fakeWrite;
    ops.ioctl = fakeIoctl;

    bool ok = expect(rad_device_register("i2c0", RAD_DEVICE_I2C, &ops) == RAD_STATUS_OK, "device should register");
    rad_device_t device = nullptr;
    ok &= expect(rad_device_open("i2c0", &device) == RAD_STATUS_OK, "device should open");
    uint8_t writeData[2] = {0x10, 0x20};
    uint8_t readData[2] = {};
    rad_i2c_transfer_t transfer{};
    transfer.address = 0x48;
    transfer.write_data = writeData;
    transfer.write_size = sizeof(writeData);
    transfer.read_data = readData;
    transfer.read_size = sizeof(readData);
    ok &= expect(rad_i2c_transfer_device(device, &transfer) == RAD_STATUS_OK, "I2C transfer should dispatch through ioctl");
    ok &= expect(bus.lastWrite.size() == 2 && bus.lastWrite[0] == 0x10, "I2C write payload should be captured");
    ok &= expect(readData[0] == bus.response && readData[1] == bus.response, "I2C read payload should be filled");
    FakeSpi spi;
    rad_device_ops_t spiOps{};
    spiOps.context = &spi;
    spiOps.ioctl = fakeSpiIoctl;
    ok &= expect(rad_device_register("spi0", RAD_DEVICE_SPI, &spiOps) == RAD_STATUS_OK, "SPI device should register");
    rad_device_t spiDevice = nullptr;
    ok &= expect(rad_device_open("spi0", &spiDevice) == RAD_STATUS_OK, "SPI device should open");
    uint8_t spiTx[2] = {0x9f, 0x01};
    uint8_t spiRx[2] = {};
    rad_spi_transfer_t spiTransfer{};
    spiTransfer.tx_data = spiTx;
    spiTransfer.rx_data = spiRx;
    spiTransfer.size = sizeof(spiTx);
    ok &= expect(rad_spi_transfer_device(spiDevice, &spiTransfer) == RAD_STATUS_OK, "SPI transfer should dispatch through ioctl");
    ok &= expect(spi.lastTx.size() == 2 && spiRx[0] == 0xa5, "SPI ioctl payload should be handled");
    rad_device_close(spiDevice);
    ok &= expect(rad_device_unregister("spi0") == RAD_STATUS_OK, "SPI device should unregister");
    char names[4][64]{};
    rad_device_type_t types[4]{};
    ok &= expect(rad_device_list(names, types, 4) >= 1, "device list should report registered device");
    rad_device_close(device);
    ok &= expect(rad_device_unregister("i2c0") == RAD_STATUS_OK, "device should unregister");
    return ok;
}

bool testInputCore() {
    FakeInput input;
    input.event.size = sizeof(input.event);
    input.event.type = RAD_INPUT_EVENT_KEY;
    input.event.code = RAD_INPUT_KEY_UNKNOWN;
    input.event.codepoint = 'a';
    input.event.modifiers = RAD_INPUT_MOD_CTRL;
    input.event.pressed = 1;

    rad_device_ops_t ops{};
    ops.context = &input;
    ops.read = fakeInputRead;
    bool ok = expect(rad_input_device_register("/dev/input/event-test", &ops) == RAD_STATUS_OK,
        "input device should register");

    rad_device_t device = nullptr;
    ok &= expect(rad_input_open("/dev/input/event-test", &device) == RAD_STATUS_OK,
        "input device should open through input API");
    rad_input_event_t event{};
    ok &= expect(rad_input_read_event(device, &event) == RAD_STATUS_OK
            && event.type == RAD_INPUT_EVENT_KEY
            && event.codepoint == 'a'
            && event.modifiers == RAD_INPUT_MOD_CTRL
            && event.pressed == 1,
        "input event should read through typed API");
    input.consumed = false;
    input.event = {};
    input.event.size = sizeof(input.event);
    input.event.type = RAD_INPUT_EVENT_POINTER_BUTTON;
    input.event.x = 123;
    input.event.y = 45;
    input.event.code = RAD_INPUT_POINTER_BUTTON_LEFT;
    input.event.buttons = RAD_INPUT_POINTER_BUTTON_LEFT;
    input.event.pressed = 1;
    ok &= expect(rad_input_read_event(device, &event) == RAD_STATUS_OK
            && event.type == RAD_INPUT_EVENT_POINTER_BUTTON
            && event.x == 123
            && event.y == 45
            && event.code == RAD_INPUT_POINTER_BUTTON_LEFT
            && event.buttons == RAD_INPUT_POINTER_BUTTON_LEFT
            && event.pressed == 1,
        "pointer input event should read through typed API");
    ok &= expect(rad_input_read_event(device, &event) == RAD_STATUS_TIMEOUT,
        "empty input device read should report timeout");
    rad_device_close(device);
    ok &= expect(rad_input_device_unregister("/dev/input/event-test") == RAD_STATUS_OK,
        "input device should unregister");
    return ok;
}

bool testBlockCore() {
    FakeBlock block;
    block.info.size = sizeof(block.info);
    block.info.sector_size = 512;
    block.info.sector_count = 128;
    rad_device_ops_t ops{};
    ops.context = &block;
    ops.ioctl = fakeBlockIoctl;
    bool ok = expect(rad_block_device_register("/dev/fakeblk0", &ops) == RAD_STATUS_OK,
        "block device should register");

    rad_device_t device = nullptr;
    ok &= expect(rad_block_open("/dev/fakeblk0", &device) == RAD_STATUS_OK,
        "block device should open through block API");
    rad_block_info_t info{};
    ok &= expect(rad_block_info(device, &info) == RAD_STATUS_OK
            && info.sector_size == 512
            && info.sector_count == 128,
        "block info should dispatch through ioctl");
    uint8_t buffer[1024]{};
    ok &= expect(rad_block_read(device, 7, 2, buffer) == RAD_STATUS_OK,
        "block read should dispatch through ioctl");
    ok &= expect(block.lastSector == 7 && block.lastSectorCount == 2 && buffer[0] == 0x5c && buffer[511] == 0x5c,
        "block read should fill requested sectors");
    buffer[0] = 0xa1;
    buffer[511] = 0xb2;
    ok &= expect(rad_block_write(device, 9, 1, buffer) == RAD_STATUS_OK,
        "block write should dispatch through ioctl");
    ok &= expect(block.lastSector == 9 && block.lastSectorCount == 1
            && block.lastWrite.size() == 512
            && block.lastWrite[0] == 0xa1
            && block.lastWrite[511] == 0xb2,
        "block write should pass requested sectors");
    ok &= expect(rad_block_flush(device) == RAD_STATUS_OK && block.flushes == 1,
        "block flush should dispatch through ioctl");
    rad_device_close(device);
    ok &= expect(rad_device_unregister("/dev/fakeblk0") == RAD_STATUS_OK,
        "block device should unregister");
    return ok;
}

bool testNetCore() {
    FakeNet net;
    net.info.size = sizeof(net.info);
    net.info.mac.bytes[0] = 0x02;
    net.info.mac.bytes[1] = 0x52;
    net.info.mac.bytes[2] = 0x41;
    net.info.mac.bytes[3] = 0x44;
    net.info.mac.bytes[4] = 0x00;
    net.info.mac.bytes[5] = 0x01;
    net.info.mtu = 1500;
    net.info.link_up = 1;
    rad_device_ops_t ops{};
    ops.context = &net;
    ops.ioctl = fakeNetIoctl;
    bool ok = expect(rad_net_device_register("/dev/net-test0", &ops) == RAD_STATUS_OK,
        "network device should register");
    rad_device_t device = nullptr;
    ok &= expect(rad_net_open("/dev/net-test0", &device) == RAD_STATUS_OK,
        "network device should open through net API");
    rad_net_link_info_t info{};
    ok &= expect(rad_net_link_info(device, &info) == RAD_STATUS_OK
            && info.link_up
            && info.mtu == 1500
            && info.mac.bytes[2] == 0x41,
        "network link info should dispatch through ioctl");
    const uint8_t frame[14] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x02, 0x52, 0x41, 0x44, 0x00, 0x01,
        0x08, 0x00
    };
    ok &= expect(rad_net_send(device, frame, sizeof(frame)) == RAD_STATUS_OK
            && net.lastPacket.size() == sizeof(frame)
            && net.lastPacket[12] == 0x08,
        "network send should dispatch packet through ioctl");
    net.rxPacket.assign(frame, frame + sizeof(frame));
    uint8_t rx[32]{};
    const intptr_t received = rad_net_receive(device, rx, sizeof(rx));
    ok &= expect(received == static_cast<intptr_t>(sizeof(frame))
            && rx[12] == 0x08
            && net.info.rx_packets == 1,
        "network receive should dispatch packet through ioctl");
    ok &= expect(rad_net_poll(device) == RAD_STATUS_OK && net.polls == 1,
        "network poll should dispatch through ioctl");
    rad_device_close(device);
    ok &= expect(rad_device_unregister("/dev/net-test0") == RAD_STATUS_OK,
        "network device should unregister");
    return ok;
}

bool testUdpSocketCore() {
    const int32_t server = static_cast<int32_t>(rad_syscall_dispatch(RAD_SYSCALL_SOCKET, RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP, 0, 0, 0));
    const int32_t client = static_cast<int32_t>(rad_syscall_dispatch(RAD_SYSCALL_SOCKET, RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP, 0, 0, 0));
    bool ok = expect(server >= 3 && client >= 3, "UDP sockets should allocate fds");
    rad_sockaddr_in_t server_addr{};
    server_addr.family = RAD_AF_INET;
    server_addr.port = 9000;
    server_addr.address = rad_ipv4_address_t{{10, 0, 2, 15}};
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_BIND, server, reinterpret_cast<uintptr_t>(&server_addr), sizeof(server_addr), 0, 0, 0) == RAD_STATUS_OK,
        "UDP bind should attach local port");
    const char payload[] = "radix-udp";
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_SENDTO, client, reinterpret_cast<uintptr_t>(payload), sizeof(payload), 0, reinterpret_cast<uintptr_t>(&server_addr), sizeof(server_addr)) == static_cast<intptr_t>(sizeof(payload)),
        "UDP sendto should deliver datagram");
    char buffer[32]{};
    rad_sockaddr_in_t from{};
    size_t from_len = sizeof(from);
    const intptr_t got = rad_syscall_dispatch(RAD_SYSCALL_RECVFROM, server, reinterpret_cast<uintptr_t>(buffer), sizeof(buffer), 0, reinterpret_cast<uintptr_t>(&from), reinterpret_cast<uintptr_t>(&from_len));
    ok &= expect(got == static_cast<intptr_t>(sizeof(payload))
            && std::strcmp(buffer, payload) == 0
            && from.family == RAD_AF_INET,
        "UDP recvfrom should return payload and sender");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_CONNECT, client, 0, 0, 0, 0, 0) == RAD_STATUS_INVALID_ARGUMENT,
        "UDP connect without an address should fail validation");
    rad_syscall_dispatch(RAD_SYSCALL_CLOSE, client, 0, 0, 0, 0, 0);
    rad_syscall_dispatch(RAD_SYSCALL_CLOSE, server, 0, 0, 0, 0, 0);
    return ok;
}

bool testTcpSocketCore() {
    const int32_t server = static_cast<int32_t>(rad_syscall_dispatch(RAD_SYSCALL_SOCKET, RAD_AF_INET, RAD_SOCK_STREAM, RAD_IPPROTO_TCP, 0, 0, 0));
    const int32_t client = static_cast<int32_t>(rad_syscall_dispatch(RAD_SYSCALL_SOCKET, RAD_AF_INET, RAD_SOCK_STREAM, RAD_IPPROTO_TCP, 0, 0, 0));
    bool ok = expect(server >= 3 && client >= 3, "TCP sockets should allocate fds");
    rad_sockaddr_in_t server_addr{};
    server_addr.family = RAD_AF_INET;
    server_addr.port = 9100;
    server_addr.address = rad_ipv4_address_t{{10, 0, 2, 15}};
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_BIND, server, reinterpret_cast<uintptr_t>(&server_addr), sizeof(server_addr), 0, 0, 0) == RAD_STATUS_OK,
        "TCP bind should attach local port");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_LISTEN, server, 4, 0, 0, 0, 0) == RAD_STATUS_OK,
        "TCP listen should enter listen state");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_CONNECT, client, reinterpret_cast<uintptr_t>(&server_addr), sizeof(server_addr), 0, 0, 0) == RAD_STATUS_OK,
        "TCP connect should establish local connection");
    rad_sockaddr_in_t peer{};
    size_t peer_len = sizeof(peer);
    const int32_t accepted = static_cast<int32_t>(rad_syscall_dispatch(RAD_SYSCALL_ACCEPT, server, reinterpret_cast<uintptr_t>(&peer), reinterpret_cast<uintptr_t>(&peer_len), 0, 0, 0));
    ok &= expect(accepted >= 3 && peer.family == RAD_AF_INET, "TCP accept should return connected fd");
    rad_socket_info_t client_info{};
    ok &= expect(rad_socket_get_info(client, &client_info) == RAD_STATUS_OK
            && client_info.type == static_cast<int>(RAD_SOCK_STREAM)
            && client_info.tcp_state == RAD_TCP_ESTABLISHED,
        "TCP connected socket should report established state");
    const char payload[] = "radix-tcp";
    ok &= expect(rad_socket_send(client, payload, sizeof(payload), 0) == static_cast<intptr_t>(sizeof(payload)),
        "TCP send should write stream bytes");
    char buffer[32]{};
    ok &= expect(rad_socket_recv(accepted, buffer, sizeof(buffer), 0) == static_cast<intptr_t>(sizeof(payload))
            && std::strcmp(buffer, payload) == 0,
        "TCP recv should read stream bytes");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_SHUTDOWN, client, 2, 0, 0, 0, 0) == RAD_STATUS_OK,
        "TCP shutdown should succeed");
    rad_syscall_dispatch(RAD_SYSCALL_CLOSE, accepted, 0, 0, 0, 0, 0);
    rad_syscall_dispatch(RAD_SYSCALL_CLOSE, client, 0, 0, 0, 0, 0);
    rad_syscall_dispatch(RAD_SYSCALL_CLOSE, server, 0, 0, 0, 0, 0);
    return ok;
}

bool testPosixSyscallCore() {
    const auto root = std::filesystem::temp_directory_path() / "radixkernel_posix_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    bool ok = expect(rad_vfs_mount_host("/posix", root.string().c_str()) == RAD_STATUS_OK,
        "POSIX test VFS should mount");

    const char payload[] = "radix-posix";
    int32_t fd = static_cast<int32_t>(rad_syscall_dispatch(RAD_SYSCALL_OPEN,
        reinterpret_cast<uintptr_t>("/posix/sys.txt"),
        RAD_VFS_WRITE | RAD_VFS_CREATE | RAD_VFS_TRUNCATE, 0, 0, 0, 0));
    ok &= expect(fd >= 3, "syscall open should allocate fd");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_WRITE, fd, reinterpret_cast<uintptr_t>(payload),
            sizeof(payload) - 1, 0, 0, 0) == static_cast<intptr_t>(sizeof(payload) - 1),
        "syscall write should write payload");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_CLOSE, fd, 0, 0, 0, 0, 0) == RAD_STATUS_OK,
        "syscall close should close writable fd");

    fd = rad_fd_open("/posix/sys.txt", RAD_VFS_READ);
    ok &= expect(fd >= 3, "rad_fd_open should open readable fd");
    int32_t duped = rad_fd_dup(fd);
    ok &= expect(duped >= 3 && duped != fd, "rad_fd_dup should allocate another fd");
    int32_t fixed = rad_fd_dup2(fd, 9);
    ok &= expect(fixed == 9, "rad_fd_dup2 should install requested fd");
    char buffer[32]{};
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_READ, fixed, reinterpret_cast<uintptr_t>(buffer),
            sizeof(buffer), 0, 0, 0) == static_cast<intptr_t>(sizeof(payload) - 1),
        "syscall read should read through dup2 fd");
    ok &= expect(std::string(buffer) == payload, "syscall read payload should match");
    ok &= expect(rad_fd_lseek(fd, 0, RAD_SEEK_SET) == 0, "rad_fd_lseek should rewind");
    rad_vfs_stat_t stat{};
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_STAT, reinterpret_cast<uintptr_t>("/posix/sys.txt"),
            reinterpret_cast<uintptr_t>(&stat), 0, 0, 0, 0) == RAD_STATUS_OK
            && stat.size == sizeof(payload) - 1,
        "syscall stat should report file size");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_CHMOD, reinterpret_cast<uintptr_t>("/posix/sys.txt"),
            0755, 0, 0, 0, 0) == RAD_STATUS_OK,
        "syscall chmod should update VFS permissions");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_LINK, reinterpret_cast<uintptr_t>("/posix/sys.txt"),
            reinterpret_cast<uintptr_t>("/posix/sys-hardlink.txt"), 0, 0, 0, 0) == RAD_STATUS_OK,
        "syscall link should create a hard link");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_SYMLINK, reinterpret_cast<uintptr_t>("/posix/sys.txt"),
            reinterpret_cast<uintptr_t>("/posix/sys-symlink.txt"), 0, 0, 0, 0) == RAD_STATUS_OK,
        "syscall symlink should create a symbolic link");
    char linkTarget[128]{};
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_READLINK, reinterpret_cast<uintptr_t>("/posix/sys-symlink.txt"),
            reinterpret_cast<uintptr_t>(linkTarget), sizeof(linkTarget), 0, 0, 0) == RAD_STATUS_OK
            && std::string(linkTarget) == "/posix/sys.txt",
        "syscall readlink should return symbolic link target");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_FSYNC, fd, 0, 0, 0, 0, 0) == RAD_STATUS_OK,
        "syscall fsync should flush file fds");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_FSTAT, fixed, reinterpret_cast<uintptr_t>(&stat),
            0, 0, 0, 0) == RAD_STATUS_NOT_SUPPORTED,
        "first-pass fstat should reject VFS fds without stored path");
    ok &= expect(rad_fd_close(duped) == RAD_STATUS_OK, "dup fd should close");
    ok &= expect(rad_fd_close(fixed) == RAD_STATUS_OK, "dup2 fd should close");
    ok &= expect(rad_fd_close(fd) == RAD_STATUS_OK, "original fd should close");

    rad_posix_timeval_t tv{};
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_GETTIMEOFDAY, reinterpret_cast<uintptr_t>(&tv),
            0, 0, 0, 0, 0) == RAD_STATUS_OK,
        "syscall gettimeofday should succeed");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_GETPID, 0, 0, 0, 0, 0, 0) == 1,
        "syscall getpid should return init pid");

    int32_t pipefd[2]{};
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_PIPE, reinterpret_cast<uintptr_t>(pipefd),
            0, 0, 0, 0, 0) == RAD_STATUS_OK
            && pipefd[0] >= 3 && pipefd[1] >= 3 && pipefd[0] != pipefd[1],
        "syscall pipe should allocate readable and writable fds");
    rad_pollfd_t pollfds[2]{{pipefd[0], RAD_POLLIN, 0}, {pipefd[1], RAD_POLLOUT, 0}};
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_POLL, reinterpret_cast<uintptr_t>(pollfds),
            2, 0, 0, 0, 0) == 1
            && (pollfds[1].revents & RAD_POLLOUT)
            && pollfds[0].revents == 0,
        "poll should report an empty pipe write end ready");
    const char pipePayload[] = "pipe-data";
    ok &= expect(rad_fd_write(pipefd[1], pipePayload, sizeof(pipePayload) - 1) == static_cast<intptr_t>(sizeof(pipePayload) - 1),
        "pipe write should accept payload");
    pollfds[0].revents = 0;
    pollfds[1].revents = 0;
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_POLL, reinterpret_cast<uintptr_t>(pollfds),
            2, 0, 0, 0, 0) == 2
            && (pollfds[0].revents & RAD_POLLIN)
            && (pollfds[1].revents & RAD_POLLOUT),
        "poll should report pipe read and write readiness after write");
    char pipeBuffer[32]{};
    ok &= expect(rad_fd_read(pipefd[0], pipeBuffer, sizeof(pipeBuffer)) == static_cast<intptr_t>(sizeof(pipePayload) - 1),
        "pipe read should return payload");
    ok &= expect(std::string(pipeBuffer) == pipePayload, "pipe read payload should match");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_FSTAT, pipefd[0], reinterpret_cast<uintptr_t>(&stat),
            0, 0, 0, 0) == RAD_STATUS_OK
            && ((stat.mode & 0170000u) == 0010000u),
        "pipe fstat should report FIFO mode");

    ok &= expect(rad_fd_fcntl(pipefd[1], RAD_FCNTL_GETFL, 0) == RAD_VFS_WRITE,
        "fcntl GETFL should return open flags");
    ok &= expect(rad_fd_fcntl(pipefd[1], RAD_FCNTL_SETFD, RAD_FD_CLOEXEC) == RAD_STATUS_OK
            && rad_fd_fcntl(pipefd[1], RAD_FCNTL_GETFD, 0) == RAD_FD_CLOEXEC,
        "fcntl should round-trip close-on-exec");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_EXECVE, reinterpret_cast<uintptr_t>("/posix/sys.txt"),
            0, 0, 0, 0, 0) == RAD_STATUS_OK,
        "syscall execve should validate executable path and update process image");
    ok &= expect(rad_fd_write(pipefd[1], pipePayload, sizeof(pipePayload) - 1) == RAD_STATUS_NOT_FOUND,
        "execve should close close-on-exec fd");
    ok &= expect(rad_fd_close(pipefd[0]) == RAD_STATUS_OK,
        "pipe read fd should close after close-on-exec write fd");

    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_PIPE, reinterpret_cast<uintptr_t>(pipefd),
            0, 0, 0, 0, 0) == RAD_STATUS_OK,
        "second pipe should allocate for fork inheritance");
    FakeForkContext forkContext{};
    rad_process_arch_ops_t archOps{};
    archOps.size = sizeof(archOps);
    archOps.context = &forkContext;
    archOps.fork_from_frame = fakeForkFromFrame;
    ok &= expect(rad_process_arch_register(&archOps) == RAD_STATUS_OK,
        "process arch hook should register");
    int32_t child = static_cast<int32_t>(rad_syscall_dispatch(RAD_SYSCALL_FORK, 0, 0, 0, 0, 0, 0));
    ok &= expect(child > 1 && forkContext.calls == 1, "syscall fork should use registered arch hook");
    int32_t childStatus = 0;
    ok &= expect(rad_process_waitpid(child, &childStatus, RAD_WAIT_NOHANG) == 0,
        "waitpid WNOHANG should report running child");
    rad_process_set_current_pid(child);
    ok &= expect(rad_fd_write(pipefd[1], pipePayload, sizeof(pipePayload) - 1) == static_cast<intptr_t>(sizeof(pipePayload) - 1),
        "forked child should inherit writable pipe fd");
    rad_process_exit(7);
    rad_process_set_current_pid(1);
    std::memset(pipeBuffer, 0, sizeof(pipeBuffer));
    ok &= expect(rad_fd_read(pipefd[0], pipeBuffer, sizeof(pipeBuffer)) == static_cast<intptr_t>(sizeof(pipePayload) - 1)
            && std::string(pipeBuffer) == pipePayload,
        "parent should read pipe payload written by child");
    ok &= expect(rad_process_waitpid(child, &childStatus, 0) == child && childStatus == 7,
        "waitpid should reap zombie child and return exit status");
    ok &= expect(rad_process_waitpid(child, &childStatus, RAD_WAIT_NOHANG) == RAD_STATUS_NOT_FOUND,
        "reaped child should no longer be waitable");
    ok &= expect(rad_fd_close(pipefd[0]) == RAD_STATUS_OK && rad_fd_close(pipefd[1]) == RAD_STATUS_OK,
        "fork pipe fds should close");
    ok &= expect(rad_syscall_dispatch(RAD_SYSCALL_EXECVE, reinterpret_cast<uintptr_t>("/posix/sys.txt"),
            0, 0, 0, 0, 0) == RAD_STATUS_OK,
        "syscall execve should validate executable path and update process image");

    rad_vfs_unmount("/posix");
    std::filesystem::remove_all(root);
    return ok;
}

rad_status_t fakeControllerTransfer(void* context, const rad_i2c_transfer_t* transfer) {
    auto* bus = static_cast<FakeBus*>(context);
    if (!bus || !transfer) return RAD_STATUS_INVALID_ARGUMENT;
    if (transfer->write_data && transfer->write_size) {
        bus->lastWrite.assign(transfer->write_data, transfer->write_data + transfer->write_size);
    }
    if (transfer->read_data && transfer->read_size) {
        std::memset(transfer->read_data, bus->response, transfer->read_size);
    }
    return RAD_STATUS_OK;
}

rad_status_t fakeI2cProbe(void* context, rad_i2c_device_t* device) {
    auto* bus = static_cast<FakeBus*>(context);
    ++bus->probes;
    bus->irqProbeCount = static_cast<int>(rad_i2c_device_irq_count(device));
    rad_irq_resource_t irq{};
    if (rad_i2c_device_get_irq(device, 0, &irq) == RAD_STATUS_OK) {
        bus->irqLine = irq.line;
        bus->irqNumber = irq.irq;
    }
    return RAD_STATUS_OK;
}

void fakeI2cRemove(void* context, rad_i2c_device_t*) {
    ++static_cast<FakeBus*>(context)->removes;
}

rad_status_t fakeSpiTransfer(void* context, const rad_spi_device_t*, const rad_spi_transfer_t* transfer) {
    auto* spi = static_cast<FakeSpi*>(context);
    if (!spi || !transfer) return RAD_STATUS_INVALID_ARGUMENT;
    if (transfer->tx_data && transfer->size) spi->lastTx.assign(transfer->tx_data, transfer->tx_data + transfer->size);
    if (transfer->rx_data && transfer->size) {
        for (size_t i = 0; i < transfer->size; ++i) {
            const uint8_t value = transfer->tx_data ? transfer->tx_data[i] : 0u;
            transfer->rx_data[i] = static_cast<uint8_t>(value ^ 0xffu);
        }
    }
    ++spi->pioTransfers;
    return RAD_STATUS_OK;
}

rad_status_t fakeSpiDmaTransfer(void* context, const rad_spi_device_t*, const rad_spi_transfer_t* transfer, rad_dma_channel_t tx, rad_dma_channel_t rx) {
    auto* spi = static_cast<FakeSpi*>(context);
    if (!spi || !transfer) return RAD_STATUS_INVALID_ARGUMENT;
    rad_dma_transfer_t txTransfer{};
    txTransfer.type = RAD_DMA_MEMORY_TO_DEVICE;
    txTransfer.request_id = RAD_DMA_DREQ_SPI0_TX;
    txTransfer.source = transfer->tx_data;
    txTransfer.size = static_cast<uint32_t>(transfer->size);
    rad_dma_transfer_t rxTransfer{};
    rxTransfer.type = RAD_DMA_DEVICE_TO_MEMORY;
    rxTransfer.request_id = RAD_DMA_DREQ_SPI0_RX;
    rxTransfer.destination = transfer->rx_data;
    rxTransfer.size = static_cast<uint32_t>(transfer->size);
    rad_status_t status = rad_dma_submit(tx, &txTransfer);
    if (status == RAD_STATUS_OK) status = rad_dma_submit(rx, &rxTransfer);
    if (status == RAD_STATUS_OK) status = rad_dma_wait(tx, 100);
    if (status == RAD_STATUS_OK) status = rad_dma_wait(rx, 100);
    if (status != RAD_STATUS_OK) return status;
    if (transfer->tx_data && transfer->size) spi->lastTx.assign(transfer->tx_data, transfer->tx_data + transfer->size);
    ++spi->dmaTransfers;
    return RAD_STATUS_OK;
}

rad_status_t fakeSpiProbe(void* context, rad_spi_device_t* device) {
    auto* spi = static_cast<FakeSpi*>(context);
    ++spi->probes;
    const size_t count = rad_spi_device_irq_count(device);
    if (count) {
        spi->irqProbeCount = static_cast<int>(count);
        rad_irq_resource_t irq{};
        if (rad_spi_device_get_irq(device, 0, &irq) == RAD_STATUS_OK) {
            spi->irqLine = irq.line;
            spi->irqNumber = irq.irq;
        }
    }
    return RAD_STATUS_OK;
}

void fakeSpiRemove(void* context, rad_spi_device_t*) {
    ++static_cast<FakeSpi*>(context)->removes;
}

rad_status_t fakeDmaRequest(void* context, rad_dma_request_id_t, void** backendChannel) {
    auto* dma = static_cast<FakeDma*>(context);
    ++dma->requests;
    *backendChannel = dma;
    return RAD_STATUS_OK;
}

void fakeDmaRelease(void* context, void*) {
    ++static_cast<FakeDma*>(context)->releases;
}

rad_status_t fakeDmaSubmit(void* context, void*, const rad_dma_transfer_t* transfer) {
    auto* dma = static_cast<FakeDma*>(context);
    ++dma->submits;
    if (!transfer) return RAD_STATUS_INVALID_ARGUMENT;
    if (transfer->type == RAD_DMA_MEMORY_TO_MEMORY && transfer->source && transfer->destination) {
        std::memcpy(transfer->destination, transfer->source, transfer->size);
    } else if (transfer->type == RAD_DMA_DEVICE_TO_MEMORY && transfer->destination) {
        std::memset(transfer->destination, 0xd5, transfer->size);
    }
    return RAD_STATUS_OK;
}

rad_status_t fakeDmaWait(void* context, void*, uint32_t) {
    auto* dma = static_cast<FakeDma*>(context);
    ++dma->waits;
    if (dma->timeoutNext) {
        dma->timeoutNext = false;
        return RAD_STATUS_TIMEOUT;
    }
    return RAD_STATUS_OK;
}

rad_status_t fakeDmaCancel(void* context, void*) {
    ++static_cast<FakeDma*>(context)->cancels;
    return RAD_STATUS_OK;
}

rad_status_t fakeFramebufferFlush(void* context, const rad_framebuffer_rect_t*) {
    ++static_cast<FakeFramebuffer*>(context)->flushes;
    return RAD_STATUS_OK;
}

rad_status_t fakeFramebufferSetMode(void* context, const rad_display_mode_t*) {
    ++static_cast<FakeFramebuffer*>(context)->setModes;
    return RAD_STATUS_OK;
}

rad_status_t fakeFramebufferBlank(void* context, int blanked) {
    auto* fake = static_cast<FakeFramebuffer*>(context);
    ++fake->blanks;
    fake->blanked = blanked;
    return RAD_STATUS_OK;
}

uint64_t fakeFramebufferVsync(void* context) {
    return static_cast<FakeFramebuffer*>(context)->vsync;
}

bool testFramebufferCore() {
    FakeFramebuffer fake;
    FakeFramebuffer secondFake;
    uint32_t pixels[64u * 32u]{};
    uint16_t secondPixels[32u * 16u]{};
    rad_framebuffer_config_t config{};
    config.size = sizeof(config);
    config.name = "/dev/fb-test";
    config.info.size = sizeof(config.info);
    config.info.width = 64;
    config.info.height = 32;
    config.info.stride_bytes = 64u * 4u;
    config.info.pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
    config.info.pixels = pixels;
    config.output_type = RAD_DISPLAY_OUTPUT_GRUB;
    config.connector = "qemu-grub";
    config.mode_count = 2;
    config.preferred_mode = 0;
    config.primary = 1;
    config.modes[0] = {64, 32, 60, 64u * 4u, RAD_PIXEL_FORMAT_XRGB8888};
    config.modes[1] = {32, 16, 60, 32u * 4u, RAD_PIXEL_FORMAT_XRGB8888};
    rad_framebuffer_ops_t ops{};
    ops.context = &fake;
    ops.flush = fakeFramebufferFlush;
    ops.set_mode = fakeFramebufferSetMode;
    ops.blank = fakeFramebufferBlank;
    ops.get_vsync_counter = fakeFramebufferVsync;

    bool ok = expect(rad_framebuffer_register_ex(&config, &ops) == RAD_STATUS_OK, "framebuffer should register");
    rad_framebuffer_info_t secondInfo{};
    secondInfo.size = sizeof(secondInfo);
    secondInfo.width = 32;
    secondInfo.height = 16;
    secondInfo.stride_bytes = 32u * 2u;
    secondInfo.pixel_format = RAD_PIXEL_FORMAT_RGB565;
    secondInfo.pixels = secondPixels;
    rad_framebuffer_ops_t secondOps{};
    secondOps.context = &secondFake;
    secondOps.flush = fakeFramebufferFlush;
    ok &= expect(rad_framebuffer_register("/dev/fb-secondary", &secondInfo, &secondOps) == RAD_STATUS_OK,
        "secondary framebuffer should register through legacy API");
    rad_framebuffer_t fb = nullptr;
    ok &= expect(rad_framebuffer_open("/dev/fb-test", &fb) == RAD_STATUS_OK, "framebuffer should open");
    rad_framebuffer_t primary = nullptr;
    ok &= expect(rad_framebuffer_open_primary(&primary) == RAD_STATUS_OK, "primary framebuffer should open");
    rad_framebuffer_display_info_t display{};
    ok &= expect(rad_framebuffer_get_display_info(primary, &display) == RAD_STATUS_OK
        && std::string(display.name) == "/dev/fb-test"
        && display.output_type == RAD_DISPLAY_OUTPUT_GRUB
        && std::string(display.connector) == "qemu-grub"
        && display.mode_count == 2
        && display.primary == 1,
        "framebuffer display metadata should query");
    rad_framebuffer_close(primary);
    rad_framebuffer_info_t queried{};
    ok &= expect(rad_framebuffer_get_info(fb, &queried) == RAD_STATUS_OK && queried.width == 64 && queried.height == 32,
        "framebuffer info should query");
    ok &= expect(rad_framebuffer_set_mode(fb, 1) == RAD_STATUS_OK && fake.setModes == 1,
        "framebuffer set mode should call backend");
    ok &= expect(rad_framebuffer_get_info(fb, &queried) == RAD_STATUS_OK && queried.width == 32 && queried.height == 16,
        "framebuffer set mode should update active dimensions");
    ok &= expect(rad_framebuffer_set_mode(fb, 0) == RAD_STATUS_OK && fake.setModes == 2,
        "framebuffer should switch back to preferred mode");
    ok &= expect(rad_framebuffer_blank(fb, 1) == RAD_STATUS_OK && fake.blanks == 1 && fake.blanked == 1,
        "framebuffer blank should call backend");
    uint64_t vsync = 0;
    ok &= expect(rad_framebuffer_get_vsync_counter(fb, &vsync) == RAD_STATUS_OK && vsync == fake.vsync,
        "framebuffer vsync counter should query backend");
    const auto overlayRoot = std::filesystem::temp_directory_path() / "radixkernel-fb-overlay-test";
    std::filesystem::remove_all(overlayRoot);
    std::filesystem::create_directories(overlayRoot);
    const auto fbJsonPath = overlayRoot / "display.radoverlay.json";
    const auto fbBlobPath = overlayRoot / "display.radoverlay";
    {
        std::ofstream out(fbJsonPath);
        out << R"json({
  "name": "display-board",
  "fragments": [
    {
      "target": "/chosen/display",
      "properties": {
        "compatible": "rad,framebuffer-output",
        "framebuffer": "/dev/fb-secondary",
        "primary": true,
        "module": "rad_fb_binding"
      }
    }
  ]
})json";
    }
    const std::string fbOverlayCommand = "python3 tools/radoverlay_compile.py \"" + fbJsonPath.string() + "\" -o \"" + fbBlobPath.string() + "\"";
    ok &= expect(std::system(fbOverlayCommand.c_str()) == 0, "framebuffer radoverlay compiler should produce a blob");
    ok &= expect(rad_vfs_mount_host("/fbov", overlayRoot.string().c_str()) == RAD_STATUS_OK, "framebuffer overlay host path should mount");
    ok &= expect(rad_overlay_load_file("/fbov/display.radoverlay") == RAD_STATUS_OK, "framebuffer overlay should load");
    std::string bindTerminal;
    ok &= expect(rad_terminal_execute("bind", terminalWriter, &bindTerminal) == RAD_STATUS_OK, "bind command should refresh framebuffer overlays");
    primary = nullptr;
    ok &= expect(rad_framebuffer_open_primary(&primary) == RAD_STATUS_OK, "overlay-selected primary framebuffer should open");
    ok &= expect(rad_framebuffer_get_display_info(primary, &display) == RAD_STATUS_OK
        && std::string(display.name) == "/dev/fb-secondary",
        "framebuffer overlay should select secondary primary framebuffer");
    rad_framebuffer_close(primary);
    rad_vfs_unmount("/fbov");
    ok &= expect(rad_framebuffer_set_primary("/dev/fb-test") == RAD_STATUS_OK, "primary framebuffer should switch back");
    ok &= expect(rad_framebuffer_clear(fb, 0x00112233u) == RAD_STATUS_OK, "framebuffer clear should succeed");
    ok &= expect(pixels[0] == 0xff112233u && pixels[63] == 0xff112233u, "framebuffer clear should fill pixels");
    ok &= expect(rad_framebuffer_draw_text(fb, 0, 0, "RAD\nFB", 0x00ffffffu, 0x00000000u) == RAD_STATUS_OK,
        "framebuffer text should draw");
    bool sawTextPixel = false;
    for (uint32_t pixel : pixels) {
        if (pixel == 0xffffffffu) {
            sawTextPixel = true;
            break;
        }
    }
    ok &= expect(sawTextPixel, "framebuffer text should write foreground pixels");
    rad_framebuffer_rect_t rect{};
    rect.width = queried.width;
    rect.height = queried.height;
    ok &= expect(rad_framebuffer_flush(fb, &rect) == RAD_STATUS_OK && fake.flushes == 1, "framebuffer flush should call backend");

    rad_device_t device = nullptr;
    ok &= expect(rad_device_open("/dev/fb-test", &device) == RAD_STATUS_OK, "framebuffer device should open");
    rad_framebuffer_info_t ioctlInfo{};
    ok &= expect(rad_device_ioctl(device, RAD_DEVICE_IOCTL_FRAMEBUFFER_INFO, &ioctlInfo) == RAD_STATUS_OK && ioctlInfo.pixels == pixels,
        "framebuffer ioctl info should work");
    ok &= expect(rad_device_ioctl(device, RAD_DEVICE_IOCTL_FRAMEBUFFER_FLUSH, &rect) == RAD_STATUS_OK && fake.flushes == 2,
        "framebuffer ioctl flush should work");
    rad_device_close(device);

    char names[4][64]{};
    rad_framebuffer_info_t infos[4]{};
    ok &= expect(rad_framebuffer_list(infos, names, 4) >= 2, "framebuffer list should report registered framebuffers");
    rad_framebuffer_display_info_t displays[4]{};
    ok &= expect(rad_framebuffer_list_ex(displays, 4) >= 2 && std::string(displays[0].name) == "/dev/fb-test",
        "extended framebuffer list should report display metadata");
    std::string terminal;
    ok &= expect(rad_terminal_execute("fb", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("/dev/fb-test") != std::string::npos
        && terminal.find("output=grub") != std::string::npos
        && terminal.find("primary=1") != std::string::npos, "fb terminal command should list framebuffer metadata");

    rad_framebuffer_close(fb);
    ok &= expect(rad_framebuffer_unregister("/dev/fb-test") == RAD_STATUS_OK, "framebuffer should unregister");
    ok &= expect(rad_framebuffer_unregister("/dev/fb-secondary") == RAD_STATUS_OK, "secondary framebuffer should unregister");
    return ok;
}

bool testOverlayAndI2cCore() {
    const auto root = std::filesystem::temp_directory_path() / "radixkernel-overlay-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto jsonPath = root / "sensor.radoverlay.json";
    const auto blobPath = root / "sensor.radoverlay";
    {
        std::ofstream out(jsonPath);
        out << R"json({
  "name": "sensor-board",
  "compatible": "rad,board-v1",
  "boot": {
    "console": "uart0",
    "flash": {"kind": "xip-qspi", "size": 16777216},
    "memory": [{"name": "sram", "base": 536870912, "size": 532480}],
    "pinmux": [{"pin": 0, "function": "uart0-tx"}]
  },
  "fragments": [
    {
      "target": "/soc/interrupt-controller@20",
      "properties": {
        "name": "test-pic",
        "compatible": "rad,test-pic",
        "interrupt-controller": true,
        "#interrupt-cells": 2,
        "interrupt-base": 32,
        "interrupt-count": 16
      }
    },
    {
      "target": "/soc/i2c@0",
      "properties": {
        "status": "okay",
        "bus-id": 0,
        "sda-gpio": 4,
        "scl-gpio": 5,
        "clock-frequency": 400000
      },
      "children": {
        "temp@48": {
          "compatible": "ti,tmp102",
          "reg": 72,
          "interrupt-parent": "/soc/interrupt-controller@20",
          "interrupts": [5, 1],
          "module": "rad_tmp102"
        }
      }
    }
  ]
})json";
    }
    const std::string command = "python3 tools/radoverlay_compile.py \"" + jsonPath.string() + "\" -o \"" + blobPath.string() + "\"";
    bool ok = expect(std::system(command.c_str()) == 0, "radoverlay compiler should produce a blob");
    ok &= expect(std::filesystem::exists(blobPath), "compiled radoverlay should exist");
    ok &= expect(rad_vfs_mount_host("/ov", root.string().c_str()) == RAD_STATUS_OK, "overlay host path should mount");
    ok &= expect(rad_overlay_load_file("/ov/sensor.radoverlay") == RAD_STATUS_OK, "overlay blob should load from VFS");

    rad_tree_node_t i2cNode = rad_tree_find("/soc/i2c@0");
    ok &= expect(i2cNode != nullptr, "runtime tree should include I2C controller node");
    uint32_t busId = 99;
    ok &= expect(rad_tree_get_property_u32(i2cNode, "bus-id", &busId) == RAD_STATUS_OK && busId == 0,
        "I2C controller bus-id property should decode");
    char flashKind[32]{};
    ok &= expect(rad_tree_get_property_string(rad_tree_find("/boot/flash"), "kind", flashKind, sizeof(flashKind)) == RAD_STATUS_OK
        && std::string(flashKind) == "xip-qspi", "boot metadata should be preserved in tree");
    uint32_t interrupts[4]{};
    ok &= expect(rad_tree_get_property_u32_array(rad_tree_find("/soc/i2c@0/temp@48"), "interrupts", interrupts, 4) == 2
        && interrupts[0] == 5 && interrupts[1] == 1, "interrupt cells should decode from overlay tree");
    rad_irq_resource_t resolved[2]{};
    ok &= expect(rad_irq_resolve_tree(rad_tree_find("/soc/i2c@0/temp@48"), resolved, 2) == 1
        && resolved[0].irq == 37 && resolved[0].line == 5
        && resolved[0].trigger == RAD_IRQ_TRIGGER_EDGE_RISING
        && std::string(resolved[0].domain) == "test-pic", "interrupt resource should resolve through IRQ domain");
    FakeIrq treeIrq;
    ok &= expect(rad_irq_resource_register_handler(&resolved[0], "tmp102-alert", fakeIrqHandler, &treeIrq) == RAD_STATUS_OK,
        "resolved IRQ resource should register a handler");
    ok &= expect(rad_irq_resource_enable(&resolved[0]) == RAD_STATUS_OK, "resolved IRQ resource should enable");
    ok &= expect(rad_irq_dispatch(resolved[0].irq) == RAD_STATUS_OK && treeIrq.calls == 1 && treeIrq.lastIrq == 37,
        "resolved IRQ resource handler should dispatch");
    ok &= expect(rad_irq_resource_disable(&resolved[0]) == RAD_STATUS_OK, "resolved IRQ resource should disable");
    ok &= expect(rad_irq_unregister(resolved[0].irq) == RAD_STATUS_OK, "resolved IRQ resource should unregister flat handler");

    FakeBus bus;
    rad_i2c_driver_t i2cDriver{};
    i2cDriver.size = sizeof(i2cDriver);
    i2cDriver.name = "rad_i2c_tmp102";
    i2cDriver.compatible = "ti,tmp102";
    i2cDriver.context = &bus;
    i2cDriver.probe = fakeI2cProbe;
    i2cDriver.remove = fakeI2cRemove;
    ok &= expect(rad_i2c_driver_register(&i2cDriver) == RAD_STATUS_OK, "I2C child driver should register");
    rad_i2c_controller_config_t config{};
    config.size = sizeof(config);
    config.bus_id = 0;
    config.name = "sim-i2c0";
    config.tree_path = "/soc/i2c@0";
    config.clock_hz = 400000;
    config.sda_gpio = 4;
    config.scl_gpio = 5;
    rad_i2c_controller_ops_t ops{};
    ops.context = &bus;
    ops.transfer = fakeControllerTransfer;
    ok &= expect(rad_i2c_controller_register(&config, &ops) == RAD_STATUS_OK, "I2C controller should register");
    ok &= expect(rad_i2c_bind_tree() == RAD_STATUS_OK, "I2C binder should run");
    ok &= expect(bus.probes == 1, "I2C child probe should run");
    ok &= expect(bus.irqProbeCount == 1 && bus.irqLine == 5 && bus.irqNumber == 37,
        "I2C child probe should see resolved IRQ resource");
    rad_tree_node_info_t nodes[32]{};
    const size_t nodeCount = rad_tree_list(nodes, 32);
    bool foundBoundTemp = false;
    for (size_t i = 0; i < nodeCount && i < 32; ++i) {
        if (std::string(nodes[i].path) == "/soc/i2c@0/temp@48") {
            foundBoundTemp = nodes[i].bound == 1 && std::string(nodes[i].module) == "rad_tmp102";
        }
    }
    ok &= expect(foundBoundTemp, "I2C child should bind to registered bus");
    rad_i2c_device_t* i2cDevice = nullptr;
    ok &= expect(rad_i2c_device_open(0, 0x48, &i2cDevice) == RAD_STATUS_OK, "I2C child device should open");
    rad_irq_resource_t openedIrq{};
    ok &= expect(rad_i2c_device_get_irq(i2cDevice, 0, &openedIrq) == RAD_STATUS_OK && openedIrq.irq == 37,
        "I2C child IRQ accessor should return resolved resource");

    rad_i2c_bus_t i2cBus = nullptr;
    ok &= expect(rad_i2c_bus_open(0, &i2cBus) == RAD_STATUS_OK, "I2C bus should open");
    uint8_t writeData[1] = {0x33};
    uint8_t readData[2] = {};
    rad_i2c_transfer_t transfer{};
    transfer.address = 0x48;
    transfer.write_data = writeData;
    transfer.write_size = sizeof(writeData);
    transfer.read_data = readData;
    transfer.read_size = sizeof(readData);
    ok &= expect(rad_i2c_transfer(i2cBus, &transfer) == RAD_STATUS_OK, "I2C core transfer should dispatch");
    ok &= expect(rad_i2c_device_transfer(i2cDevice, &transfer) == RAD_STATUS_OK, "I2C child transfer should dispatch");
    ok &= expect(bus.lastWrite.size() == 1 && bus.lastWrite[0] == 0x33, "I2C core should capture write");
    ok &= expect(readData[0] == bus.response && readData[1] == bus.response, "I2C core should fill read");
    rad_i2c_device_close(i2cDevice);
    rad_i2c_bus_close(i2cBus);

    ok &= expect(RAD_IOCTL_TYPE(RAD_DEVICE_IOCTL_I2C_TRANSFER) == RAD_IOCTL_TYPE_I2C, "I2C ioctl type should encode");
    ok &= expect(RAD_IOCTL_DIR(RAD_DEVICE_IOCTL_I2C_TRANSFER) == RAD_IOCTL_READWRITE, "I2C ioctl direction should encode");
    ok &= expect(RAD_IOCTL_SIZE(RAD_DEVICE_IOCTL_I2C_TRANSFER) == sizeof(rad_i2c_transfer_t), "I2C ioctl size should encode");

    std::string terminal;
    ok &= expect(rad_terminal_execute("tree", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("/soc/i2c@0/temp@48") != std::string::npos, "tree terminal command should show overlay node");
    terminal.clear();
    ok &= expect(rad_terminal_execute("overlays", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("sensor-board") != std::string::npos, "overlays terminal command should show loaded overlay");
    terminal.clear();
    ok &= expect(rad_terminal_execute("drivers", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("rad_i2c_tmp102") != std::string::npos, "drivers command should list I2C child driver");
    terminal.clear();
    ok &= expect(rad_terminal_execute("irq-tree", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("test-pic") != std::string::npos
        && terminal.find("irq=37") != std::string::npos, "irq-tree command should list domain and resource");
    terminal.clear();
    ok &= expect(rad_terminal_execute("i2c", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("/soc/i2c@0/temp@48") != std::string::npos, "i2c command should list bound child");

    ok &= expect(rad_overlay_load_memory("bad", 3) == RAD_STATUS_INVALID_ARGUMENT, "invalid overlay memory should reject");
    rad_i2c_controller_unregister(0);
    ok &= expect(bus.removes == 1, "I2C child remove should run");
    rad_i2c_driver_unregister("rad_i2c_tmp102");
    rad_vfs_unmount("/ov");
    std::filesystem::remove_all(root);
    return ok;
}

bool testSpiAndDmaCore() {
    const auto root = std::filesystem::temp_directory_path() / "radixkernel-spi-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto jsonPath = root / "spi.radoverlay.json";
    const auto blobPath = root / "spi.radoverlay";
    {
        std::ofstream out(jsonPath);
        out << R"json({
  "name": "spi-board",
  "fragments": [
    {
      "target": "/soc/spi@0",
      "properties": {
        "status": "okay",
        "bus-id": 0,
        "clock-frequency": 24000000
      },
      "children": {
        "flash@0": {
          "compatible": "jedec,w25qxx",
          "cs": 0,
          "clock-frequency": 12000000,
          "mode": 0,
          "bits-per-word": 8,
          "interrupt-parent": "/soc/interrupt-controller@20",
          "interrupts": [6, 4],
          "transfer-mode": "auto"
        },
        "display@1": {
          "compatible": "ilitek,ili9341",
          "cs": 1,
          "clock-frequency": 32000000,
          "mode": 3,
          "bits-per-word": 8,
          "transfer-mode": "pio"
        }
      }
    }
  ]
})json";
    }
    const std::string command = "python3 tools/radoverlay_compile.py \"" + jsonPath.string() + "\" -o \"" + blobPath.string() + "\"";
    bool ok = expect(std::system(command.c_str()) == 0, "SPI radoverlay compiler should produce a blob");
    ok &= expect(rad_vfs_mount_host("/spiov", root.string().c_str()) == RAD_STATUS_OK, "SPI overlay host path should mount");
    ok &= expect(rad_overlay_load_file("/spiov/spi.radoverlay") == RAD_STATUS_OK, "SPI overlay blob should load");

    FakeDma dma;
    rad_dma_controller_config_t dmaConfig{};
    dmaConfig.size = sizeof(dmaConfig);
    dmaConfig.name = "rad_dma_fake";
    dmaConfig.channel_count = 4;
    rad_dma_backend_ops_t dmaOps{};
    dmaOps.context = &dma;
    dmaOps.request = fakeDmaRequest;
    dmaOps.release = fakeDmaRelease;
    dmaOps.submit = fakeDmaSubmit;
    dmaOps.wait = fakeDmaWait;
    dmaOps.cancel = fakeDmaCancel;
    ok &= expect(rad_dma_controller_register(&dmaConfig, &dmaOps) == RAD_STATUS_OK, "DMA controller should register");

    rad_dma_channel_t channel = nullptr;
    uint8_t src[3] = {1, 2, 3};
    uint8_t dst[3] = {};
    ok &= expect(rad_dma_channel_request(RAD_DMA_DREQ_NONE, &channel) == RAD_STATUS_OK, "DMA channel should request");
    rad_dma_transfer_t memcopy{};
    memcopy.type = RAD_DMA_MEMORY_TO_MEMORY;
    memcopy.source = src;
    memcopy.destination = dst;
    memcopy.size = sizeof(src);
    ok &= expect(rad_dma_submit(channel, &memcopy) == RAD_STATUS_OK, "DMA memcopy should submit");
    ok &= expect(rad_dma_wait(channel, 100) == RAD_STATUS_OK, "DMA memcopy should complete");
    ok &= expect(dst[0] == 1 && dst[2] == 3, "DMA memcopy should copy bytes");
    dma.timeoutNext = true;
    ok &= expect(rad_dma_submit(channel, &memcopy) == RAD_STATUS_OK, "DMA timeout transfer should submit");
    ok &= expect(rad_dma_wait(channel, 1) == RAD_STATUS_TIMEOUT, "DMA wait should report timeout");
    ok &= expect(rad_dma_cancel(channel) == RAD_STATUS_OK, "DMA cancel should succeed");
    rad_dma_channel_release(channel);

    FakeSpi spi;
    rad_spi_driver_t flashDriver{};
    flashDriver.size = sizeof(flashDriver);
    flashDriver.name = "rad_spi_w25qxx";
    flashDriver.compatible = "jedec,w25qxx";
    flashDriver.context = &spi;
    flashDriver.probe = fakeSpiProbe;
    flashDriver.remove = fakeSpiRemove;
    ok &= expect(rad_spi_driver_register(&flashDriver) == RAD_STATUS_OK, "SPI flash driver should register");
    rad_spi_driver_t displayDriver{};
    displayDriver.size = sizeof(displayDriver);
    displayDriver.name = "rad_spi_ili9341";
    displayDriver.compatible = "ilitek,ili9341";
    displayDriver.context = &spi;
    displayDriver.probe = fakeSpiProbe;
    displayDriver.remove = fakeSpiRemove;
    ok &= expect(rad_spi_driver_register(&displayDriver) == RAD_STATUS_OK, "SPI display driver should register");

    rad_spi_controller_config_t spiConfig{};
    spiConfig.size = sizeof(spiConfig);
    spiConfig.bus_id = 0;
    spiConfig.name = "rad_spi_fake";
    spiConfig.tree_path = "/soc/spi@0";
    spiConfig.clock_hz = 24000000;
    rad_spi_controller_ops_t spiOps{};
    spiOps.context = &spi;
    spiOps.transfer = fakeSpiTransfer;
    spiOps.transfer_dma = fakeSpiDmaTransfer;
    ok &= expect(rad_spi_controller_register(&spiConfig, &spiOps) == RAD_STATUS_OK, "SPI controller should register");
    ok &= expect(spi.probes == 2, "SPI child probes should run");
    ok &= expect(spi.irqProbeCount == 1 && spi.irqLine == 6 && spi.irqNumber == 38,
        "SPI child probe should see resolved IRQ resource");

    rad_spi_device_t* flash = nullptr;
    ok &= expect(rad_spi_device_open(0, 0, &flash) == RAD_STATUS_OK, "SPI flash child should open");
    rad_irq_resource_t flashIrq{};
    ok &= expect(rad_spi_device_get_irq(flash, 0, &flashIrq) == RAD_STATUS_OK
        && flashIrq.irq == 38 && flashIrq.trigger == RAD_IRQ_TRIGGER_LEVEL_HIGH,
        "SPI child IRQ accessor should return resolved resource");
    uint8_t smallTx[2] = {0x9f, 0x00};
    uint8_t smallRx[2] = {};
    rad_spi_transfer_t small{};
    small.tx_data = smallTx;
    small.rx_data = smallRx;
    small.size = sizeof(smallTx);
    small.transfer_mode = RAD_SPI_TRANSFER_MODE_PIO;
    ok &= expect(rad_spi_device_transfer(flash, &small) == RAD_STATUS_OK, "SPI PIO transfer should dispatch");
    ok &= expect(spi.pioTransfers == 1 && smallRx[0] == static_cast<uint8_t>(0x9f ^ 0xffu), "SPI PIO should fill RX");

    uint8_t largeTx[80]{};
    uint8_t largeRx[80]{};
    for (size_t i = 0; i < sizeof(largeTx); ++i) largeTx[i] = static_cast<uint8_t>(i);
    rad_spi_transfer_t automatic{};
    automatic.tx_data = largeTx;
    automatic.rx_data = largeRx;
    automatic.size = sizeof(largeTx);
    automatic.transfer_mode = RAD_SPI_TRANSFER_MODE_AUTO;
    ok &= expect(rad_spi_device_transfer(flash, &automatic) == RAD_STATUS_OK, "SPI auto transfer should use DMA when available");
    ok &= expect(spi.dmaTransfers == 1 && largeRx[0] == 0xd5 && largeRx[79] == 0xd5, "SPI DMA should fill RX through DMA backend");
    rad_spi_device_close(flash);

    rad_spi_bus_t bus = nullptr;
    ok &= expect(rad_spi_bus_open(0, &bus) == RAD_STATUS_OK, "SPI bus should open");
    ok &= expect(rad_spi_transfer(bus, &small) == RAD_STATUS_OK, "SPI bus transfer should dispatch");
    rad_spi_bus_close(bus);

    rad_spi_device_info_t spiDevices[4]{};
    ok &= expect(rad_spi_list_devices(spiDevices, 4) >= 2, "SPI device list should report children");
    std::string terminal;
    ok &= expect(rad_terminal_execute("spi", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("/soc/spi@0/flash@0") != std::string::npos, "spi command should list flash");
    terminal.clear();
    ok &= expect(rad_terminal_execute("dma", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("rad_dma_fake") != std::string::npos, "dma command should list fake DMA channels");
    terminal.clear();
    ok &= expect(rad_terminal_execute("drivers", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("rad_spi_w25qxx") != std::string::npos, "drivers command should list SPI child driver");

    rad_spi_controller_unregister(0);
    ok &= expect(spi.removes == 2, "SPI child removes should run");
    rad_spi_driver_unregister("rad_spi_w25qxx");
    rad_spi_driver_unregister("rad_spi_ili9341");
    ok &= expect(rad_dma_controller_unregister("rad_dma_fake") == RAD_STATUS_OK, "DMA controller should unregister");
    rad_vfs_unmount("/spiov");
    std::filesystem::remove_all(root);
    return ok;
}

bool testServiceOverlayCore() {
    const auto root = std::filesystem::temp_directory_path() / "radixkernel-service-overlay-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto jsonPath = root / "services.radoverlay.json";
    const auto blobPath = root / "services.radoverlay";
    {
        std::ofstream out(jsonPath);
        out << R"json({
  "name": "service-board",
  "fragments": [
    {
      "target": "/services/rad-compositor",
      "properties": {
        "compatible": "rad,compositor",
        "status": "okay",
        "backend": "slint",
        "display": "/dev/fb0",
        "keyboard": "/dev/input/event0",
        "pointer": "/dev/input/event1",
        "terminal": "/dev/tty0",
        "autostart": true,
        "order": 50
      }
    }
  ]
})json";
    }

    FakeService service;
    rad_service_descriptor_t descriptor{};
    descriptor.size = sizeof(descriptor);
    descriptor.name = "RADCompositor";
    descriptor.compatible = "rad,compositor";
    descriptor.capability = "compositor";
    descriptor.default_order = 100;
    descriptor.context = &service;
    descriptor.start = fakeServiceStart;
    descriptor.stop = fakeServiceStop;
    descriptor.poll = fakeServicePoll;

    bool ok = expect(rad_service_register(&descriptor) == RAD_STATUS_OK, "service should register");
    const std::string command = "python3 tools/radoverlay_compile.py \"" + jsonPath.string() + "\" -o \"" + blobPath.string() + "\"";
    ok &= expect(std::system(command.c_str()) == 0, "service radoverlay compiler should produce a blob");
    ok &= expect(rad_vfs_mount_host("/svcov", root.string().c_str()) == RAD_STATUS_OK, "service overlay host path should mount");
    ok &= expect(rad_overlay_load_file("/svcov/services.radoverlay") == RAD_STATUS_OK, "service overlay should load");

    std::string terminal;
    ok &= expect(rad_terminal_execute("bind", terminalWriter, &terminal) == RAD_STATUS_OK, "bind command should configure services");
    ok &= expect(service.starts == 1, "autostart service should start once");
    ok &= expect(service.backend == "slint" && service.display == "/dev/fb0" && service.keyboard == "/dev/input/event0"
        && service.pointer == "/dev/input/event1" && service.terminal == "/dev/tty0" && service.order == 50,
        "service start should receive overlay config");

    ok &= expect(rad_service_poll_all() == RAD_STATUS_OK && service.polls == 1, "running service should poll");
    rad_service_info_t services[4]{};
    const size_t count = rad_service_list(services, 4);
    bool listed = false;
    for (size_t i = 0; i < count && i < 4; ++i) {
        if (std::string(services[i].name) == "RADCompositor"
            && services[i].state == RAD_SERVICE_RUNNING
            && std::string(services[i].backend) == "slint"
            && services[i].autostart == 1
            && services[i].order == 50) {
            listed = true;
        }
    }
    ok &= expect(listed, "service list should expose configured running service");

    terminal.clear();
    ok &= expect(rad_terminal_execute("services", terminalWriter, &terminal) == RAD_STATUS_OK
        && terminal.find("RADCompositor") != std::string::npos
        && terminal.find("state=running") != std::string::npos
        && terminal.find("backend=slint") != std::string::npos,
        "services command should list configured service");

    ok &= expect(rad_service_stop("RADCompositor") == RAD_STATUS_OK && service.stops == 1, "service should stop");
    ok &= expect(rad_service_unregister("RADCompositor") == RAD_STATUS_OK, "service should unregister");
    rad_vfs_unmount("/svcov");
    std::filesystem::remove_all(root);
    return ok;
}

bool testSerialTerminal() {
    FakeSerial serial;
    serial.input = "time\n";
    rad_device_ops_t ops{};
    ops.context = &serial;
    ops.read = fakeSerialRead;
    ops.write = fakeSerialWrite;
    ops.ioctl = fakeSerialIoctl;
    bool ok = expect(rad_device_register("/dev/ttyS0", RAD_DEVICE_SERIAL, &ops) == RAD_STATUS_OK, "serial should register");
    rad_device_t device = nullptr;
    ok &= expect(rad_device_open("/dev/ttyS0", &device) == RAD_STATUS_OK, "serial should open");
    rad_serial_config_t serialConfig{};
    serialConfig.baud_rate = 115200;
    serialConfig.data_bits = 8;
    serialConfig.stop_bits = 1;
    ok &= expect(rad_serial_configure(device, &serialConfig) == RAD_STATUS_OK, "serial configure should dispatch");
    rad_device_close(device);
    ok &= expect(rad_terminal_attach_device("/dev/ttyS0") == RAD_STATUS_OK, "terminal should attach to serial");
    ok &= expect(rad_terminal_poll_attached() == RAD_STATUS_OK, "terminal serial poll should run");
    ok &= expect(serial.output.find("ms") != std::string::npos, "serial terminal should execute command");
    ok &= expect(rad_device_unregister("/dev/ttyS0") == RAD_STATUS_OK, "serial should unregister");
    return ok;
}

bool testTtyAndPtyCore() {
    bool ok = true;
    rad_tty_t console = nullptr;
    ok &= expect(rad_tty_open("/dev/console", &console) == RAD_STATUS_OK, "default console TTY should open");
    std::string echoed;
    ok &= expect(rad_tty_set_output_callback(console, ttyOutput, &echoed) == RAD_STATUS_OK,
        "TTY output callback should install");
    size_t done = 0;
    ok &= expect(rad_tty_push_input(console, "abc\bde\n", 7, &done) == RAD_STATUS_OK && done == 7,
        "TTY canonical input should consume input");
    char line[32]{};
    ok &= expect(rad_tty_read(console, line, sizeof(line), &done) == RAD_STATUS_OK, "TTY read should succeed");
    ok &= expect(std::string(line, done) == "abde\n", "TTY should cook backspace and newline");
    ok &= expect(echoed.find("abc\b \bde\n") != std::string::npos, "TTY should echo canonical edits");

    rad_tty_window_size_t size{40, 100};
    ok &= expect(rad_tty_set_window_size(console, &size) == RAD_STATUS_OK, "TTY resize should succeed");
    rad_tty_window_size_t readSize{};
    ok &= expect(rad_tty_get_window_size(console, &readSize) == RAD_STATUS_OK
        && readSize.rows == 40 && readSize.columns == 100, "TTY size should round trip");
    ok &= expect(rad_tty_set_mode(console, 0) == RAD_STATUS_OK, "TTY raw mode should set");
    uint32_t mode = 0;
    ok &= expect(rad_tty_get_mode(console, &mode) == RAD_STATUS_OK && mode == 0, "TTY mode should round trip");
    ok &= expect(rad_tty_push_input(console, "xy", 2, &done) == RAD_STATUS_OK, "TTY raw input should push bytes");
    std::memset(line, 0, sizeof(line));
    ok &= expect(rad_tty_read(console, line, sizeof(line), &done) == RAD_STATUS_OK
        && std::string(line, done) == "xy", "TTY raw input should read immediately");
    ok &= expect(rad_tty_set_mode(console, RAD_TTY_MODE_CANONICAL | RAD_TTY_MODE_ECHO | RAD_TTY_MODE_CRLF) == RAD_STATUS_OK,
        "TTY canonical mode should restore");
    rad_tty_close(console);

    rad_pty_t pty = nullptr;
    ok &= expect(rad_pty_open_pair("test-pty", &pty) == RAD_STATUS_OK, "PTY pair should open");
    char slaveName[64]{};
    ok &= expect(rad_pty_slave_name(pty, slaveName, sizeof(slaveName)) == RAD_STATUS_OK
        && std::string(slaveName).find("/dev/pts/") == 0, "PTY slave name should be exposed");
    ok &= expect(rad_pty_write_master(pty, "help\n", 5, &done) == RAD_STATUS_OK && done == 5,
        "PTY master write should feed slave TTY input");
    std::memset(line, 0, sizeof(line));
    ok &= expect(rad_pty_read_slave(pty, line, sizeof(line), &done) == RAD_STATUS_OK
        && std::string(line, done) == "help\n", "PTY slave should read cooked input");
    ok &= expect(rad_pty_write_slave(pty, "ready\n", 6, &done) == RAD_STATUS_OK && done == 6,
        "PTY slave output should write to master");
    std::memset(line, 0, sizeof(line));
    ok &= expect(rad_pty_read_master(pty, line, sizeof(line), &done) == RAD_STATUS_OK
        && std::string(line, done).find("ready\n") != std::string::npos, "PTY master should read slave output");
    rad_tty_window_size_t ptySize{24, 80};
    ok &= expect(rad_pty_resize(pty, &ptySize) == RAD_STATUS_OK, "PTY resize should succeed");
    rad_tty_t slave = nullptr;
    ok &= expect(rad_tty_open(slaveName, &slave) == RAD_STATUS_OK, "PTY slave TTY should open by name");
    readSize = {};
    ok &= expect(rad_tty_get_window_size(slave, &readSize) == RAD_STATUS_OK
        && readSize.rows == 24 && readSize.columns == 80, "PTY resize should update slave TTY size");
    ok &= expect(rad_pty_write_master(pty, "time\n", 5, &done) == RAD_STATUS_OK && done == 5,
        "PTY master should feed shell command input");
    ok &= expect(rad_terminal_poll_tty(slave) == RAD_STATUS_OK, "PTY slave should poll terminal shell");
    char output[128]{};
    ok &= expect(rad_pty_read_master(pty, output, sizeof(output), &done) == RAD_STATUS_OK
        && std::string(output, done).find("ms") != std::string::npos, "PTY shell output should return to master");
    rad_tty_close(slave);
    rad_pty_close(pty);
    return ok;
}

bool testIrqCore() {
    FakeIrq fake;
    bool ok = expect(rad_irq_register(7, "fake-irq", fakeIrqHandler, &fake) == RAD_STATUS_OK,
        "IRQ handler should register");
    ok &= expect(rad_irq_register(7, "fake-irq-dup", fakeIrqHandler, &fake) == RAD_STATUS_ALREADY_EXISTS,
        "duplicate IRQ handler should be rejected");
    ok &= expect(rad_irq_dispatch(7) == RAD_STATUS_NOT_SUPPORTED,
        "disabled IRQ dispatch should be reported");
    ok &= expect(rad_irq_enable(7) == RAD_STATUS_OK, "IRQ should enable");
    ok &= expect(rad_irq_dispatch(7) == RAD_STATUS_OK, "enabled IRQ should dispatch");
    ok &= expect(fake.calls == 1 && fake.lastIrq == 7, "IRQ handler should run with IRQ number");

    rad_irq_info_t irqs[4]{};
    const size_t count = rad_irq_list(irqs, 4);
    bool listed = false;
    for (size_t i = 0; i < count && i < 4; ++i) {
        if (irqs[i].irq == 7 && std::string(irqs[i].name) == "fake-irq"
            && irqs[i].enabled && irqs[i].count == 1 && irqs[i].unhandled_count == 1) {
            listed = true;
        }
    }
    ok &= expect(listed, "IRQ list should expose enabled handler and counters");

    std::string terminalOut;
    ok &= expect(rad_terminal_execute("irq", terminalWriter, &terminalOut) == RAD_STATUS_OK,
        "irq terminal command should execute");
    ok &= expect(terminalOut.find("fake-irq") != std::string::npos,
        "irq terminal command should list registered handler");
    ok &= expect(rad_irq_disable(7) == RAD_STATUS_OK, "IRQ should disable");
    ok &= expect(rad_irq_unregister(7) == RAD_STATUS_OK, "IRQ should unregister");
    ok &= expect(rad_irq_unregister(7) == RAD_STATUS_NOT_FOUND, "missing IRQ unregister should fail");
    return ok;
}

bool testA53PlatformCore() {
    rad_process_set_current_pid(1);
    rad_a53_note_boot_normalized(0, 0x80000u, 1);
    bool ok = expect(rad_a53_platform_init() == RAD_STATUS_OK, "A53 platform init should succeed");
    ok &= expect(rad_a53_process_arch_init() == RAD_STATUS_OK, "A53 process arch should register");
    rad_a53_capabilities_t caps{};
    caps.size = sizeof(caps);
    ok &= expect(rad_a53_get_capabilities(&caps) == RAD_STATUS_OK
            && caps.boot_normalized
            && caps.secondary_cores_parked
            && caps.exception_vectors_ready
            && caps.svc_ready
            && caps.user_copy_ready
            && caps.fork_ready
            && caps.exec_ready
            && caps.cow_ready
            && caps.mmu_ready
            && caps.ttbr0_user_ready
            && caps.ttbr1_kernel_ready
            && caps.page_size == 4096u,
        "A53 capabilities should report boot, MMU, SVC, user-copy, fork, exec, and COW readiness");
    ok &= expect(rad_a53_vm_self_test() == 1, "A53 VM COW self-test should isolate parent and child pages");
    ok &= expect(rad_a53_process_self_test() == RAD_STATUS_OK, "A53 process self-test should fork, exec-mark, exit, and wait");
    return ok;
}

} // namespace

int main() {
    bool ok = true;
    ok &= testKernelLifecycle();
    ok &= testBootInfoAndRunLoop();
    ok &= testPiHandoffAbi();
    ok &= testTasksAndEvents();
    ok &= testPerfWorkAndWaitQueues();
    ok &= testMutexAndMemory();
    ok &= testCppAndModuleCore();
    ok &= testVfsAndTerminal();
    ok &= testDeviceRegistry();
    ok &= testInputCore();
    ok &= testBlockCore();
    ok &= testNetCore();
    ok &= testUdpSocketCore();
    ok &= testTcpSocketCore();
    ok &= testPosixSyscallCore();
    ok &= testFramebufferCore();
    ok &= testOverlayAndI2cCore();
    ok &= testSpiAndDmaCore();
    ok &= testServiceOverlayCore();
    ok &= testTtyAndPtyCore();
    ok &= testIrqCore();
    ok &= testA53PlatformCore();
    ok &= testSerialTerminal();
    rad_kernel_shutdown();
    if (ok) {
        std::cout << "[RADixKernelTests] PASS\n";
        return 0;
    }
    return 1;
}
