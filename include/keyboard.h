#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

#define KEYBOARD_BUFFER_SIZE 256

void keyboard_init(void);

/* Returns next character from keyboard buffer (blocks until available) */
char keyboard_getchar(void);

/* Non-blocking: returns 0 if buffer empty */
char keyboard_poll(void);

/* Read a full line into buf (max len-1 chars), null-terminated, echoes input */
void keyboard_readline(char *buf, size_t len);

#endif /* KEYBOARD_H */
