# API Groups {#api_groups}

The Crimson API is organized around a small number of kernel subsystems. Use
this page as the human index, then jump to the generated file and member
references for exact signatures.

## Kernel And Boot

Use `rad_kernel_init()`, `rad_kernel_poll()`, `rad_kernel_run()`,
`rad_kernel_shutdown()`, `rad_printk()`, `rad_cpu_idle()`, and the
`rad_boot_info_t` helpers to bring a platform online and report early state.

## Time, Tasks, And Synchronization

Use `rad_time_micros()`, timer-source registration, `rad_task_create_config()`,
wait queues, mutexes, and events for in-kernel scheduling and coordination.
`rad_scheduler_info_get()` and `rad_core_info_get()` expose current scheduling
and multicore state.

## POSIX-Inspired Runtime

Use VFS handles for kernel code and integer file descriptors for the userspace
path. `rad_syscall_dispatch()` is the stable syscall dispatch table boundary for
the current x86 VM work.

## Device Model

Use `rad_device_register()` for generic device nodes and subsystem wrappers for
specialized devices: block, network, framebuffer, input, TTY, PTY, audio,
serial, and USB. Ioctl request macros follow a Linux-style
direction/type/number/size layout. The first USB surface is host-info discovery
for `/dev/usb0`, with HID devices expected to publish input events through the
generic input queues.

## Networking

The experimental network path registers `/dev/net0` on the x86 VM when
virtio-net is present, supports Ethernet frame send/receive operations, and
provides small IPv4/UDP datagram and local TCP stream socket subsets through
the POSIX-inspired syscall table. Full wire-level TCP/IP remains future work.

## Driver Binding

Modules advertise name, bus, and compatibility metadata. I2C and SPI controllers
register a bus, child drivers match overlay children by `compatible`, and DMA
controllers provide channels for peripherals such as SPI TX/RX.

## Overlay Tree And IRQs

Use the overlay APIs to load runtime tree blobs, query properties, and bind
devices. IRQ domains convert tree-local `interrupts` cells into flat RADix IRQ
resources that drivers can enable, disable, and attach handlers to.

## Display And Input

Framebuffers describe memory layout, output type, connector, available modes,
and primary display state. Input queues provide a shared event path for keyboard,
pointer, terminal, Slint, shell, and future GUI consumers.

## Experimental Compositor

RADCompositor is the current Slint-backed windowing service. It renders desktop
and application surfaces into off-screen buffers, tracks dirty rectangles,
composes damaged regions into a software back buffer, presents only dirty
framebuffer rectangles, and translates global pointer events into per-surface
Slint input events. The x86 smoke path also accepts a shm-backed userspace
surface through `/dev/compositor0`.
