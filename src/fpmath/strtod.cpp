#include <fpmath/fpmath.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

struct bignum {
  uint32_t words[130];
  int num_words;
};

static inline int is_digit(char c) { return '0' <= c && c <= '9'; }

static void bignum_muladd(struct bignum *bignum, uint32_t mul, uint32_t add) {
  uint64_t res;
  int i;

  for (i = 0; i < bignum->num_words; i++) {
    res = (uint64_t)bignum->words[i] * mul + add;
    bignum->words[i] = res;
    add = res >> 32;
  }

  if (add) {
    bignum->words[bignum->num_words] = add;
    bignum->num_words++;
  }
}

static int bit_count(uint32_t n) {
  int res = 0;

  while (n) {
    n >>= 1;
    res++;
  }

  return res;
}

static double bignum_to_double(struct bignum *bignum) {
  uint32_t words[4] = {0};
  int high_bits;
  uint64_t mantissa;
  int half_bit;
  uint32_t tail;
  int exp;
  int i;

  for (i = 0; i < bignum->num_words && i < 4; i++)
    words[i] = bignum->words[bignum->num_words - i - 1];

  if (bignum->num_words == 1)
    return words[0];

  high_bits = bit_count(words[0]);

  mantissa = words[0];
  if (high_bits >= 21) {
    mantissa <<= (53 - high_bits);
    mantissa |= words[1] >> (high_bits - 21);
  } else {
    mantissa <<= 32;
    mantissa |= words[1];
    if (bignum->num_words > 2) {
      mantissa <<= (21 - high_bits);
      mantissa |= words[2] >> (high_bits + 11);
    }
  }

  if (high_bits > 21) {
    half_bit = (words[1] >> (high_bits - 22)) & 1;
    tail = words[1] & ((1 << (high_bits - 22)) - 1);
    tail |= words[2];
  } else if (high_bits == 21) {
    half_bit = words[2] >> 31;
    tail = words[2] & 0x7fffffff;
  } else {
    half_bit = (words[2] >> (high_bits + 10)) & 1;
    tail = words[2] & ((1 << (high_bits + 10)) - 1);
    tail |= words[3];
  }

  if (half_bit && (tail || (mantissa & 1)))
    mantissa++;

  exp = (bignum->num_words - 1) * 32 + high_bits - 53;
  if (exp < 0)
    exp = 0;

  double res = mantissa;
  while (exp--)
    res *= 2;
  return res;
}

static double binexp(double x, int n) {
  double res = 1.0;

  while (n) {
    if (n & 1)
      res *= x;
    x *= x;
    n >>= 1;
  }

  return res;
}

static int strtoi(const char *str, int len, int *res) {
  int val = 0;
  int neg = 0;
  int digit;
  int i;

  if (len == 0)
    return -1;
  if (*str == '+' || *str == '-') {
    if (*str == '-')
      neg = 1;
    str++;
    len--;
  }
  for (i = 0; i < len; i++) {
    digit = str[i] - '0';
    if (digit < 0 || digit > 10)
      return -1;
    val = val * 10 + digit;
  }
  *res = neg ? -val : val;

  return 0;
}

int fp_strtod(const char *str, int len, double *res) {
  struct bignum bignum;
  int is_neg = 0;
  int num_len;
  int num_digits = 0;
  int num_digits_after_decimal = 0;
  int seen_decimal = 0;
  int exp = 0;
  double num;
  int i;

  switch (*str) {
  case '-':
    is_neg = 1;
  case '+':
    str++;
    len--;
  }

  for (i = 0; i < len; i++) {
    if (is_digit(str[i])) {
      num_digits++;
      if (seen_decimal)
        num_digits_after_decimal++;
    } else if (str[i] == '.') {
      seen_decimal = 1;
    } else if (str[i] == 'e' || str[i] == 'E') {
      strtoi(&str[i + 1], len - (i + 1), &exp);
      break;
    }
  }
  num_len = i;

  if (num_digits <= 15) {
    num = 0;
    for (i = 0; i < num_len; i++) {
      if (!is_digit(str[i]))
        continue;
      num = num * 10 + (str[i] - '0');
    }
  } else {
    bignum.words[0] = 0;
    bignum.num_words = 1;

    for (i = 0; i < num_len; i++) {
      if (!is_digit(str[i]))
        continue;
      bignum_muladd(&bignum, 10, str[i] - '0');
    }

    num = bignum_to_double(&bignum);
  }

  num_digits_after_decimal -= exp;
  if (num_digits_after_decimal < 0) {
    *res = num * binexp(10.0, -num_digits_after_decimal);
  } else {
    *res = num / binexp(10.0, num_digits_after_decimal);
  }

  if (is_neg)
    *res = -*res;

  return 0;
}
