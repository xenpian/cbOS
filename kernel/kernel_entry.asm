; =============================================================================
; kernel_entry.asm — 32-bit kernel entry point
; Linked first so it sits at 0x1000 (where the bootloader jumps).
; Sets up the stack, zeroes BSS, then calls kernel_main().
; =============================================================================

[BITS 32]

; Declare kernel_main as external C symbol
extern kernel_main

global _start

_start:
    ; Reload segment registers just in case (bootloader already set them)
    mov  ax, 0x10               ; DATA_SEG selector from GDT
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Set up kernel stack (grows downward)
    mov  esp, _kernel_stack_top

    ; Zero out the BSS section
    extern _bss_start
    extern _bss_end
    mov  edi, _bss_start
    mov  ecx, _bss_end
    sub  ecx, edi
    xor  eax, eax
    rep  stosb

    ; Call the C kernel entry point — should never return
    call kernel_main

    ; Hang if kernel_main returns (should never happen)
hang:
    cli
    hlt
    jmp hang

; ── Kernel stack (16 KB) ──────────────────────────────────────────────────────
section .bss
align 16
_kernel_stack_bottom:
    resb 16384                  ; 16 KB stack
_kernel_stack_top:
