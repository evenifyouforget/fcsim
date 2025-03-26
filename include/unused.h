#ifndef UNUSED_H
#define UNUSED_H

/*
 * [[maybe_unused]] is standard and better
 * but old C++ and C doesn't support it
 * so we use the non-portable alternate solution.
 *
 * This suppresses compiler warnings about unused variables.
 */
#ifndef __cplusplus
  #define UNUSED __attribute__((unused))
#else
  #if __cplusplus < 201703L
    #define UNUSED __attribute__((unused))
  #else
    #define UNUSED [[maybe_unused]]
  #endif
#endif

#endif
