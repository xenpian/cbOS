#ifndef VGA_H
#define VGA_H

#include "types.h"

/* VGA text-mode dimensions */
#define VGA_COLS   80
#define VGA_ROWS   25
#define VGA_BASE   ((uint16_t*)0xB8000)

/* Color constants (foreground / background) */
typedef enum {
    COLOR_BLACK         = 0,
    COLOR_BLUE          = 1,
    COLOR_GREEN         = 2,
    COLOR_CYAN          = 3,
    COLOR_RED           = 4,
    COLOR_MAGENTA       = 5,
    COLOR_BROWN         = 6,
    COLOR_LIGHT_GREY    = 7,
    COLOR_DARK_GREY     = 8,
    COLOR_LIGHT_BLUE    = 9,
    COLOR_LIGHT_GREEN   = 10,
    COLOR_LIGHT_CYAN    = 11,
    COLOR_LIGHT_RED     = 12,
    COLOR_LIGHT_MAGENTA = 13,
    COLOR_YELLOW        = 14,
    COLOR_WHITE         = 15,
} vga_color_t;

/* Pack fg + bg into a single attribute byte */
static inline uint8_t vga_make_color(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)(fg | (bg << 4));
}

/* Pack char + attribute into a VGA cell (16 bits) */
static inline uint16_t vga_make_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

/* Public API */
void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_put_hex(uint32_t val);
void vga_put_dec(uint32_t val);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_set_cursor(uint8_t row, uint8_t col);
void vga_delete_last_char(void);

#endif /* VGA_H */
