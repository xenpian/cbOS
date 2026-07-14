/* =============================================================================
 * vga.c — VGA text-mode driver (80×25, 16 colors)
 * Talks directly to the VGA buffer at 0xB8000 and the CRTC registers
 * for hardware cursor positioning.
 * ============================================================================= */

#include "../include/vga.h"
#include "../include/io.h"

/* ── VGA CRTC port addresses ─────────────────────────────────────────────── */
#define CRTC_ADDR  0x3D4
#define CRTC_DATA  0x3D5
#define CRTC_CURSOR_HIGH  14
#define CRTC_CURSOR_LOW   15

/* ── Internal state ──────────────────────────────────────────────────────── */
static uint8_t  cursor_row;
static uint8_t  cursor_col;
static uint8_t  current_attr;   /* packed fg | (bg << 4) */
static uint16_t *vga_buf = VGA_BASE;

/* ── Hardware cursor helpers ─────────────────────────────────────────────── */
static void update_hw_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_row * VGA_COLS + cursor_col);
    outb(CRTC_ADDR, CRTC_CURSOR_HIGH);
    outb(CRTC_DATA, (uint8_t)(pos >> 8));
    outb(CRTC_ADDR, CRTC_CURSOR_LOW);
    outb(CRTC_DATA, (uint8_t)(pos & 0xFF));
}

/* ── Scroll one line up ──────────────────────────────────────────────────── */
static void scroll(void) {
    /* Move every row one line up */
    for (int row = 1; row < VGA_ROWS; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            vga_buf[(row - 1) * VGA_COLS + col] = vga_buf[row * VGA_COLS + col];
        }
    }
    /* Clear the last row */
    uint16_t blank = vga_make_entry(' ', current_attr);
    for (int col = 0; col < VGA_COLS; col++) {
        vga_buf[(VGA_ROWS - 1) * VGA_COLS + col] = blank;
    }
    cursor_row = VGA_ROWS - 1;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void vga_init(void) {
    current_attr = vga_make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    cursor_row   = 0;
    cursor_col   = 0;
    vga_clear();
}

void vga_clear(void) {
    uint16_t blank = vga_make_entry(' ', current_attr);
    for (int i = 0; i < VGA_ROWS * VGA_COLS; i++) {
        vga_buf[i] = blank;
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hw_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    current_attr = vga_make_color(fg, bg);
}

void vga_set_cursor(uint8_t row, uint8_t col) {
    if (row < VGA_ROWS) cursor_row = row;
    if (col < VGA_COLS) cursor_col = col;
    update_hw_cursor();
}

void vga_putchar(char c) {
    switch (c) {
    case '\n':
        cursor_col = 0;
        cursor_row++;
        break;
    case '\r':
        cursor_col = 0;
        break;
    case '\t':
        cursor_col = (uint8_t)((cursor_col + 8) & ~7u);
        break;
    case '\b':
        if (cursor_col > 0) {
            cursor_col--;
            vga_buf[cursor_row * VGA_COLS + cursor_col] =
                vga_make_entry(' ', current_attr);
        }
        break;
    default:
        vga_buf[cursor_row * VGA_COLS + cursor_col] =
            vga_make_entry(c, current_attr);
        cursor_col++;
        break;
    }

    /* Wrap column */
    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }

    /* Scroll if past last row */
    if (cursor_row >= VGA_ROWS) {
        scroll();
    }

    update_hw_cursor();
}

void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}

void vga_delete_last_char(void) {
    if (cursor_col > 0) {
        cursor_col--;
    } else if (cursor_row > 0) {
        cursor_row--;
        cursor_col = VGA_COLS - 1;
    }
    vga_buf[cursor_row * VGA_COLS + cursor_col] =
        vga_make_entry(' ', current_attr);
    update_hw_cursor();
}

void vga_put_hex(uint32_t val) {
    const char *hex = "0123456789ABCDEF";
    vga_puts("0x");
    /* Print 8 hex digits, most significant first */
    for (int i = 28; i >= 0; i -= 4) {
        vga_putchar(hex[(val >> i) & 0xF]);
    }
}

void vga_put_dec(uint32_t val) {
    char buf[12];
    int  i = 0;
    if (val == 0) { vga_putchar('0'); return; }
    while (val > 0) {
        buf[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    /* Reverse and print */
    for (int j = i - 1; j >= 0; j--) {
        vga_putchar(buf[j]);
    }
}
