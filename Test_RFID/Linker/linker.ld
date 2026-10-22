ENTRY(Reset_Handler)


/* =========================================================
 * Memory
 * ========================================================= */

_estack = ORIGIN(RAM) + LENGTH(RAM);


MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}


/* =========================================================
 * Sections
 * ========================================================= */

SECTIONS
{

    /* -----------------------------------------------------
     * Vector table
     * ----------------------------------------------------- */

    .isr_vector :
    {
        . = ALIGN(4);

        KEEP(*(.isr_vector))

        . = ALIGN(4);

    } > FLASH


    /* -----------------------------------------------------
     * Program code + constants
     * ----------------------------------------------------- */

    .text :
    {
        . = ALIGN(4);

        *(.text)
        *(.text.*)

        *(.rodata)
        *(.rodata.*)

        . = ALIGN(4);

        _etext = .;

    } > FLASH


    /* -----------------------------------------------------
     * Initialized data
     *
     * Stored in FLASH but copied to RAM
     * by startup.s
     * ----------------------------------------------------- */

    .data :
    {
        . = ALIGN(4);

        _sdata = .;

        *(.data)
        *(.data.*)

        . = ALIGN(4);

        _edata = .;

    } > RAM AT > FLASH


    /* Address of .data initialization values in FLASH */

    _sidata = LOADADDR(.data);


    /* -----------------------------------------------------
     * BSS
     * ----------------------------------------------------- */

    .bss :
    {
        . = ALIGN(4);

        _sbss = .;

        *(.bss)
        *(.bss.*)

        *(COMMON)

        . = ALIGN(4);

        _ebss = .;

    } > RAM


    /* -----------------------------------------------------
     * Discard unnecessary sections
     * ----------------------------------------------------- */

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}