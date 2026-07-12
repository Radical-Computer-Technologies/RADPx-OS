# Minimal Examples {#minimal_examples}

These examples show the intended public API shape. They omit platform-specific
register setup and error logging for clarity.

## Register A Kernel Module

```c
static rad_status_t my_driver_init(void *context) {
    (void)context;
    return RAD_STATUS_OK;
}

static void my_driver_exit(void *context) {
    (void)context;
}

static const rad_module_descriptor_t module = {
    .size = sizeof(rad_module_descriptor_t),
    .name = "rad_example",
    .bus = "platform",
    .compatible = "rad,example",
    .init = my_driver_init,
    .exit = my_driver_exit,
};

rad_module_register(&module);
```

## Register An I2C Controller And Child Driver

```c
static rad_status_t i2c_xfer(void *context,
                             const rad_i2c_transfer_t *transfer) {
    (void)context;
    return transfer ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
}

static rad_status_t tmp102_probe(void *context, rad_i2c_device_t *device) {
    (void)context;
    rad_irq_resource_t irq;
    if (rad_i2c_device_get_irq(device, 0, &irq) == RAD_STATUS_OK) {
        rad_irq_resource_enable(&irq);
    }
    return RAD_STATUS_OK;
}

rad_i2c_controller_ops_t ops = {
    .transfer = i2c_xfer,
};
rad_i2c_controller_config_t controller = {
    .size = sizeof(rad_i2c_controller_config_t),
    .bus_id = 0,
    .name = "rad_i2c0",
    .tree_path = "/soc/i2c@0",
    .clock_hz = 400000,
};
rad_i2c_driver_t driver = {
    .size = sizeof(rad_i2c_driver_t),
    .name = "rad_i2c_tmp102",
    .compatible = "ti,tmp102",
    .probe = tmp102_probe,
};

rad_i2c_driver_register(&driver);
rad_i2c_controller_register(&controller, &ops);
rad_i2c_bind_tree();
```

## Register DMA And Use SPI Auto Mode

```c
rad_dma_controller_config_t dma = {
    .size = sizeof(rad_dma_controller_config_t),
    .name = "rad_dma0",
    .channel_count = 8,
};
rad_dma_backend_ops_t dma_ops = {
    .request = my_dma_request,
    .release = my_dma_release,
    .submit = my_dma_submit,
    .wait = my_dma_wait,
    .cancel = my_dma_cancel,
};

rad_dma_controller_register(&dma, &dma_ops);

rad_spi_transfer_t transfer = {
    .tx_data = tx,
    .rx_data = rx,
    .size = count,
    .speed_hz = 12000000,
    .bits_per_word = 8,
    .cs = 0,
    .transfer_mode = RAD_SPI_TRANSFER_MODE_AUTO,
};

rad_spi_device_transfer(device, &transfer);
```

## Register A Framebuffer

```c
rad_framebuffer_config_t fb = {
    .size = sizeof(rad_framebuffer_config_t),
    .name = "/dev/fb0",
    .output_type = RAD_DISPLAY_OUTPUT_GRUB,
    .connector = "virtio-vga",
    .primary = 1,
};
fb.info.size = sizeof(rad_framebuffer_info_t);
fb.info.width = 1024;
fb.info.height = 768;
fb.info.stride_bytes = 4096;
fb.info.pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
fb.info.pixels = framebuffer_pixels;

rad_framebuffer_register_ex(&fb, &fb_ops);
```

## Read Input Events

```c
rad_input_queue_t queue;
rad_input_queue_create("kbd0", 64, &queue);
rad_input_device_register_queue("/dev/input/event0", queue);

rad_input_event_t events[8];
size_t count = 0;
rad_input_queue_read(queue, events, 8, 16, &count);
```

## Mount A VFS Provider

```c
rad_vfs_backend_ops_t ops = {
    .context = fs_context,
    .open = fs_open,
    .read = fs_read,
    .write = fs_write,
    .close = fs_close,
    .stat = fs_stat,
    .list = fs_list,
};

rad_vfs_mount_provider("/", &ops);
```

## Use File Descriptors Through The Syscall ABI

```c
int32_t fd = rad_fd_open("/etc/init.radsh", RAD_VFS_READ);
char buffer[128];
intptr_t n = rad_fd_read(fd, buffer, sizeof(buffer));
rad_fd_close(fd);
```
