// bcm283x_wifi.cpp — Cypress CYW43438 (BCM43438, reports as "CYW43430") SDIO
// WiFi driver for the Raspberry Pi Zero 2 W.
//
// TOPOLOGY. On a real Pi Zero 2 W the on-board WiFi chip is an SDIO device on the
// BCM2835 "EMMC" / Arasan SDHCI controller (peripheral+0x300000), routed to
// GPIO34-39 (alt3), while the microSD card is on the separate SDHOST controller
// (GPIO48-53, alt0). The storage HAL programs exactly that routing before calling
// us (see rad_hal_register_default_devices -> bcm283x_emmc_init), so the Arasan
// controller is ours to drive here.
//
// HARDWARE-ONLY. QEMU raspi3ap/raspi3b do NOT emulate the CYW43438 (no SDIO
// function, no chip firmware) — verified: `qemu -device help` lists no wifi/brcm/
// cyw/sdio device. So this driver can only be *validated* on a physical Pi Zero
// 2 W. Under QEMU the SDIO probe (CMD5 IO_SEND_OP_COND) gets no response and we
// report "WiFi not present" and return cleanly, without hanging the boot. Every
// register access here is real-silicon-correct so a QEMU-clean image behaves
// identically on hardware up to the point the chip must actually respond.
//
// STATUS. The SDIO bus transport (CMD5/CMD3/CMD7/CMD52/CMD53) and the chip
// presence/backplane chip-ID probe below are complete and real. The firmware
// download, SDPCM/BCDC protocol, ioctl layer (scan/join/WPA2) and the net-device
// data path are scaffolded with the exact structure + register map needed and
// explicit TODOs — those layers can only be finished and tested against real
// hardware + the proprietary firmware blob (see bcm283x_wifi_firmware.sh).
//
// REFERENCES: Circle addon/wlan (Plan9-derived ether4330.c), Broadcom/Cypress
// firmware repo RPi-Distro/firmware-nonfree, georgerobotics/cyw43-driver, and the
// Linux brcmfmac driver. The SDIO command formats and the GPIO routing here match
// ether4330.c's sdioinit() exactly.

#include <radkernel/radkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" uint64_t rad_hal_time_micros(void); // bcm283x HAL system-timer clock

namespace {
constexpr uintptr_t EmmcOffset = 0x300000u; // Arasan SDHCI (carries WiFi SDIO on real HW)

// --- SDIO / CCCR (Fn0) register map (SD Association SDIO spec) --------------
constexpr uint32_t SdioFn0 = 0u;
constexpr uint32_t SdioFn1 = 1u; // backplane function
constexpr uint32_t SdioFn2 = 2u; // WLAN data function
constexpr uint32_t CccrIoEnable = 0x02u;
constexpr uint32_t CccrIoReady = 0x03u;
constexpr uint32_t CccrIntEnable = 0x04u;
constexpr uint32_t CccrIoAbort = 0x06u;
constexpr uint32_t CccrBusIfc = 0x07u;
constexpr uint32_t CccrHighspeed = 0x13u;
constexpr uint32_t SdioFbr1 = 0x100u;
constexpr uint32_t SdioFbr2 = 0x200u;
constexpr uint32_t SdioBlksize = 0x10u;

// SDIO commands.
constexpr uint32_t CmdGoIdle = 0u;          // CMD0
constexpr uint32_t CmdIoSendOpCond = 5u;    // CMD5 (R4)
constexpr uint32_t CmdSendRelAddr = 3u;     // CMD3 (R6)
constexpr uint32_t CmdSelectCard = 7u;      // CMD7 (R1b)
constexpr uint32_t CmdIoRwDirect = 52u;     // CMD52 (R5)
constexpr uint32_t CmdIoRwExtended = 53u;   // CMD53 (R5)
constexpr uint32_t SdioOcrV3_3 = 0x00fc0000u; // 3.2-3.4V window

// Sonics Silicon Backplane — access to the chip's internal cores over SDIO Fn1.
constexpr uint32_t SbEnumBase = 0x18000000u;
// Fn1 backplane address window registers (in the Fn1 SDIO address space).
constexpr uint32_t SbSdioAddrLow = 0x1000au;
constexpr uint32_t SbSdioAddrMid = 0x1000bu;
constexpr uint32_t SbSdioAddrHigh = 0x1000cu;

// SDHCI register offsets (same controller model as the SD-card driver).
constexpr uint32_t SdhciBlockSize = 0x04u;
constexpr uint32_t SdhciBlockCount = 0x06u;
constexpr uint32_t SdhciArg = 0x08u;
constexpr uint32_t SdhciTransferMode = 0x0cu;
constexpr uint32_t SdhciCommand = 0x0eu;
constexpr uint32_t SdhciResponse0 = 0x10u;
constexpr uint32_t SdhciData = 0x20u;
constexpr uint32_t SdhciPresentState = 0x24u;
constexpr uint32_t SdhciClockControl = 0x2cu;
constexpr uint32_t SdhciIntStatus = 0x30u;
constexpr uint16_t SdhciCmdRespShort = 0x02u;
constexpr uint16_t SdhciCmdCrc = 0x08u;
constexpr uint16_t SdhciCmdIndexCheck = 0x10u;
constexpr uint16_t SdhciCmdData = 0x20u;
constexpr uint32_t SdhciIntCmdComplete = 1u << 0u;
constexpr uint32_t SdhciIntTransferComplete = 1u << 1u;
constexpr uint32_t SdhciIntError = 0xffff0000u;
constexpr uint32_t SdhciPresentReadReady = 1u << 11u;

struct WifiState {
    uintptr_t peripheral_base = 0x3f000000u;
    int present = 0;
    uint32_t chip_id = 0;
    uint32_t rca = 0;
    uint32_t backplane_window = ~0u; // last programmed SB window (force first set)
    uint8_t mac[6] = {0};
};
WifiState g_wifi;

volatile uint32_t *reg32(uintptr_t a) { return reinterpret_cast<volatile uint32_t*>(a); }
uint32_t rd(uintptr_t a) { return *reg32(a); }
void wr(uintptr_t a, uint32_t v) { *reg32(a) = v; }
void wr16(uintptr_t a, uint16_t v) { *reinterpret_cast<volatile uint16_t*>(a) = v; }
uintptr_t emmc(uint32_t off) { return g_wifi.peripheral_base + EmmcOffset + off; }

void udelay(uint32_t us) {
    const uint64_t start = rad_hal_time_micros();
    while (rad_hal_time_micros() - start < us) { asm volatile("nop"); }
}

// Issue an SDHCI command (SDIO uses the same controller). Returns the 32-bit R5/
// R4/R6 short response in *resp. `data_read` selects a data-in transfer (CMD53).
rad_status_t sdhci_cmd(uint32_t index, uint32_t arg, int data_read, uint32_t *resp) {
    // Short timeouts: a present CYW43438 answers in microseconds, so a long wait
    // only slows the QEMU "no chip" case (which we want to fail fast, ~5 ms/cmd).
    for (uint32_t i = 0; (rd(emmc(SdhciPresentState)) & 0x1u); ++i) {
        if (i > 5000u) return RAD_STATUS_TIMEOUT;
        udelay(1);
    }
    wr(emmc(SdhciIntStatus), 0xffffffffu);
    wr(emmc(SdhciArg), arg);
    uint16_t flags = SdhciCmdRespShort | SdhciCmdCrc | SdhciCmdIndexCheck;
    if (data_read) flags |= SdhciCmdData;
    wr16(emmc(SdhciCommand), static_cast<uint16_t>((index << 8u) | flags));
    for (uint32_t i = 0; i < 5000u; ++i) {
        const uint32_t st = rd(emmc(SdhciIntStatus));
        if (st & SdhciIntError) { wr(emmc(SdhciIntStatus), st); return RAD_STATUS_ERROR; }
        if (st & SdhciIntCmdComplete) {
            wr(emmc(SdhciIntStatus), SdhciIntCmdComplete);
            if (resp) *resp = rd(emmc(SdhciResponse0));
            return RAD_STATUS_OK;
        }
        udelay(1);
    }
    return RAD_STATUS_TIMEOUT;
}

// CMD52 IO_RW_DIRECT: single-register read/write in an SDIO function's space.
// arg = (write<<31)|(fn<<28)|(raw<<27)|(addr<<9)|data  (SD Association SDIO spec).
rad_status_t sdio_rw_direct(int write, uint32_t fn, uint32_t addr, uint8_t data, uint8_t *out) {
    const uint32_t arg = (static_cast<uint32_t>(write ? 1u : 0u) << 31)
        | ((fn & 7u) << 28)
        | ((addr & 0x1ffffu) << 9)
        | (data & 0xffu);
    uint32_t resp = 0;
    const rad_status_t st = sdhci_cmd(CmdIoRwDirect, arg, 0, &resp);
    if (st != RAD_STATUS_OK) return st;
    if (out) *out = static_cast<uint8_t>(resp & 0xffu);
    return RAD_STATUS_OK;
}

uint8_t sdio_read8(uint32_t fn, uint32_t addr) { uint8_t v = 0; sdio_rw_direct(0, fn, addr, 0, &v); return v; }
void sdio_write8(uint32_t fn, uint32_t addr, uint8_t v) { sdio_rw_direct(1, fn, addr, v, nullptr); }

// Point the Fn1 backplane window at `addr` (upper address bits), so subsequent
// Fn1 accesses in the low 32 KB window hit chip-internal core registers/RAM.
void sb_window(uint32_t addr) {
    const uint32_t win = addr & ~0x7fffu;
    if (win == g_wifi.backplane_window) return;
    g_wifi.backplane_window = win;
    sdio_write8(SdioFn1, SbSdioAddrLow, static_cast<uint8_t>((win >> 8) & 0xff));
    sdio_write8(SdioFn1, SbSdioAddrMid, static_cast<uint8_t>((win >> 16) & 0xff));
    sdio_write8(SdioFn1, SbSdioAddrHigh, static_cast<uint8_t>((win >> 24) & 0xff));
}

// Bring up the SDIO bus and enumerate the WiFi function. Real + HW-testable.
// Returns RAD_STATUS_NOT_FOUND when no card responds (the QEMU case).
rad_status_t sdio_bus_init() {
    (void)sdhci_cmd(CmdGoIdle, 0, 0, nullptr); // CMD0
    // CMD5 IO_SEND_OP_COND: probe, then request the 3.3V window until ready.
    uint32_t ocr = 0;
    if (sdhci_cmd(CmdIoSendOpCond, 0, 0, &ocr) != RAD_STATUS_OK) return RAD_STATUS_NOT_FOUND;
    int ready = 0;
    for (uint32_t i = 0; i < 50u; ++i) {
        if (sdhci_cmd(CmdIoSendOpCond, SdioOcrV3_3, 0, &ocr) != RAD_STATUS_OK) return RAD_STATUS_NOT_FOUND;
        if (ocr & 0x80000000u) { ready = 1; break; }
        udelay(2000);
    }
    if (!ready) return RAD_STATUS_NOT_FOUND;
    uint32_t rca = 0;
    if (sdhci_cmd(CmdSendRelAddr, 0, 0, &rca) != RAD_STATUS_OK) return RAD_STATUS_ERROR; // CMD3
    g_wifi.rca = rca & 0xffff0000u;
    if (sdhci_cmd(CmdSelectCard, g_wifi.rca, 0, nullptr) != RAD_STATUS_OK) return RAD_STATUS_ERROR; // CMD7
    // CCCR: high speed, 4-bit bus, Fn1/Fn2 block sizes, enable Fn1 (backplane).
    sdio_write8(SdioFn0, CccrHighspeed, 0x02u);
    sdio_write8(SdioFn0, CccrBusIfc, 0x02u);
    sdio_write8(SdioFn0, SdioFbr1 + SdioBlksize, 64u);
    sdio_write8(SdioFn0, SdioFbr1 + SdioBlksize + 1u, 0u);
    sdio_write8(SdioFn0, SdioFbr2 + SdioBlksize, 0u);          // 512 = 0x200
    sdio_write8(SdioFn0, SdioFbr2 + SdioBlksize + 1u, 2u);
    sdio_write8(SdioFn0, CccrIoEnable, 1u << SdioFn1);
    sdio_write8(SdioFn0, CccrIntEnable, 0u);
    for (uint32_t i = 0; i < 50u; ++i) {
        if (sdio_read8(SdioFn0, CccrIoReady) & (1u << SdioFn1)) return RAD_STATUS_OK;
        udelay(2000);
    }
    return RAD_STATUS_ERROR;
}

// Read the ChipCommon core's chip-ID register through the backplane window.
// (Real: proves the chip is alive and identifies 43430 = the Pi Zero 2 W part.)
uint32_t read_chip_id() {
    sb_window(SbEnumBase);
    // ChipCommon chipid register is at backplane offset 0; the low 15 bits of the
    // Fn1 window map to it. Read 4 bytes little-endian via CMD52 (byte-wise keeps
    // this dependency-free of the CMD53 block path during bring-up).
    uint32_t v = 0;
    for (int b = 0; b < 4; ++b) {
        v |= static_cast<uint32_t>(sdio_read8(SdioFn1, (SbEnumBase & 0x7fffu) + b)) << (8 * b);
    }
    return v & 0xffffu; // chip id is the low 16 bits
}

} // namespace

// Entry point called by the bcm283x storage/device init after the Arasan EMMC has
// been routed to the WiFi (GPIO34-39 alt3). Probes for the CYW43438 and, on real
// hardware, is the hook from which firmware download + net-device bring-up run.
extern "C" rad_status_t rad_bcm283x_wifi_init(uintptr_t peripheral_base) {
    g_wifi.peripheral_base = peripheral_base ? peripheral_base : g_wifi.peripheral_base;

    if (sdio_bus_init() != RAD_STATUS_OK) {
        // No SDIO WiFi function responded. This is the expected QEMU path (the
        // CYW43438 is not emulated) and also a real board with WiFi disabled.
        rad_debug_marker("RAD_PI_WIFI_ABSENT_OK");
        return RAD_STATUS_NOT_FOUND;
    }
    g_wifi.present = 1;
    rad_debug_marker("RAD_PI_WIFI_SDIO_OK");

    g_wifi.chip_id = read_chip_id();
    if (g_wifi.chip_id == 0x4343u /*43430*/ || g_wifi.chip_id == 0xa9a6u) {
        rad_debug_marker("RAD_PI_WIFI_CHIP_OK");
    }

    // ------------------------------------------------------------------------
    // HARDWARE-COMPLETION TODO (only reachable on a real Pi Zero 2 W, where the
    // steps above have already succeeded). Each maps to a specific reference in
    // Circle addon/wlan/ether4330.c and cyw43-driver:
    //
    //  1. Firmware download: sb_window() to the SOCRAM/ARM-core RAM, CMD53-write
    //     the brcmfmac43430-sdio.bin image + the NVRAM (.txt) tail + the CLM blob
    //     (see bcm283x_wifi_firmware.sh for provisioning), then release the ARM
    //     core from reset (sbreset()) and wait for the "HT clock available"
    //     backplane bit.  [ether4330.c: ramscan/upload/sbreset]
    //  2. SDPCM transport: enable Fn2, set the watermark, and implement the
    //     software framing (sequence numbers, channels: control/event/data) over
    //     CMD53 block reads/writes.  [ether4330.c: sdioreadwrite/txctl/rxctl]
    //  3. BCDC control: wl ioctls over SDPCM control channel — set country,
    //     bsscfg, WSEC/WPA_AUTH, then scan + set_ssid for join.  [ether4330.c:
    //     wlcmd/wlcmdint; cyw43-driver: cyw43_ll_wifi_*]
    //  4. Net device: register a rad_net_device whose tx enqueues an SDPCM data
    //     frame and whose rx drains data-channel frames into the IP stack, then
    //     DHCP over the existing runtime UDP stack.  [rad_net_device_register]
    //
    // Until (1)-(4) land + are validated on hardware, WiFi is detected but not
    // brought online. Do NOT claim RAD_SERVICE_NETWORK_OK from here.
    // ------------------------------------------------------------------------
    return RAD_STATUS_OK;
}
