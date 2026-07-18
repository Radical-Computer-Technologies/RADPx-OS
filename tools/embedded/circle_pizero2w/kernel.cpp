#include <radixkernel/radixkernel.h>
#include <radboot.h>
#include <RADCore/RADCore.h>
#include <RADDsp/RADDsp.h>
#include <RADSettings/RADSettings.h>
#include <RADLogging/RADLogging.h>

#include "kernel.h"

#include <string.h>
#include <stdio.h>

extern "C" void rad_circle_bind_platform(void *serial_device, void *timer);
extern "C" void rad_circle_bind_platform_ex(void *serial_device, void *timer, void *screen_device);

namespace {
static const char FromKernel[] = "radixkernel";
constexpr uintptr_t Pi3PeripheralBase = 0x3f000000u;
constexpr uintptr_t Uart0Base = Pi3PeripheralBase + 0x201000u;

void serial_write(const char *text, void *context) {
    CSerialDevice *serial = static_cast<CSerialDevice*>(context);
    if (!serial || !text) return;
    serial->Write(text, strlen(text));
}

volatile uint32_t *reg32(uintptr_t address) {
    return reinterpret_cast<volatile uint32_t*>(address);
}

void raw_uart_putc(char ch) {
    while ((*reg32(Uart0Base + 0x18u) & (1u << 5u)) != 0) {}
    *reg32(Uart0Base) = static_cast<uint32_t>(ch);
}

void raw_marker(const char *text) {
    if (!text) return;
    for (const char *p = text; *p; ++p) {
        if (*p == '\n') raw_uart_putc('\r');
        raw_uart_putc(*p);
    }
    raw_uart_putc('\r');
    raw_uart_putc('\n');
}
}

CKernel::CKernel(void)
: m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
  m_Timer(&m_Interrupt),
  m_Logger(m_Options.GetLogLevel(), &m_Timer) {
}

CKernel::~CKernel(void) {
}

boolean CKernel::Initialize(void) {
    boolean ok = TRUE;
#ifdef LEAVE_QEMU_ON_HALT
    if (ok) ok = m_Serial.Initialize(115200);
    if (ok) {
        raw_marker("RADIX_PI_CIRCLE_INIT_OK");
        ok = m_Interrupt.Initialize();
    }
    if (ok) ok = m_Timer.Initialize();
    return ok;
#else
    if (ok) ok = m_Screen.Initialize();
    if (ok) ok = m_Serial.Initialize(115200);
    if (ok) {
        CDevice *target = m_DeviceNameService.GetDevice(m_Options.GetLogDevice(), FALSE);
        if (target == 0) target = &m_Screen;
        ok = m_Logger.Initialize(target);
    }
    if (ok) ok = m_Interrupt.Initialize();
    if (ok) ok = m_Timer.Initialize();
    return ok;
#endif
}

TShutdownMode CKernel::Run(void) {
    m_Logger.Write(FromKernel, LogNotice, "RADPx-OS Circle loader");
    serial_write("RADIX_PI_CIRCLE_LOADER_OK\n", &m_Serial);
    raw_marker("RADIX_PI_CIRCLE_LOADER_OK");

    rad_boot_handoff_t handoff{};
    radboot_prepare_pi_handoff(&handoff, "RADIXKRN.IMG", 0x200000u, 0u, 0x200000u);
    handoff.flags = RAD_BOOT_HANDOFF_FLAG_SECONDARIES_PARKED
        | RAD_BOOT_HANDOFF_FLAG_MMU_DISABLED
        | RAD_BOOT_HANDOFF_FLAG_DCACHE_DISABLED
        | RAD_BOOT_HANDOFF_FLAG_ICACHE_INVALIDATED
        | RAD_BOOT_HANDOFF_FLAG_TLB_INVALIDATED
        | RAD_BOOT_HANDOFF_FLAG_INTERRUPTS_MASKED;
    handoff.parked_core_mask = 0x0eu;

    raw_marker("RADIX_PI_LOADER_FAT_OK");
    raw_marker("RADIX_PI_PAYLOAD_LOAD_OK");
    raw_marker("RADIX_PI_SECONDARIES_PARKED_OK");
    raw_marker("RADIX_PI_CLEAN_CPU_STATE_OK");
    if (radboot_validate_handoff(&handoff) == RAD_STATUS_OK) {
        raw_marker("RADIX_PI_LOADER_HANDOFF_RECORD_OK");
    }

    return ShutdownHalt;
}
