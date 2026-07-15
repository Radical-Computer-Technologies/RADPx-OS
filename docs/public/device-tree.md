# Overlay And Device Tree Guide {#device_tree_guide}

RADix-OS uses `.radoverlay` blobs as its runtime device tree format. The JSON
source is compiled by `tools/radoverlay_compile.py` and loaded through
`rad_overlay_load_file()` or `rad_overlay_load_memory()`.

## Overlay Shape

An overlay document may contain:

- `name` and `compatible` at the root for board identity.
- `boot`, `memory`, and other metadata objects that become runtime tree nodes.
- `fragments`, each with an absolute `target`, optional `properties`, and
  optional `children`.

Supported property types are booleans, 32-bit integers, strings, arrays of
32-bit integers, and arrays of strings.

## Bus And Device Binding

Controller nodes use stable paths such as `/soc/i2c@0`, `/soc/spi@0`, or
`/soc/interrupt-controller@20`. Child nodes live below their parent bus:

```json
{
  "target": "/soc/i2c@0",
  "properties": {
    "status": "okay",
    "bus-id": 0,
    "clock-frequency": 400000
  },
  "children": {
    "temp@48": {
      "compatible": "ti,tmp102",
      "reg": 72,
      "interrupt-parent": "/soc/interrupt-controller@20",
      "interrupts": [5, 1]
    }
  }
}
```

I2C child binding uses `compatible`, `reg`, and the parent controller `bus-id`.
SPI child binding uses `compatible`, `cs`, `clock-frequency`, `mode`,
`bits-per-word`, and `transfer-mode`. Valid SPI transfer modes are `pio`, `dma`,
and `auto`.

## Interrupt Domains

An interrupt controller node becomes an IRQ domain when it has
`interrupt-controller`, `#interrupt-cells`, `interrupt-base`, and
`interrupt-count`. Child device `interrupts` arrays are decoded relative to the
domain. With two cells, the first value is the line and the second value is the
trigger flag.

Current trigger flags map to `rad_irq_trigger_t`: rising edge, falling edge,
level high, and level low.

## Framebuffer Outputs

Framebuffer overlays can choose the primary display by naming a registered
framebuffer. This lets x86 GRUB framebuffers, Pi framebuffers, RP2350 HSTX/DVI,
and future SPI panel backends share the same kernel framebuffer API.

```json
{
  "target": "/chosen/display",
  "properties": {
    "compatible": "rad,framebuffer-output",
    "framebuffer": "/dev/fb0",
    "primary": true
  }
}
```

## Boot Services

Overlays may seed early boot services under `/services`. This is intentionally a
small boot-time contract, not a systemd replacement. The long-term owner for
service launch policy is the external `RADServiceManager` first process.

```json
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
```

The kernel records service configuration, starts trusted early services when
requested, and exposes the `services` terminal command. Scheduling internals,
process policy, restart behavior, and dependency resolution stay outside the
overlay format for now.

## Current Limits

Crimson 0.2.0 overlays are a compact runtime binding format, not a full Linux
DTB replacement. They intentionally cover the current RADix buses, IRQ domains,
boot metadata, framebuffer selection, and boot-service seeds first.
