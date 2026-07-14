/* =============================================================================
 * shell.c — cbOS interactive shell
 *
 * Built-in commands:
 *   help      — list commands
 *   clear     — clear the screen
 *   echo      — print arguments
 *   mem       — show heap usage
 *   hex <n>   — print a decimal number in hex
 *   reboot    — triple-fault reboot (QEMU / real HW)
 *   halt      — disable interrupts and halt
 *   about     — kernel info
 * ============================================================================= */

#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/mm.h"
#include "../include/string.h"
#include "../include/types.h"

#define CMD_BUF   256
#define MAX_ARGS  16

/* ── Colour helpers ──────────────────────────────────────────────────────── */
static void set_prompt_color(void) { vga_set_color(COLOR_LIGHT_GREEN,  COLOR_BLACK); }
static void set_text_color(void)   { vga_set_color(COLOR_LIGHT_GREY,   COLOR_BLACK); }
static void set_error_color(void)  { vga_set_color(COLOR_LIGHT_RED,    COLOR_BLACK); }
static void set_info_color(void)   { vga_set_color(COLOR_LIGHT_CYAN,   COLOR_BLACK); }
static void set_warn_color(void)   { vga_set_color(COLOR_YELLOW,       COLOR_BLACK); }

/* ── Prompt ──────────────────────────────────────────────────────────────── */
static void print_prompt(void) {
    set_prompt_color();
    vga_puts("cbOS");
    vga_set_color(COLOR_WHITE, COLOR_BLACK);
    vga_puts(":~$ ");
    set_text_color();
}

/* ── Simple argument tokeniser ───────────────────────────────────────────── */
static int tokenise(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max_args) {
        /* Skip spaces */
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        /* Find end of token */
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

/* ── Built-in: help ──────────────────────────────────────────────────────── */
static void cmd_help(void) {
    set_info_color();
    vga_puts("\n Available commands:\n");
    vga_puts(" ─────────────────────────────────────\n");
    set_text_color();
    vga_puts("  help            Show this help\n");
    vga_puts("  clear           Clear the screen\n");
    vga_puts("  echo <text>     Print text to screen\n");
    vga_puts("  mem             Show memory usage\n");
    vga_puts("  hex <decimal>   Convert decimal to hex\n");
    vga_puts("  reboot          Reboot the system\n");
    vga_puts("  halt            Halt the CPU\n");
    vga_puts("  about           About cbOS\n");
    vga_putchar('\n');
}

/* ── Built-in: mem ───────────────────────────────────────────────────────── */
static void cmd_mem(void) {
    size_t used = mm_used();
    size_t free = mm_free();
    size_t total = used + free;

    set_info_color();
    vga_puts("\n Memory usage:\n");
    set_text_color();
    vga_puts("  Used  : "); vga_put_dec((uint32_t)(used  / 1024)); vga_puts(" KB\n");
    vga_puts("  Free  : "); vga_put_dec((uint32_t)(free  / 1024)); vga_puts(" KB\n");
    vga_puts("  Total : "); vga_put_dec((uint32_t)(total / 1024)); vga_puts(" KB\n\n");
}

/* ── Built-in: echo ──────────────────────────────────────────────────────── */
static void cmd_echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        vga_puts(argv[i]);
        if (i < argc - 1) vga_putchar(' ');
    }
    vga_putchar('\n');
}

/* ── Built-in: hex ───────────────────────────────────────────────────────── */
static uint32_t parse_uint(const char *s) {
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (uint32_t)(*s++ - '0');
    return v;
}

static void cmd_hex(int argc, char *argv[]) {
    if (argc < 2) {
        set_error_color();
        vga_puts("Usage: hex <decimal_number>\n");
        set_text_color();
        return;
    }
    uint32_t n = parse_uint(argv[1]);
    vga_put_dec(n);
    vga_puts(" = ");
    vga_put_hex(n);
    vga_putchar('\n');
}

/* ── Built-in: about ─────────────────────────────────────────────────────── */
static void cmd_about(void) {
    set_info_color();
    vga_puts("\n  cbOS v1.0\n");
    set_text_color();
    vga_puts("  Architecture : x86 (i686), 32-bit protected mode\n");
    vga_puts("  Bootloader   : custom MBR (BIOS INT 13h)\n");
    vga_puts("  Memory       : bitmap page allocator + free-list heap\n");
    vga_puts("  Interrupts   : PIC 8259A remapped, 256-gate IDT\n");
    vga_puts("  Keyboard     : PS/2 scan-code set 1, ring buffer\n");
    vga_puts("  Display      : VGA text 80x25, 16 colours\n");
    set_warn_color();
    vga_puts("  Built with   : nasm + i686-elf-gcc + ld\n\n");
    set_text_color();
}

/* ── Built-in: reboot ────────────────────────────────────────────────────── */
static void cmd_reboot(void) {
    set_warn_color();
    vga_puts("Rebooting...\n");
    set_text_color();
    /* Triple fault: load a null IDT then trigger an interrupt */
    __asm__ volatile (
        "lidt %0\n\t"
        "int $0\n\t"
        :: "m"((uint8_t[6]){0})
    );
}

/* ── Built-in: halt ──────────────────────────────────────────────────────── */
static void cmd_halt(void) {
    set_warn_color();
    vga_puts("System halted. Safe to power off.\n");
    set_text_color();
    __asm__ volatile("cli; hlt");
}

/* ── Command dispatcher ──────────────────────────────────────────────────── */
static void dispatch(char *line) {
    /* Trim trailing newline */
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
        line[--len] = '\0';
    }
    if (len == 0) return;

    char  *argv[MAX_ARGS];
    int    argc = tokenise(line, argv, MAX_ARGS);
    if (argc == 0) return;

    if      (strcmp(argv[0], "help")   == 0) cmd_help();
    else if (strcmp(argv[0], "clear")  == 0) vga_clear();
    else if (strcmp(argv[0], "echo")   == 0) cmd_echo(argc, argv);
    else if (strcmp(argv[0], "mem")    == 0) cmd_mem();
    else if (strcmp(argv[0], "hex")    == 0) cmd_hex(argc, argv);
    else if (strcmp(argv[0], "about")  == 0) cmd_about();
    else if (strcmp(argv[0], "reboot") == 0) cmd_reboot();
    else if (strcmp(argv[0], "halt")   == 0) cmd_halt();
    else {
        set_error_color();
        vga_puts("Unknown command: '");
        vga_puts(argv[0]);
        vga_puts("'  (type 'help' for a list)\n");
        set_text_color();
    }
}

/* ── Shell main loop ─────────────────────────────────────────────────────── */
void shell_run(void) {
    char buf[CMD_BUF];

    set_info_color();
    vga_puts(" Type 'help' for available commands.\n\n");
    set_text_color();

    for (;;) {
        print_prompt();
        keyboard_readline(buf, CMD_BUF);
        dispatch(buf);
    }
}
