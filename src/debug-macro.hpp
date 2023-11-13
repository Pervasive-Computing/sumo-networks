#pragma once

#include "ansi-escape-codes.hpp"

#include <cstdio>
// TODO: what if we don't have <unistd.h> e.g. on Windows
#include <unistd.h>

#ifdef DEBUG_PRINTF
#warning "DEBUG_PRINTF is already defined"
#endif

#ifndef NDEBUG
#define DEBUG_PRINTF(format, ...)                                              \
  /* The do {...} while(0) structure is a common idiom used in macros. */      \
  /* It allows the macro to be used in all contexts that a normal function     \
   * call could be used. */                                                    \
  /* It creates a compound statement in C/C++ that behaves as a single         \
   * statement. */                                                             \
  do {                                                                         \
    bool is_tty = isatty(fileno(stderr));                                      \
    if (is_tty) {                                                              \
      std::fprintf(                                                            \
          stderr, "%s%s%s:%s%s%s:%d: ", escape_codes::color::fg::cyan,         \
          __FILE__, escape_codes::reset, escape_codes::color::fg::yellow,      \
          __func__, escape_codes::reset, __LINE__);                            \
    } else {                                                                   \
      std::fprintf(stderr, "%s:%s:%d: ", __FILE__, __func__, __LINE__);        \
    }                                                                          \
    std::fprintf(stderr, format, ##__VA_ARGS__);                               \
    std::fprintf(stderr, "\n");                                                \
  } while (0)
#else
// do nothing
#define DEBUG_PRINTF(format, ...)
#endif
