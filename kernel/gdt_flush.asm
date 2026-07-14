; =============================================================================
; gdt_flush.asm — reload segment registers after a new GDT is installed
; Called from C as:  extern void gdt_flush(uint32_t gdt_ptr_addr);
; =============================================================================

[BITS 32]
global gdt_flush

gdt_flush:
    mov  eax, [esp + 4]         ; First argument: address of gdt_ptr_t
    lgdt [eax]                  ; Load the new GDT

    ; Reload code segment with a far jump
    jmp  0x08:.flush            ; 0x08 = kernel code selector

.flush:
    ; Reload all data segment registers
    mov  ax, 0x10               ; 0x10 = kernel data selector
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    ret
