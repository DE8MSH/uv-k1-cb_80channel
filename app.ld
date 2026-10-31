/* CB Skaner - linker script.
 *
 * The app runs from the firmware's 4 KiB overlay RAM at APP_VMA (passed by
 * build.sh with --defsym). app_main must be the first byte of the image; code,
 * constants, data and zero-initialised data all share the 4 KiB.
 */

ENTRY(app_main)

MEMORY {
    WORKSPACE (rwx) : ORIGIN = APP_VMA, LENGTH = 4K
}

SECTIONS {
    .image : {
        KEEP(*(.text.entry))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        *(.data*)
        . = ALIGN(4);
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
    } > WORKSPACE

    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) *(.comment) *(.note*) }
}
