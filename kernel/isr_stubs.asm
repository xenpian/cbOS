; =============================================================================
; isr_stubs.asm — ISR and IRQ entry stubs for all 48 vectors
;
; Each stub:
;   1. Pushes a dummy error code (for exceptions that don't push one)
;   2. Pushes the interrupt number
;   3. Saves all registers (pusha)
;   4. Calls the common C handler: isr_common_handler(registers_t*)
;   5. Restores and returns with iretd
; =============================================================================

[BITS 32]

extern isr_common_handler

; ── Macro: exception WITHOUT error code ──────────────────────────────────────
%macro ISR_NO_ERR 1
global isr%1
isr%1:
    cli
    push dword 0        ; dummy error code
    push dword %1       ; interrupt number
    jmp  isr_common_stub
%endmacro

; ── Macro: exception WITH error code (CPU pushes it automatically) ───────────
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push dword %1       ; interrupt number (error code already on stack)
    jmp  isr_common_stub
%endmacro

; ── Macro: IRQ stub ──────────────────────────────────────────────────────────
%macro IRQ 2            ; %1=IRQ number, %2=vector number
global irq%1
irq%1:
    cli
    push dword 0        ; dummy error code
    push dword %2       ; vector number
    jmp  isr_common_stub
%endmacro

; ── CPU exceptions ────────────────────────────────────────────────────────────
ISR_NO_ERR  0   ; #DE Divide Error
ISR_NO_ERR  1   ; #DB Debug
ISR_NO_ERR  2   ; NMI
ISR_NO_ERR  3   ; #BP Breakpoint
ISR_NO_ERR  4   ; #OF Overflow
ISR_NO_ERR  5   ; #BR Bound Range Exceeded
ISR_NO_ERR  6   ; #UD Invalid Opcode
ISR_NO_ERR  7   ; #NM Device Not Available
ISR_ERR     8   ; #DF Double Fault          (has error code)
ISR_NO_ERR  9   ; Coprocessor Segment Overrun
ISR_ERR    10   ; #TS Invalid TSS           (has error code)
ISR_ERR    11   ; #NP Segment Not Present   (has error code)
ISR_ERR    12   ; #SS Stack Fault           (has error code)
ISR_ERR    13   ; #GP General Protection    (has error code)
ISR_ERR    14   ; #PF Page Fault            (has error code)
ISR_NO_ERR 15   ; Reserved
ISR_NO_ERR 16   ; #MF x87 FP Exception
ISR_ERR    17   ; #AC Alignment Check       (has error code)
ISR_NO_ERR 18   ; #MC Machine Check
ISR_NO_ERR 19   ; #XM SIMD FP Exception
ISR_NO_ERR 20   ; #VE Virtualisation
ISR_NO_ERR 21
ISR_NO_ERR 22
ISR_NO_ERR 23
ISR_NO_ERR 24
ISR_NO_ERR 25
ISR_NO_ERR 26
ISR_NO_ERR 27
ISR_NO_ERR 28
ISR_NO_ERR 29
ISR_NO_ERR 30
ISR_NO_ERR 31

; ── Hardware IRQs (PIC remapped to vectors 32-47) ─────────────────────────────
IRQ  0, 32   ; PIT  timer
IRQ  1, 33   ; PS/2 keyboard
IRQ  2, 34
IRQ  3, 35
IRQ  4, 36
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40   ; RTC
IRQ  9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44   ; PS/2 mouse
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ── Common stub ──────────────────────────────────────────────────────────────
global idt_flush

idt_flush:
    mov  eax, [esp + 4]
    lidt [eax]
    ret

isr_common_stub:
    pusha                       ; Save EDI ESI EBP ESP EBX EDX ECX EAX

    ; Load kernel data segments
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    ; Pass pointer to saved registers as argument
    push esp
    call isr_common_handler
    add  esp, 4                 ; Clean up argument

    ; Restore data segments
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa                        ; Restore registers
    add  esp, 8                 ; Clean up int_no + err_code
    sti
    iretd
