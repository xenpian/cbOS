; =============================================================================
; boot.asm — Stage 1 Bootloader (MBR, 512 bytes)
; Loads the kernel from disk and jumps to protected mode.
; =============================================================================

[BITS 16]
[ORG 0x7C00]

; ── Constants ────────────────────────────────────────────────────────────────
KERNEL_OFFSET   equ 0x1000      ; Where we load the kernel in memory
KERNEL_SECTORS  equ 64          ; How many sectors to read (32 KB)

; ── Entry ────────────────────────────────────────────────────────────────────
start:
    ; Set up segments and stack
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00             ; Stack grows downward from bootloader

    ; Save the BIOS boot drive number
    mov  [BOOT_DRIVE], dl

    ; Print boot message
    mov  si, MSG_BOOT
    call print_string_rm

    ; Load kernel from disk
    call load_kernel

    ; Switch to protected mode
    call switch_to_pm

    jmp $                       ; Should never reach here

; ── Load kernel from disk (BIOS INT 13h) ─────────────────────────────────────
load_kernel:
    mov  si, MSG_LOAD
    call print_string_rm

    mov  bx, KERNEL_OFFSET      ; ES:BX = destination buffer
    mov  dh, KERNEL_SECTORS
    mov  dl, [BOOT_DRIVE]
    call disk_load
    ret

; ── disk_load: read DH sectors starting at LBA 1 into ES:BX ──────────────────
disk_load:
    push dx
    mov  ah, 0x02               ; BIOS read sectors
    mov  al, dh                 ; Number of sectors
    mov  ch, 0x00               ; Cylinder 0
    mov  cl, 0x02               ; Start from sector 2 (sector 1 = MBR)
    mov  dh, 0x00               ; Head 0
    int  0x13
    jc   disk_error
    pop  dx
    cmp  al, dh
    jne  sectors_error
    ret

disk_error:
    mov  si, MSG_DISK_ERR
    call print_string_rm
    jmp  $

sectors_error:
    mov  si, MSG_SECT_ERR
    call print_string_rm
    jmp  $

; ── print_string_rm: print null-terminated string (SI) in real mode ───────────
print_string_rm:
    pusha
    mov  ah, 0x0E
.loop:
    lodsb
    test al, al
    jz   .done
    int  0x10
    jmp  .loop
.done:
    popa
    ret

; ── GDT ──────────────────────────────────────────────────────────────────────
gdt_start:

gdt_null:                       ; Mandatory null descriptor
    dd 0x00000000
    dd 0x00000000

gdt_code:                       ; Code segment: base=0, limit=4GB, ring 0
    dw 0xFFFF                   ; Limit low
    dw 0x0000                   ; Base low
    db 0x00                     ; Base mid
    db 10011010b                ; Access: present, ring0, code, exec/read
    db 11001111b                ; Flags: 4KB gran, 32-bit + limit high
    db 0x00                     ; Base high

gdt_data:                       ; Data segment: base=0, limit=4GB, ring 0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b                ; Access: present, ring0, data, read/write
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size
    dd gdt_start                ; Address

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; ── switch_to_pm: enable A20, load GDT, set PE bit ───────────────────────────
switch_to_pm:
    cli

    ; Enable A20 line via keyboard controller
    call enable_a20

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Set PE (Protection Enable) bit in CR0
    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax

    ; Far jump to flush pipeline and enter 32-bit protected mode
    jmp  CODE_SEG:init_pm

enable_a20:
    in   al, 0x92
    or   al, 2
    out  0x92, al
    ret

; ── 32-bit protected mode init ────────────────────────────────────────────────
[BITS 32]
init_pm:
    ; Reload all segment registers with data segment selector
    mov  ax, DATA_SEG
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    ; Set up a proper stack in protected mode
    mov  ebp, 0x90000
    mov  esp, ebp

    ; Jump to the kernel entry point
    call KERNEL_OFFSET

    jmp  $                      ; Halt if kernel returns

; ── Data ─────────────────────────────────────────────────────────────────────
[BITS 16]
MSG_BOOT     db 'cbOS Bootloader v1.0', 0x0D, 0x0A, 0
MSG_LOAD     db 'Loading kernel...', 0x0D, 0x0A, 0
MSG_DISK_ERR db 'Disk read error!', 0x0D, 0x0A, 0
MSG_SECT_ERR db 'Incorrect sector count!', 0x0D, 0x0A, 0
BOOT_DRIVE   db 0

; ── Boot signature ────────────────────────────────────────────────────────────
times 510 - ($ - $$) db 0
dw 0xAA55
