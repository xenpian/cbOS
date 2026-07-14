#ifndef TYPES_H
#define TYPES_H

/* Sized integer types — no stdlib available */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint32_t           size_t;
typedef int32_t            ssize_t;
typedef uint32_t           uintptr_t;

#define NULL  ((void*)0)
#define true  1
#define false 0
typedef uint8_t bool;

/* Compiler hints */
#define PACKED      __attribute__((packed))
#define UNUSED      __attribute__((unused))
#define NORETURN    __attribute__((noreturn))
#define ALIGN(n)    __attribute__((aligned(n)))

#endif /* TYPES_H */
