/*
 * Checks fcsim's sin/cos (src/fpmath/sincos.c) against the x87 FSIN/FCOS
 * instructions, for spectre s794-fsin (#13). Not part of the build or CI.
 *
 * Build (x86 or x86-64; gcc or clang):
 *   cc -O2 -ffp-contract=off -Iinclude tools/fsin_check.c -lm -o fsin_check
 *
 * Modes:
 *   fsin_check info
 *       CPU and a few sample values.
 *   fsin_check verify [SECONDS] [SEED]
 *       Random |x| in [1, 2^63), log-uniform, both signs: the emulation
 *       (x87_sincos) must equal the hardware for sin and cos. SECONDS = 0
 *       (default) runs until interrupted. Prints every mismatch.
 *   fsin_check scan START COUNT
 *       Exhaustive: COUNT consecutive doubles from START upwards. Reports
 *       (X, Y): every x in [X, Y) matches, and Y is the first mismatch (or
 *       none was found). Can be resumed from the last value it prints.
 *   fsin_check survey [SAMPLES]
 *       Per binade [2^e, 2^(e+1)), e = 0..62: how often the fast path
 *       (fp_sincos) and the emulation each differ from the hardware.
 *   fsin_check candidates
 *       No hardware needed. For each candidate switch point between 6.1
 *       and 90500, how often s794-fsin and s794-stable differ just above
 *       it, and the first angle where they do. For building FC test designs
 *       to pin down the real switch point.
 */

#define USE_FSIN
#include "../src/fpmath/sincos.c"

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__i386__) || defined(__x86_64__)
#define HAVE_X87 1
#include <cpuid.h>

static double hw_sincos(double x, int want_cos) {
  long double r;
  if (want_cos)
    __asm__("fcos" : "=t"(r) : "0"((long double)x));
  else
    __asm__("fsin" : "=t"(r) : "0"((long double)x));
  /* FSIN/FCOS ignore the precision control and return a 64-bit
   * significand; this store rounds it to double */
  return (double)r;
}
#else
#define HAVE_X87 0
static double hw_sincos(double x, int want_cos) {
  (void)x;
  (void)want_cos;
  return 0.0;
}
#endif

static double emu_sincos(double x, int want_cos) {
  return x87_sincos(x, want_cos);
}

static double fast_sincos(double x, int want_cos) {
  return fp_sincos(x, want_cos ? 0x10 : 0);
}

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;

static uint64_t rng_next(void) {
  /* xorshift64* */
  rng_state ^= rng_state >> 12;
  rng_state ^= rng_state << 25;
  rng_state ^= rng_state >> 27;
  return rng_state * 0x2545F4914F6CDD1Dull;
}

/* random double in [2^elo, 2^(ehi+1)), log-uniform over binades */
static double rng_binades(int elo, int ehi) {
  uint64_t e = (uint64_t)(elo + (int)(rng_next() % (uint64_t)(ehi - elo + 1)));
  uint64_t bits = ((e + 1023) << 52) | (rng_next() & 0xFFFFFFFFFFFFFull);
  return double_of(bits);
}

static double now_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static volatile sig_atomic_t interrupted = 0;

static void on_sigint(int sig) {
  (void)sig;
  interrupted = 1;
}

static int need_x87(void) {
  if (!HAVE_X87) {
    fprintf(stderr, "this mode needs an x86 CPU (x87 FSIN/FCOS)\n");
    return 1;
  }
  return 0;
}

static void print_cpu(void) {
#if HAVE_X87
  unsigned a, b, c, d;
  char vendor[13] = {0};
  char brand[49] = {0};
  if (__get_cpuid(0, &a, &b, &c, &d)) {
    memcpy(vendor, &b, 4);
    memcpy(vendor + 4, &d, 4);
    memcpy(vendor + 8, &c, 4);
  }
  for (unsigned i = 0; i < 3; i++)
    if (__get_cpuid(0x80000002 + i, &a, &b, &c, &d)) {
      memcpy(brand + 16 * i, &a, 4);
      memcpy(brand + 16 * i + 4, &b, 4);
      memcpy(brand + 16 * i + 8, &c, 4);
      memcpy(brand + 16 * i + 12, &d, 4);
    }
  printf("cpu: %s / %s\n", vendor, brand);
#else
  printf("cpu: not x86\n");
#endif
}

static void print_mismatch(const char *what, double x, int want_cos, double hw,
                           double other) {
  printf("MISMATCH %s %s x=%.17g (%a) hw=%a %s=%a\n", what,
         want_cos ? "cos" : "sin", x, x, hw, what, other);
}

static int mode_info(void) {
  static const double xs[] = {0.5,    6.1,         90112.0, 90500.0,
                              1e6,    123456789.0, 4.5e15,  9.2e18,
                              -90500, -1e6,        -7.7e17};
  print_cpu();
  if (!HAVE_X87)
    return 0;
  printf("%-22s %-4s %-24s %-24s %-24s\n", "x", "fn", "hardware", "emulation",
         "fast path");
  for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++)
    for (int c = 0; c < 2; c++)
      printf("%-22.17g %-4s %-24a %-24a %-24a\n", xs[i], c ? "cos" : "sin",
             hw_sincos(xs[i], c), emu_sincos(xs[i], c), fast_sincos(xs[i], c));
  return 0;
}

static int mode_verify(double seconds, uint64_t seed) {
  uint64_t n = 0, bad = 0;
  double start = now_s(), last = start;
  if (need_x87())
    return 2;
  rng_state = seed ? seed : 0x9E3779B97F4A7C15ull;
  print_cpu();
  printf("verify: emulation vs hardware, |x| in [1, 2^63), seed %" PRIu64
         ", %s\n",
         seed, seconds > 0 ? "timed" : "until Ctrl-C");
  fflush(stdout);
  signal(SIGINT, on_sigint);
  while (!interrupted) {
    double x = rng_binades(0, 62);
    if (rng_next() & 1)
      x = -x;
    for (int c = 0; c < 2; c++) {
      double hw = hw_sincos(x, c), emu = emu_sincos(x, c);
      if (bits_of(hw) != bits_of(emu)) {
        bad++;
        print_mismatch("emu", x, c, hw, emu);
        fflush(stdout);
      }
    }
    n++;
    if ((n & 0xFFFFF) == 0) {
      double t = now_s();
      if (t - last >= 60 || (seconds > 0 && t - start >= seconds)) {
        printf("progress: %" PRIu64 " inputs (x2 functions), %" PRIu64
               " mismatches, %.0f s\n",
               n, bad, t - start);
        fflush(stdout);
        last = t;
      }
      if (seconds > 0 && t - start >= seconds)
        break;
    }
  }
  printf("RESULT verify: %" PRIu64 " inputs (sin and cos each), %" PRIu64
         " mismatches, seed %" PRIu64 "\n",
         n, bad, seed);
  return bad ? 1 : 0;
}

static int mode_scan(double start, uint64_t count) {
  uint64_t bits, i;
  double t0 = now_s(), last = t0;
  if (need_x87())
    return 2;
  if (!(start >= 1.0 && start < X87_LIMIT)) {
    fprintf(stderr, "START must be in [1, 2^63)\n");
    return 2;
  }
  print_cpu();
  signal(SIGINT, on_sigint);
  bits = bits_of(start);
  for (i = 0; i < count && !interrupted; i++, bits++) {
    double x = double_of(bits);
    if (x >= X87_LIMIT)
      break;
    for (int c = 0; c < 2; c++) {
      double hw = hw_sincos(x, c), emu = emu_sincos(x, c);
      if (bits_of(hw) != bits_of(emu)) {
        print_mismatch("emu", x, c, hw, emu);
        printf("RESULT scan: (X, Y) = (%.17g, %.17g): every x in [X, Y) "
               "matches, Y does not\n",
               start, x);
        return 1;
      }
    }
    if ((i & 0xFFFFFF) == 0xFFFFFF) {
      double t = now_s();
      if (t - last >= 60) {
        printf("progress: %" PRIu64 " checked, next %.17g (%a), %.0f s\n",
               i + 1, double_of(bits + 1), double_of(bits + 1), t - t0);
        fflush(stdout);
        last = t;
      }
    }
  }
  printf("RESULT scan: no mismatch in [%.17g, %.17g) (%" PRIu64
         " doubles); resume from %a\n",
         start, double_of(bits), i, double_of(bits));
  return 0;
}

static int mode_survey(uint64_t samples) {
  if (need_x87())
    return 2;
  print_cpu();
  printf("%-6s %-24s %-14s %-14s\n", "e", "binade start", "fast != hw",
         "emu != hw");
  for (int e = 0; e <= 62; e++) {
    uint64_t fast_bad = 0, emu_bad = 0;
    for (uint64_t i = 0; i < samples; i++) {
      double x = rng_binades(e, e);
      for (int c = 0; c < 2; c++) {
        uint64_t hw = bits_of(hw_sincos(x, c));
        fast_bad += bits_of(fast_sincos(x, c)) != hw;
        emu_bad += bits_of(emu_sincos(x, c)) != hw;
      }
    }
    printf("%-6d %-24.17g %-14.6f %-14.6f\n", e, ldexp(1.0, e),
           (double)fast_bad / (2.0 * (double)samples),
           (double)emu_bad / (2.0 * (double)samples));
    fflush(stdout);
  }
  return 0;
}

/* The fast path's range check compares the top 16 bits of |x| (sign
 * cleared), so the possible switch points are the doubles whose low 48
 * bits are zero. */
static int mode_candidates(void) {
  const uint64_t samples = 20000;
  uint64_t lo = bits_of(6.1) >> 48, hi = bits_of(90500.0) >> 48;
  printf("%-8s %-24s %-12s %-24s\n", "top16", "switch point T", "differ",
         "first differing angle >= T");
  for (uint64_t h = lo; h <= hi; h++) {
    double t = double_of(h << 48), next = double_of((h + 1) << 48);
    uint64_t differ = 0;
    double first = 0.0;
    for (uint64_t i = 0; i < samples; i++) {
      double x = t + (next - t) * ((double)i / (double)samples);
      int d = 0;
      for (int c = 0; c < 2; c++)
        d |= bits_of(emu_sincos(x, c)) != bits_of(fast_sincos(x, c));
      if (d && !differ)
        first = x;
      differ += d;
    }
    printf("0x%04" PRIx64 "   %-24.17g %-12.4f %-24.17g\n", h, t,
           (double)differ / (double)samples, first);
  }
  return 0;
}

int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "";
  if (!strcmp(mode, "info"))
    return mode_info();
  if (!strcmp(mode, "verify"))
    return mode_verify(argc > 2 ? strtod(argv[2], 0) : 0.0,
                       argc > 3 ? strtoull(argv[3], 0, 0) : 1);
  if (!strcmp(mode, "scan") && argc > 3)
    return mode_scan(strtod(argv[2], 0), strtoull(argv[3], 0, 0));
  if (!strcmp(mode, "survey"))
    return mode_survey(argc > 2 ? strtoull(argv[2], 0, 0) : 100000);
  if (!strcmp(mode, "candidates"))
    return mode_candidates();
  fprintf(stderr,
          "usage: %s info | verify [SECONDS] [SEED] | scan START "
          "COUNT | survey [SAMPLES] | candidates\n",
          argv[0]);
  return 2;
}
