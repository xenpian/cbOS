/* =============================================================================
 * keyboard.c — PS/2 keyboard driver
 * Hooks IRQ1 (vector 33), translates scan codes to ASCII, and feeds a
 * circular ring buffer that the shell reads with keyboard_getchar().
 * ============================================================================= */

#include "../include/keyboard.h"
#include "../include/idt.h"
#include "../include/io.h"
#include "../include/vga.h"

/* ── PS/2 ports ──────────────────────────────────────────────────────────── */
#define KB_DATA    0x60
#define KB_STATUS  0x64

/* ── Modifier key state ──────────────────────────────────────────────────── */
static bool shift_pressed = false;
static bool caps_lock      = false;

/* ── Ring buffer ─────────────────────────────────────────────────────────── */
static char     kb_buf[KEYBOARD_BUFFER_SIZE];
static uint32_t kb_head = 0;   /* write index */
static uint32_t kb_tail = 0;   /* read  index */

static bool buf_empty(void) { return kb_head == kb_tail; }
static bool buf_full(void)  {
    return ((kb_head + 1) % KEYBOARD_BUFFER_SIZE) == kb_tail;
}
static void buf_push(char c) {
    if (!buf_full()) {
        kb_buf[kb_head] = c;
        kb_head = (kb_head + 1) % KEYBOARD_BUFFER_SIZE;
    }
}
static char buf_pop(void) {
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

/* ── US QWERTY scan-code set 1 ───────────────────────────────────────────── */
/* Index = scan code; value = ASCII (unshifted) */
static const char sc_normal[128] = {
/*00*/  0,   0,  '1', '2', '3', '4', '5', '6',
/*08*/ '7', '8', '9', '0', '-', '=',  '\b', '\t',
/*10*/ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
/*18*/ 'o', 'p', '[', ']', '\n',  0,  'a', 's',
/*20*/ 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
/*28*/ '\'', '`',  0, '\\', 'z', 'x', 'c', 'v',
/*30*/ 'b', 'n', 'm', ',', '.', '/',  0,  '*',
/*38*/  0,  ' ',  0,   0,   0,   0,   0,   0,
/*40*/  0,   0,   0,   0,   0,   0,   0,  '7',
/*48*/ '8', '9', '-', '4', '5', '6', '+', '1',
/*50*/ '2', '3', '0', '.',  0,   0,   0,   0,
/*58*/  0,   0,   0,   0,   0,   0,   0,   0,
/*60*/  0,   0,   0,   0,   0,   0,   0,   0,
/*68*/  0,   0,   0,   0,   0,   0,   0,   0,
/*70*/  0,   0,   0,   0,   0,   0,   0,   0,
/*78*/  0,   0,   0,   0,   0,   0,   0,   0,
};

static const char sc_shifted[128] = {
/*00*/  0,   0,  '!', '@', '#', '$', '%', '^',
/*08*/ '&', '*', '(', ')', '_', '+', '\b', '\t',
/*10*/ 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
/*18*/ 'O', 'P', '{', '}', '\n',  0,  'A', 'S',
/*20*/ 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
/*28*/ '"',  '~',  0,  '|', 'Z', 'X', 'C', 'V',
/*30*/ 'B', 'N', 'M', '<', '>', '?',  0,  '*',
/*38*/  0,  ' ',  0,   0,   0,   0,   0,   0,
/*40*/  0,   0,   0,   0,   0,   0,   0,  '7',
/*48*/ '8', '9', '-', '4', '5', '6', '+', '1',
/*50*/ '2', '3', '0', '.',  0,   0,   0,   0,
/*58*/  0,   0,   0,   0,   0,   0,   0,   0,
/*60*/  0,   0,   0,   0,   0,   0,   0,   0,
/*68*/  0,   0,   0,   0,   0,   0,   0,   0,
/*70*/  0,   0,   0,   0,   0,   0,   0,   0,
/*78*/  0,   0,   0,   0,   0,   0,   0,   0,
};

/* Special scan codes */
#define SC_LSHIFT  0x2A
#define SC_RSHIFT  0x36
#define SC_CAPS    0x3A
#define SC_LSHIFT_REL  0xAA
#define SC_RSHIFT_REL  0xB6
#define SC_CTRL    0x1D
#define SC_ALT     0x38

/* ── IRQ1 handler ────────────────────────────────────────────────────────── */
static void keyboard_irq_handler(registers_t *regs) {
    (void)regs;

    uint8_t sc = inb(KB_DATA);

    /* Handle release codes (bit 7 set) */
    if (sc & 0x80) {
        uint8_t released = sc & 0x7F;
        if (released == SC_LSHIFT || released == SC_RSHIFT)
            shift_pressed = false;
        return;
    }

    /* Press codes */
    switch (sc) {
    case SC_LSHIFT:
    case SC_RSHIFT:
        shift_pressed = true;
        return;
    case SC_CAPS:
        caps_lock = !caps_lock;
        return;
    default:
        break;
    }

    /* Translate to ASCII */
    char c;
    if (shift_pressed) {
        c = sc_shifted[sc];
    } else {
        c = sc_normal[sc];
    }

    /* Apply caps lock to letters */
    if (caps_lock && c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (caps_lock && c >= 'A' && c <= 'Z') c = (char)(c + 32);

    if (c) buf_push(c);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void keyboard_init(void) {
    idt_register_handler(IRQ1, keyboard_irq_handler);
}

char keyboard_poll(void) {
    if (buf_empty()) return 0;
    return buf_pop();
}

char keyboard_getchar(void) {
    while (buf_empty()) {
        __asm__ volatile("hlt");   /* sleep until next interrupt */
    }
    return buf_pop();
}

void keyboard_readline(char *buf, size_t len) {
    size_t i = 0;
    while (i < len - 1) {
        char c = keyboard_getchar();
        if (c == '\n' || c == '\r') {
            vga_putchar('\n');
            break;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                vga_delete_last_char();
            }
        } else {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
    buf[i] = '\0';
}
