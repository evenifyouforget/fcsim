/*
 * Checks fcsim's sin/cos (src/fpmath/sincos.c) against the x87 FSIN/FCOS
 * instructions, for spectre s794-fsin (#13). Not part of the build or CI.
 *
 * Build (x86 or x86-64; gcc or clang):
 *   c++ -O2 -ffp-contract=off -Iinclude tools/fsin_check.cpp -o fsin_check
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
 *   fsin_check dump sin|cos FILE COUNT [SEED]
 *       Writes COUNT records of the hardware FSIN (or FCOS) to FILE, to send
 *       to someone else. Each record is 16 bytes: the bits of x, then the bits
 *       of the hardware result, both as little-endian 64-bit integers. The x
 *       values are |x| in [1, 2^63), log-uniform, both signs, and depend only
 *       on SEED (default 1), so files from different computers line up.
 *   fsin_check compare sin|cos FILE [FILE2]
 *       Needs no x87 hardware. With one FILE, counts how often the emulation
 *       and the fast path (the software sin/cos, s794-stable) differ from the
 *       recorded results. With two files, compares them record by record
 *       (their x values must match). Prints the first mismatches. Exit status
 *       1 if the emulation (one file) or the two files (two files) differ.
 *   fsin_check candidates
 *       No hardware needed. For each candidate switch point between 6.1
 *       and 90500, how often s794-fsin and s794-stable differ just above
 *       it, and the first angle where they do. For building FC test designs
 *       to pin down the real switch point.
 */

#define USE_FSIN
#include "../src/fpmath/sincos.c"

#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

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

static volatile std::sig_atomic_t interrupted = 0;

static void on_sigint(int sig) {
  (void)sig;
  interrupted = 1;
}

static int need_x87(void) {
  if (!HAVE_X87) {
    std::fprintf(stderr, "this mode needs an x86 CPU (x87 FSIN/FCOS)\n");
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
    std::memcpy(vendor, &b, 4);
    std::memcpy(vendor + 4, &d, 4);
    std::memcpy(vendor + 8, &c, 4);
  }
  for (unsigned i = 0; i < 3; i++)
    if (__get_cpuid(0x80000002 + i, &a, &b, &c, &d)) {
      std::memcpy(brand + 16 * i, &a, 4);
      std::memcpy(brand + 16 * i + 4, &b, 4);
      std::memcpy(brand + 16 * i + 8, &c, 4);
      std::memcpy(brand + 16 * i + 12, &d, 4);
    }
  std::printf("cpu: %s / %s\n", vendor, brand);
#else
  std::printf("cpu: not x86\n");
#endif
}

static void print_mismatch(const char *what, double x, int want_cos, double hw,
                           double other) {
  std::printf("MISMATCH %s %s x=%.17g (%a) hw=%a %s=%a\n", what,
              want_cos ? "cos" : "sin", x, x, hw, what, other);
}

static int mode_info(void) {
  static const double xs[] = {0.5,    6.1,         90112.0, 90500.0,
                              1e6,    123456789.0, 4.5e15,  9.2e18,
                              -90500, -1e6,        -7.7e17};
  print_cpu();
  if (!HAVE_X87)
    return 0;
  std::printf("%-22s %-4s %-24s %-24s %-24s\n", "x", "fn", "hardware",
              "emulation", "fast path");
  for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++)
    for (int c = 0; c < 2; c++)
      std::printf("%-22.17g %-4s %-24a %-24a %-24a\n", xs[i], c ? "cos" : "sin",
                  hw_sincos(xs[i], c), emu_sincos(xs[i], c),
                  fast_sincos(xs[i], c));
  return 0;
}

static int mode_verify(double seconds, uint64_t seed) {
  uint64_t n = 0, bad = 0;
  double start = now_s(), last = start;
  if (need_x87())
    return 2;
  rng_state = seed ? seed : 0x9E3779B97F4A7C15ull;
  print_cpu();
  std::printf("verify: emulation vs hardware, |x| in [1, 2^63), seed %" PRIu64
              ", %s\n",
              seed, seconds > 0 ? "timed" : "until Ctrl-C");
  std::fflush(stdout);
  std::signal(SIGINT, on_sigint);
  while (!interrupted) {
    double x = rng_binades(0, 62);
    if (rng_next() & 1)
      x = -x;
    for (int c = 0; c < 2; c++) {
      double hw = hw_sincos(x, c), emu = emu_sincos(x, c);
      if (bits_of(hw) != bits_of(emu)) {
        bad++;
        print_mismatch("emu", x, c, hw, emu);
        std::fflush(stdout);
      }
    }
    n++;
    if ((n & 0xFFFFF) == 0) {
      double t = now_s();
      if (t - last >= 60 || (seconds > 0 && t - start >= seconds)) {
        std::printf("progress: %" PRIu64 " inputs (x2 functions), %" PRIu64
                    " mismatches, %.0f s\n",
                    n, bad, t - start);
        std::fflush(stdout);
        last = t;
      }
      if (seconds > 0 && t - start >= seconds)
        break;
    }
  }
  std::printf("RESULT verify: %" PRIu64 " inputs (sin and cos each), %" PRIu64
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
    std::fprintf(stderr, "START must be in [1, 2^63)\n");
    return 2;
  }
  print_cpu();
  std::signal(SIGINT, on_sigint);
  bits = bits_of(start);
  for (i = 0; i < count && !interrupted; i++, bits++) {
    double x = double_of(bits);
    if (x >= X87_LIMIT)
      break;
    for (int c = 0; c < 2; c++) {
      double hw = hw_sincos(x, c), emu = emu_sincos(x, c);
      if (bits_of(hw) != bits_of(emu)) {
        print_mismatch("emu", x, c, hw, emu);
        std::printf("RESULT scan: (X, Y) = (%.17g, %.17g): every x in [X, Y) "
                    "matches, Y does not\n",
                    start, x);
        return 1;
      }
    }
    if ((i & 0xFFFFFF) == 0xFFFFFF) {
      double t = now_s();
      if (t - last >= 60) {
        std::printf("progress: %" PRIu64 " checked, next %.17g (%a), %.0f s\n",
                    i + 1, double_of(bits + 1), double_of(bits + 1), t - t0);
        std::fflush(stdout);
        last = t;
      }
    }
  }
  std::printf("RESULT scan: no mismatch in [%.17g, %.17g) (%" PRIu64
              " doubles); resume from %a\n",
              start, double_of(bits), i, double_of(bits));
  return 0;
}

static int mode_survey(uint64_t samples) {
  if (need_x87())
    return 2;
  print_cpu();
  std::printf("%-6s %-24s %-14s %-14s\n", "e", "binade start", "fast != hw",
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
    std::printf("%-6d %-24.17g %-14.6f %-14.6f\n", e, std::ldexp(1.0, e),
                (double)fast_bad / (2.0 * (double)samples),
                (double)emu_bad / (2.0 * (double)samples));
    std::fflush(stdout);
  }
  return 0;
}

/* 0 for sin, 1 for cos, -1 for anything else */
static int parse_fn(const char *s) {
  if (!std::strcmp(s, "sin"))
    return 0;
  if (!std::strcmp(s, "cos"))
    return 1;
  return -1;
}

/* Record files: 16 bytes per entry, little-endian x bits then result bits. */
static const int RECORD_BYTES = 16;

static void put_le64(unsigned char *p, uint64_t v) {
  for (int i = 0; i < 8; i++)
    p[i] = static_cast<unsigned char>(v >> (8 * i));
}

static uint64_t get_le64(const unsigned char *p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++)
    v |= static_cast<uint64_t>(p[i]) << (8 * i);
  return v;
}

/* The i-th x of the sequence for a seed. Integer-only, so it is the same on
 * every computer. */
static double next_record_x() {
  double x = rng_binades(0, 62);
  if (rng_next() & 1)
    x = -x;
  return x;
}

static int mode_dump(int want_cos, const char *path, uint64_t count,
                     uint64_t seed) {
  if (need_x87())
    return 2;
  std::FILE *f = std::fopen(path, "wb");
  if (!f) {
    std::fprintf(stderr, "cannot write %s\n", path);
    return 2;
  }
  rng_state = seed ? seed : 0x9E3779B97F4A7C15ull;
  print_cpu();
  for (uint64_t i = 0; i < count; i++) {
    unsigned char rec[16];
    double x = next_record_x();
    put_le64(rec, bits_of(x));
    put_le64(rec + 8, bits_of(hw_sincos(x, want_cos)));
    if (std::fwrite(rec, 1, sizeof rec, f) != sizeof rec) {
      std::fprintf(stderr, "write error on %s\n", path);
      std::fclose(f);
      return 2;
    }
  }
  if (std::fclose(f) != 0) {
    std::fprintf(stderr, "write error on %s\n", path);
    return 2;
  }
  std::printf("RESULT dump: %s, %" PRIu64 " records of %s (%" PRIu64
              " bytes), seed %" PRIu64 "\n",
              path, count, want_cos ? "cos" : "sin", count * RECORD_BYTES,
              seed);
  return 0;
}

/* reads the next record; returns 1, 0 at a clean end of file, -1 on a short
 * or failed read */
static int read_record(std::FILE *f, uint64_t *x_bits, uint64_t *res_bits) {
  unsigned char rec[16];
  size_t n = std::fread(rec, 1, sizeof rec, f);
  if (n == 0 && !std::ferror(f))
    return 0;
  if (n != sizeof rec)
    return -1;
  *x_bits = get_le64(rec);
  *res_bits = get_le64(rec + 8);
  return 1;
}

static const int MAX_SHOWN = 10;

static int mode_compare(int want_cos, const char *path_a, const char *path_b) {
  const char *fn = want_cos ? "cos" : "sin";
  std::FILE *fa = std::fopen(path_a, "rb");
  std::FILE *fb = path_b ? std::fopen(path_b, "rb") : nullptr;
  if (!fa || (path_b && !fb)) {
    std::fprintf(stderr, "cannot read %s\n", fa ? path_b : path_a);
    return 2;
  }
  uint64_t n = 0, emu_bad = 0, fast_bad = 0, differ = 0;
  uint64_t xa, ra, xb, rb;
  for (;;) {
    int ga = read_record(fa, &xa, &ra);
    int gb = fb ? read_record(fb, &xb, &rb) : 0;
    if (ga < 0 || gb < 0 || (fb && ga != gb)) {
      std::fprintf(stderr,
                   "bad or mismatched file length after %" PRIu64
                   " records (each record is %d bytes)\n",
                   n, RECORD_BYTES);
      return 2;
    }
    if (ga == 0)
      break;
    double x = double_of(xa);
    if (fb) {
      if (xa != xb) {
        std::fprintf(stderr,
                     "record %" PRIu64 ": the files have different x (%a vs "
                     "%a); use the same SEED\n",
                     n, x, double_of(xb));
        return 2;
      }
      if (ra != rb) {
        if (differ++ < MAX_SHOWN)
          std::printf("DIFFER %s x=%.17g (%a) a=%a b=%a\n", fn, x, x,
                      double_of(ra), double_of(rb));
      }
    } else {
      uint64_t emu = bits_of(emu_sincos(x, want_cos));
      uint64_t fast = bits_of(fast_sincos(x, want_cos));
      if (emu != ra) {
        if (emu_bad++ < MAX_SHOWN)
          std::printf("MISMATCH emu %s x=%.17g (%a) file=%a emu=%a\n", fn, x, x,
                      double_of(ra), double_of(emu));
      }
      fast_bad += fast != ra;
    }
    n++;
  }
  std::fclose(fa);
  if (fb)
    std::fclose(fb);
  if (fb) {
    std::printf("RESULT compare: %" PRIu64 " records of %s, %" PRIu64
                " differ between the files\n",
                n, fn, differ);
    return differ ? 1 : 0;
  }
  std::printf("RESULT compare: %" PRIu64 " records of %s: emulation differs "
              "from the file in %" PRIu64
              ", fast path (s794-stable) in %" PRIu64 "\n",
              n, fn, emu_bad, fast_bad);
  return emu_bad ? 1 : 0;
}

/* The fast path's range check compares the top 16 bits of |x| (sign
 * cleared), so the possible switch points are the doubles whose low 48
 * bits are zero. */
static int mode_candidates(void) {
  const uint64_t samples = 20000;
  uint64_t lo = bits_of(6.1) >> 48, hi = bits_of(90500.0) >> 48;
  std::printf("%-8s %-24s %-12s %-24s\n", "top16", "switch point T", "differ",
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
    std::printf("0x%04" PRIx64 "   %-24.17g %-12.4f %-24.17g\n", h, t,
                (double)differ / (double)samples, first);
  }
  return 0;
}

int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "";
  if (!std::strcmp(mode, "info"))
    return mode_info();
  if (!std::strcmp(mode, "verify"))
    return mode_verify(argc > 2 ? std::strtod(argv[2], 0) : 0.0,
                       argc > 3 ? std::strtoull(argv[3], 0, 0) : 1);
  if (!std::strcmp(mode, "scan") && argc > 3)
    return mode_scan(std::strtod(argv[2], 0), std::strtoull(argv[3], 0, 0));
  if (!std::strcmp(mode, "survey"))
    return mode_survey(argc > 2 ? std::strtoull(argv[2], 0, 0) : 100000);
  if (!std::strcmp(mode, "dump") && argc > 4 && parse_fn(argv[2]) >= 0)
    return mode_dump(parse_fn(argv[2]), argv[3], std::strtoull(argv[4], 0, 0),
                     argc > 5 ? std::strtoull(argv[5], 0, 0) : 1);
  if (!std::strcmp(mode, "compare") && argc > 3 && parse_fn(argv[2]) >= 0)
    return mode_compare(parse_fn(argv[2]), argv[3],
                        argc > 4 ? argv[4] : nullptr);
  if (!std::strcmp(mode, "candidates"))
    return mode_candidates();
  std::fprintf(stderr,
               "usage: %s info | verify [SECONDS] [SEED] | scan START "
               "COUNT | survey [SAMPLES] | dump sin|cos FILE COUNT [SEED] | "
               "compare sin|cos FILE [FILE2] | candidates\n",
               argv[0]);
  return 2;
}
