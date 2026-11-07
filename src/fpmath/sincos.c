#include <fpmath/fpmath.h>
#include <math.h>
#include <stdint.h>

union tab_t {
  uint64_t x[256];
  double d[256];
};

static union tab_t tab;

static double c0 = -0.16666666666666666;
static double c1 = -0.5;
static double c2 = 0.008333333333333333;
static double c3 = 0.041666666666666664;
static double c4 = -0.0001984126984126984;
static double c5 = -0.001388888888888889;
static double c6 = 2.7557319223985893e-06;
static double c7 = 2.48015873015873e-05;
static double c8 = 10.185916357881302;
static double c9 = 6755399441055744.0;
static double c10 = 3.798187816439979e-12;
static double c11 = 3.798187816439979e-12;
static double c12 = 0.09817477042088285;
static double c13 = 1.2639164054974691e-22;

static double fp_sincos(double x, int off) {
  double x0, x1, x2, x3, x4, x5, x6, x7;
  double x0h, x1h, x2h, x5h, x6h;
  int idx;
  double *ptr;

  x0 = x;
  x1 = c8;
  x1 *= x0;
  x2 = c9;
  idx = rint(x1);
  x1 += x2;
  x3 = c12;
  x1 -= x2;
  x2 = c10;
  x2h = c11;
  x3 *= x1;
  x1h = x1;
  idx += off;
  x4 = x0;
  idx &= 0x3f;
  x5 = c6;
  x5h = c7;
  ptr = &tab.d[idx << 2];
  x2 *= x1;
  x2h *= x1h;
  x0 -= x3;
  x1 *= c13;
  x4 -= x3;
  x7 = ptr[1];
  x0h = x0;
  x3 = x4;
  x4 -= x2;
  x5 *= x0;
  x5h *= x0h;
  x0 -= x2;
  x0h -= x2h;
  x6 = c2;
  x6h = c3;
  x7 *= x4;
  x3 -= x4;
  x5 *= x0;
  x5h *= x0h;
  x0 *= x0;
  x0h *= x0h;
  x3 -= x2;
  x2 = ptr[0];
  x2h = ptr[1];
  x1 -= x3;
  x3 = ptr[3];
  x2 += x3;
  x7 -= x2;
  x2 *= x4;
  x6 *= x0;
  x6h *= x0h;
  x3 *= x4;
  x2 *= x0;
  x2h *= x0h;
  x0 *= x0;
  x0h *= x0h;
  x5 += c4;
  x5h += c5;
  x4 *= ptr[0];
  x6 += c0;
  x6h += c1;
  x5 *= x0;
  x5h *= x0h;
  x0 = x3;
  x3 += ptr[1];
  x1 *= x7;
  x7 = x4;
  x4 += x3;
  x6 += x5;
  x6h += x5h;
  x5 = ptr[1];
  x5 -= x3;
  x3 -= x4;
  x1 += ptr[2];
  x6 *= x2;
  x6h *= x2h;
  x5 += x0;
  x3 += x7;
  x1 += x5;
  x1 += x3;
  x1 += x6;
  x6 = x6h;
  x1 += x6;
  x4 += x1;
  x0 = x4;
  return x0;
}

/*
 * Large arguments (#13).
 *
 * fp_sincos is the fast path of the SSE2 sin/cos in the 794-target runtime.
 * That runtime only takes the fast path while the top 16 bits of |x| are at
 * most 0x40F5, i.e. |x| < 90112. For larger |x| it falls back to the x87
 * FSIN/FCOS instructions. Those reduce the argument with a 66-bit
 * approximation of pi, pi66 = 0xC90FDAA22168C234C * 2^-66, so for large x
 * their result is close to sin(x * pi / pi66), not sin(x). The difference is
 * a few ulps, which is enough to change how a design plays out.
 *
 * x87_sincos emulates that: it evaluates sin or cos of x * pi / pi66 in
 * double-double arithmetic, then rounds the way FSIN's result reaches the
 * game: first to the x87's 64-bit significand, then to double.
 *
 * Above 2^63, FSIN does nothing and sets C2. What the runtime does then is
 * not known, so those arguments keep the fast path, as before.
 */

#define X87_THRESHOLD 90112.0
#define X87_LIMIT 9223372036854775808.0 /* 2^63 */

/* double-double helpers. These rely on every operation being rounded to
 * double on its own, so they must not be contracted into fused
 * multiply-adds (no FMA on the x86-64 baseline or wasm32). */
#ifdef __clang__
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

struct dd {
  double hi, lo;
};

static struct dd two_sum(double a, double b) {
  struct dd r;
  double bb;
  r.hi = a + b;
  bb = r.hi - a;
  r.lo = (a - (r.hi - bb)) + (b - bb);
  return r;
}

static struct dd quick_two_sum(double a, double b) {
  struct dd r;
  r.hi = a + b;
  r.lo = b - (r.hi - a);
  return r;
}

static void split(double a, double *hi, double *lo) {
  double t = 134217729.0 * a; /* 2^27 + 1 */
  *hi = t - (t - a);
  *lo = a - *hi;
}

static struct dd two_prod(double a, double b) {
  struct dd r;
  double ah, al, bh, bl;
  r.hi = a * b;
  split(a, &ah, &al);
  split(b, &bh, &bl);
  r.lo = ((ah * bh - r.hi) + ah * bl + al * bh) + al * bl;
  return r;
}

static struct dd dd_add(struct dd a, struct dd b) {
  struct dd s = two_sum(a.hi, b.hi);
  struct dd t = two_sum(a.lo, b.lo);
  s.lo += t.hi;
  s = quick_two_sum(s.hi, s.lo);
  s.lo += t.lo;
  return quick_two_sum(s.hi, s.lo);
}

static struct dd dd_mul(struct dd a, struct dd b) {
  struct dd p = two_prod(a.hi, b.hi);
  p.lo += a.hi * b.lo + a.lo * b.hi;
  return quick_two_sum(p.hi, p.lo);
}

static struct dd dd_div_int(struct dd a, double n) {
  /* n is a small integer */
  double q1 = a.hi / n;
  struct dd p = two_prod(q1, n);
  double q2 = ((a.hi - p.hi) - p.lo + a.lo) / n;
  return quick_two_sum(q1, q2);
}

/* sin(r) (want_cos == 0) or cos(r) for |r| <= ~0.8, by Taylor series */
static struct dd dd_sin_cos_small(struct dd r, int want_cos) {
  struct dd r2 = dd_mul(r, r);
  struct dd term, sum;
  int n;
  if (want_cos) {
    term.hi = 1.0;
    term.lo = 0.0;
    n = 0;
  } else {
    term = r;
    n = 1;
  }
  sum = term;
  /* 0.8^31 / 31! < 1e-36: far below double-double precision */
  while (n < 31) {
    term = dd_div_int(dd_mul(term, r2), (double)((n + 1) * (n + 2)));
    term.hi = -term.hi;
    term.lo = -term.lo;
    sum = dd_add(sum, term);
    n += 2;
  }
  return sum;
}

static uint64_t bits_of(double x) {
  union {
    double d;
    uint64_t u;
  } v;
  v.d = x;
  return v.u;
}

static double double_of(uint64_t u) {
  union {
    double d;
    uint64_t u;
  } v;
  v.u = u;
  return v.d;
}

/* round hi + lo (|lo| <= ulp(hi)/2, hi a normal double) to a 64-bit
 * significand, then to double, as when FSIN's extended result is stored to
 * a double. The x87 and the store both round to nearest, ties to even. */
static double round_x87_then_double(struct dd v) {
  uint64_t hb = bits_of(v.hi);
  if (v.lo == 0.0)
    return v.hi;
  /* the neighbouring double on lo's side: one step away from or towards
   * zero in the bit pattern */
  int away = (v.lo > 0) == (v.hi > 0);
  double other = double_of(away ? hb + 1 : hb - 1);
  double s = fabs(other - v.hi);        /* spacing towards lo, a power of two */
  double t = rint(v.lo / (s / 2048.0)); /* lo in 64-bit-significand units */
  if (fabs(t) < 1024.0)
    return v.hi;
  if (fabs(t) > 1024.0)
    return other;
  /* exactly halfway between v.hi and other: the even significand wins */
  return (hb & 1) == 0 ? v.hi : other;
}

static double x87_sincos(double x, int want_cos) {
  /* pi / pi66 - 1, as two doubles */
  static const double d_hi = 1.2874146789751208e-21;
  static const double d_lo = -6.975670213564707e-38;
  /* pi/2 as four doubles */
  static const double p1 = 1.5707963267948966;
  static const double p2 = 6.123233995736766e-17;
  static const double p3 = -1.4973849048591698e-33;
  static const double p4 = 5.562271104316826e-50;
  struct dd r, e;
  double k, k2;
  int q;

  /* The argument is x * (1 + d) = x + e. x is reduced on its own first:
   * folding the tiny e into a double-double next to a huge x would lose
   * most of e's bits. */
  e = two_prod(x, d_hi);
  e.lo += x * d_lo;

  /* r = x - k * pi/2 in double-double. two_prod makes each k * p_i exact,
   * and pi/2 to ~210 bits leaves enough after cancelling the up to ~64
   * leading bits that large k removes. */
  k = rint(x * 0.6366197723675814);
  r.hi = x;
  r.lo = 0.0;
  r = dd_add(r, two_prod(-k, p1));
  r = dd_add(r, two_prod(-k, p2));
  r = dd_add(r, two_prod(-k, p3));
  r = dd_add(r, two_prod(-k, p4));
  r = dd_add(r, e);
  /* k from x * 2/pi is only good to a few units once |x| > 2^53, so the
   * now small remainder is reduced once more */
  k2 = rint(r.hi * 0.6366197723675814);
  if (k2 != 0.0) {
    r = dd_add(r, two_prod(-k2, p1));
    r = dd_add(r, two_prod(-k2, p2));
    r = dd_add(r, two_prod(-k2, p3));
  }

  /* quadrant: sin(r + q*pi/2). k is a whole number below 2^63 and k2 is
   * small, so both convert to int64_t exactly, and & 3 is the sum mod 4
   * even when negative */
  q = (int)(((int64_t)k + (int64_t)k2) & 3);
  if (want_cos)
    q = (q + 1) & 3;
  {
    struct dd v = dd_sin_cos_small(r, q & 1);
    if (q & 2) {
      v.hi = -v.hi;
      v.lo = -v.lo;
    }
    return round_x87_then_double(v);
  }
}

static int x87_range(double x) {
  double ax = fabs(x);
  return ax >= X87_THRESHOLD && ax < X87_LIMIT;
}

double fp_sin(double x) {
  return x87_range(x) ? x87_sincos(x, 0) : fp_sincos(x, 0);
}

double fp_cos(double x) {
  return x87_range(x) ? x87_sincos(x, 1) : fp_sincos(x, 0x10);
}

static union tab_t tab = {
    .x = {
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x3FF0000000000000, 0xBF73B92E176D6D31, 0x3FB917A6BC29B42C,
        0xBC3E2718E0000000, 0x3FF0000000000000, 0xBF93AD06011469FB,
        0x3FC8F8B83C69A60B, 0xBC626D19C0000000, 0x3FF0000000000000,
        0xBFA60BEA939D225A, 0x3FD294062ED59F06, 0xBC75D28DA0000000,
        0x3FF0000000000000, 0xBFB37CA1866B95CF, 0x3FD87DE2A6AEA963,
        0xBC672CEDE0000000, 0x3FF0000000000000, 0xBFBE3A6873FA1279,
        0x3FDE2B5D3806F63B, 0x3C5E0D8920000000, 0x3FF0000000000000,
        0xBFC592675BC57974, 0x3FE1C73B39AE68C8, 0x3C8B25DD20000000,
        0x3FF0000000000000, 0xBFCD0DFE53ABA2FD, 0x3FE44CF325091DD6,
        0x3C68076A20000000, 0x3FF0000000000000, 0x3FCA827999FCEF32,
        0x3FE6A09E667F3BCD, 0xBC8BDD3420000000, 0x3FE0000000000000,
        0x3FC133CC94247758, 0x3FE8BC806B151741, 0xBC82C5E120000000,
        0x3FE0000000000000, 0x3FAC73B39AE68C87, 0x3FEA9B66290EA1A3,
        0x3C39F630E0000000, 0x3FE0000000000000, 0xBF9D4A2C7F909C4E,
        0x3FEC38B2F180BDB1, 0xBC76E0B180000000, 0x3FE0000000000000,
        0xBFBE087565455A75, 0x3FED906BCF328D46, 0x3C7457E620000000,
        0x3FE0000000000000, 0x3FA4A03176ACF82D, 0x3FEE9F4156C62DDA,
        0x3C8760B1E0000000, 0x3FD0000000000000, 0xBFAC1D1F0E5967D5,
        0x3FEF6297CFF75CB0, 0x3C75621720000000, 0x3FD0000000000000,
        0xBF9BA1650F592F50, 0x3FEFD88DA3D12526, 0xBC887DF640000000,
        0x3FC0000000000000, 0x0000000000000000, 0x3FF0000000000000,
        0x0000000000000000, 0x0000000000000000, 0x3F9BA1650F592F50,
        0x3FEFD88DA3D12526, 0xBC887DF640000000, 0xBFC0000000000000,
        0x3FAC1D1F0E5967D5, 0x3FEF6297CFF75CB0, 0x3C75621720000000,
        0xBFD0000000000000, 0xBFA4A03176ACF82D, 0x3FEE9F4156C62DDA,
        0x3C8760B1E0000000, 0xBFD0000000000000, 0x3FBE087565455A75,
        0x3FED906BCF328D46, 0x3C7457E620000000, 0xBFE0000000000000,
        0x3F9D4A2C7F909C4E, 0x3FEC38B2F180BDB1, 0xBC76E0B180000000,
        0xBFE0000000000000, 0xBFAC73B39AE68C87, 0x3FEA9B66290EA1A3,
        0x3C39F630E0000000, 0xBFE0000000000000, 0xBFC133CC94247758,
        0x3FE8BC806B151741, 0xBC82C5E120000000, 0xBFE0000000000000,
        0xBFCA827999FCEF32, 0x3FE6A09E667F3BCD, 0xBC8BDD3420000000,
        0xBFE0000000000000, 0x3FCD0DFE53ABA2FD, 0x3FE44CF325091DD6,
        0x3C68076A20000000, 0xBFF0000000000000, 0x3FC592675BC57974,
        0x3FE1C73B39AE68C8, 0x3C8B25DD20000000, 0xBFF0000000000000,
        0x3FBE3A6873FA1279, 0x3FDE2B5D3806F63B, 0x3C5E0D8920000000,
        0xBFF0000000000000, 0x3FB37CA1866B95CF, 0x3FD87DE2A6AEA963,
        0xBC672CEDE0000000, 0xBFF0000000000000, 0x3FA60BEA939D225A,
        0x3FD294062ED59F06, 0xBC75D28DA0000000, 0xBFF0000000000000,
        0x3F93AD06011469FB, 0x3FC8F8B83C69A60B, 0xBC626D19C0000000,
        0xBFF0000000000000, 0x3F73B92E176D6D31, 0x3FB917A6BC29B42C,
        0xBC3E2718E0000000, 0xBFF0000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0xBFF0000000000000,
        0x3F73B92E176D6D31, 0xBFB917A6BC29B42C, 0x3C3E2718E0000000,
        0xBFF0000000000000, 0x3F93AD06011469FB, 0xBFC8F8B83C69A60B,
        0x3C626D19C0000000, 0xBFF0000000000000, 0x3FA60BEA939D225A,
        0xBFD294062ED59F06, 0x3C75D28DA0000000, 0xBFF0000000000000,
        0x3FB37CA1866B95CF, 0xBFD87DE2A6AEA963, 0x3C672CEDE0000000,
        0xBFF0000000000000, 0x3FBE3A6873FA1279, 0xBFDE2B5D3806F63B,
        0xBC5E0D8920000000, 0xBFF0000000000000, 0x3FC592675BC57974,
        0xBFE1C73B39AE68C8, 0xBC8B25DD20000000, 0xBFF0000000000000,
        0x3FCD0DFE53ABA2FD, 0xBFE44CF325091DD6, 0xBC68076A20000000,
        0xBFF0000000000000, 0xBFCA827999FCEF32, 0xBFE6A09E667F3BCD,
        0x3C8BDD3420000000, 0xBFE0000000000000, 0xBFC133CC94247758,
        0xBFE8BC806B151741, 0x3C82C5E120000000, 0xBFE0000000000000,
        0xBFAC73B39AE68C87, 0xBFEA9B66290EA1A3, 0xBC39F630E0000000,
        0xBFE0000000000000, 0x3F9D4A2C7F909C4E, 0xBFEC38B2F180BDB1,
        0x3C76E0B180000000, 0xBFE0000000000000, 0x3FBE087565455A75,
        0xBFED906BCF328D46, 0xBC7457E620000000, 0xBFE0000000000000,
        0xBFA4A03176ACF82D, 0xBFEE9F4156C62DDA, 0xBC8760B1E0000000,
        0xBFD0000000000000, 0x3FAC1D1F0E5967D5, 0xBFEF6297CFF75CB0,
        0xBC75621720000000, 0xBFD0000000000000, 0x3F9BA1650F592F50,
        0xBFEFD88DA3D12526, 0x3C887DF640000000, 0xBFC0000000000000,
        0x0000000000000000, 0xBFF0000000000000, 0x0000000000000000,
        0x0000000000000000, 0xBF9BA1650F592F50, 0xBFEFD88DA3D12526,
        0x3C887DF640000000, 0x3FC0000000000000, 0xBFAC1D1F0E5967D5,
        0xBFEF6297CFF75CB0, 0xBC75621720000000, 0x3FD0000000000000,
        0x3FA4A03176ACF82D, 0xBFEE9F4156C62DDA, 0xBC8760B1E0000000,
        0x3FD0000000000000, 0xBFBE087565455A75, 0xBFED906BCF328D46,
        0xBC7457E620000000, 0x3FE0000000000000, 0xBF9D4A2C7F909C4E,
        0xBFEC38B2F180BDB1, 0x3C76E0B180000000, 0x3FE0000000000000,
        0x3FAC73B39AE68C87, 0xBFEA9B66290EA1A3, 0xBC39F630E0000000,
        0x3FE0000000000000, 0x3FC133CC94247758, 0xBFE8BC806B151741,
        0x3C82C5E120000000, 0x3FE0000000000000, 0x3FCA827999FCEF32,
        0xBFE6A09E667F3BCD, 0x3C8BDD3420000000, 0x3FE0000000000000,
        0xBFCD0DFE53ABA2FD, 0xBFE44CF325091DD6, 0xBC68076A20000000,
        0x3FF0000000000000, 0xBFC592675BC57974, 0xBFE1C73B39AE68C8,
        0xBC8B25DD20000000, 0x3FF0000000000000, 0xBFBE3A6873FA1279,
        0xBFDE2B5D3806F63B, 0xBC5E0D8920000000, 0x3FF0000000000000,
        0xBFB37CA1866B95CF, 0xBFD87DE2A6AEA963, 0x3C672CEDE0000000,
        0x3FF0000000000000, 0xBFA60BEA939D225A, 0xBFD294062ED59F06,
        0x3C75D28DA0000000, 0x3FF0000000000000, 0xBF93AD06011469FB,
        0xBFC8F8B83C69A60B, 0x3C626D19C0000000, 0x3FF0000000000000,
        0xBF73B92E176D6D31, 0xBFB917A6BC29B42C, 0x3C3E2718E0000000,
        0x3FF0000000000000,
    }};
