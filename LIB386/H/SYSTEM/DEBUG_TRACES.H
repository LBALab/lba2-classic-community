/*
 * DEBUG_TRACES.H - Debug trace macros for LIB386 ASM-vs-CPP comparison
 *
 * Define ENABLE_LIB386_DEBUG_TRACES before including this header to enable
 * printf-based debug traces. When not defined, all traces compile to no-ops.
 *
 * Trace format: [CPP][FunctionName][N] description: values...
 * where N is a sequential trace ID within each function:
 *   [1] = entry / parameters
 *   [2] = return value or intermediate checkpoint
 *   [3+] = additional intermediate checkpoints
 *
 * The matching ASM traces use the same format with [ASM] prefix and are
 * delimited by:
 *   ; --- debug trace [ASM][FuncName][N] description ---
 *   ...
 *   ; --- end debug trace ---
 */

#ifndef DEBUG_TRACES_H
#define DEBUG_TRACES_H

#ifdef ENABLE_LIB386_DEBUG_TRACES

#include <stdio.h>

#define LIB386_TRACE(impl, func, id, fmt, ...) \
    printf("[" impl "][" func "][" id "] " fmt "\n", ##__VA_ARGS__)

#define LIB386_TRACE_CPP(func, id, fmt, ...) \
    LIB386_TRACE("CPP", func, id, fmt, ##__VA_ARGS__)

#else

#define LIB386_TRACE(impl, func, id, fmt, ...) ((void)0)
#define LIB386_TRACE_CPP(func, id, fmt, ...) ((void)0)

#endif /* ENABLE_LIB386_DEBUG_TRACES */

#endif /* DEBUG_TRACES_H */
