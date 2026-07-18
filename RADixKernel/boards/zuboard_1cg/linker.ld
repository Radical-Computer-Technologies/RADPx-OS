ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x00200000;

    .text : ALIGN(4096) {
        KEEP(*(.text.boot))
        *(.text*)
        *(.rodata*)
    } :text

    .data : ALIGN(4096) {
        *(.data*)
    } :data

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } :data

    __image_end = .;
}
