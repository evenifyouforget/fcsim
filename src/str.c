#include <stdlib.h>
#include <string.h>

#include "str.h"

void make_str(struct str *str, size_t cap) {
  str->mem = malloc(cap);
  str->mem[0] = 0;
  str->len = 0;
  str->cap = cap;
}

void free_str(struct str *str) { free(str->mem); }

void append_str(struct str *str, char *s) {
  size_t s_len;
  size_t new_len;
  size_t new_cap = str->cap;
  char *new_mem;

  s_len = strlen(s);
  new_len = str->len + s_len;

  if (new_cap <= new_len) {
    while (new_cap <= new_len)
      new_cap *= 2;
    new_mem = malloc(new_cap);
    memcpy(new_mem, str->mem, str->len);
    free(str->mem);
    str->mem = new_mem;
    str->cap = new_cap;
  }

  memcpy(str->mem + str->len, s, s_len + 1);
  str->len = new_len;
}
