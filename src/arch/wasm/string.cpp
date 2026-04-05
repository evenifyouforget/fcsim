#include <stddef.h>

// Internal helpers: not part of the C ABI.

static void *memcpy_right(void *dest, const void *src, size_t n) {
  char *destc = (char *)dest;
  const char *srcc = (const char *)src;
  while (n--)
    *destc++ = *srcc++;
  return dest;
}

static void *memcpy_left(void *dest, const void *src, size_t n) {
  char *destc = (char *)dest;
  const char *srcc = (const char *)src;
  while (n--)
    destc[n] = srcc[n];
  return dest;
}

extern "C" {

size_t strlen(const char *s) {
  size_t res = 0;
  while (*s) {
    s++;
    res++;
  }
  return res;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && *s2 && *s1 == *s2) {
    s1++;
    s2++;
  }
  return *s1 - *s2;
}

void *memset(void *s, int c, size_t n) {
  char *sc = (char *)s;
  while (n--)
    *sc++ = (char)c;
  return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
  return memcpy_right(dest, src, n);
}

void *memmove(void *dest, const void *src, size_t n) {
  if (dest < src)
    return memcpy_right(dest, src, n);
  else
    return memcpy_left(dest, src, n);
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const char *s1c = (const char *)s1;
  const char *s2c = (const char *)s2;
  while (n--) {
    int res = *s1c - *s2c;
    if (res)
      return res;
    s1c++;
    s2c++;
  }
  return 0;
}

} // extern "C"
