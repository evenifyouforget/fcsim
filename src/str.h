#ifndef STR_H
#define STR_H

#ifdef __cplusplus
extern "C" {
#endif

struct str {
  char *mem;
  size_t len;
  size_t cap;
};

void make_str(struct str *str, size_t cap);

void free_str(struct str *str);

void append_str(struct str *str, char *s);

#ifdef __cplusplus
}
#endif

#endif
