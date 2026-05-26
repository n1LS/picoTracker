/* picoTracker Bootloader Memory Layout
 * 
 * Stage 3 Bootloader (0x10000100 - 0x10040000 = 256 KB)
 * Firmware Slot A (0x10040000 - 0x107A0000 = 7.5 MB)
 * Firmware Slot B (0x107A0000 - 0x10B00000 = 7.5 MB)
 * 
 * Based on GCC ARM embedded samples.
 */

MEMORY
{
    FLASH(rx) : ORIGIN = 0x10000100, LENGTH = (256k - 256)
    RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
    SCRATCH_X(rwx) : ORIGIN = 0x20040000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20041000, LENGTH = 4k
}

ENTRY(_entry_point)

SECTIONS
{
    /* The second stage bootloader is included by pico_sdk
       and comes before .text. For the bootloader, we skip it since
       we're at Stage 3 and need our own entry point.
    */

    .flash_begin : {
        __flash_binary_start = .;
    } > FLASH

    .text : {
        __logical_binary_start = .;
        KEEP (*(.vectors))
        KEEP (*(.binary_info_header))
        __binary_info_header_end = .;
        KEEP (*(.embedded_block))
        __embedded_block_end = .;
        KEEP (*(.reset))
        
        *(.init)
        *(EXCLUDE_FILE(*libgcc.a: *libc.a:*lib_a-mem*.o *libm.a:) .text*)
        *(.fini)
        
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)
        
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)

        . = ALIGN(4);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(SORT(.preinit_array.*)))
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);

        . = ALIGN(4);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);

        *(.rodata*)

        . = ALIGN(4);
        *(.ARM.exidx*)
        . = ALIGN(4);

    } > FLASH

    .vfat : {
        *(.gnu.sgottab*)
    } > FLASH

    .etext : {
        __etext = .;
    } > FLASH

    .ram_vector_table (NOLOAD): {
        *(.ram_vector_table)
    } > RAM

    .data : {
        __data_start__ = .;
        *(vtable)

        *(.data*)

        . = ALIGN(4);
        *(.after_data.*)
        . = ALIGN(4);

        __data_end__ = .;
    } > RAM AT > FLASH

    .uninitialized_data (NOLOAD): {
        . = ALIGN(4);
        *(.uninitialized_data*)
    } > RAM

    .bss : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.bss*)))
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    .heap (NOLOAD):
    {
        __end__ = .;
        end = __end__;
        *(.heap*)
        __HeapLimit = .;
    } > RAM

    .stack_dummy (NOLOAD):
    {
        *(.stack*)
    } > RAM

    .flash_end : {
        __flash_binary_end = .;
    } > FLASH

    .scratch_x (NOLOAD): {
        *(.scratch_x.*)
    } > SCRATCH_X

    .scratch_y (NOLOAD): {
        *(.scratch_y.*)
    } > SCRATCH_Y

    __StackTop = ORIGIN(RAM) + LENGTH(RAM);
    __StackLimit = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);

    ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
