/* SPDX-License-Identifier: MIT
 * Original Kavrune BIOS test payload, written for this repository.
 * This is a synthetic boot check, not an operating system or an OS installer.
 * GNU as --32 assembles it; ld places the sector at the BIOS load address.
 */
.code16
.section .text
.global _start
_start:
    /* Normalize either BIOS entry convention (0000:7c00 or 07c0:0000). */
    ljmp $0, $normalized
normalized:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp
    cld
    movw $boot_message, %si
emit_character:
    lodsb
    testb %al, %al
    jz finished
    outb %al, $0xe9
    jmp emit_character
finished:
    /* QEMU isa-debug-exit returns (0x10 << 1) | 1, or status 33. */
    movb $0x10, %al
    outb %al, $0xf4
halt:
    hlt
    jmp halt
boot_message:
    .asciz "KAVRUNE_BOOT_OK"

/* A valid active MBR partition occupies sectors 1 through 8191 of a 4 MiB
 * synthetic disk. CHS fields are compatibility placeholders; LBA is precise.
 */
.org 446, 0
    .byte 0x80
    .byte 0x00, 0x02, 0x00
    .byte 0x83
    .byte 0xfe, 0xff, 0xff
    .long 1
    .long 8191
    .fill 48, 1, 0
.org 510, 0
    .byte 0x55, 0xaa
