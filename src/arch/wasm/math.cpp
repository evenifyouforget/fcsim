#include <fpmath/fpmath.h>

// Stub declarations: provided by the WASM runtime, not implemented here.
extern "C" double sqrt(double x);
extern "C" double fabs(double x);
extern "C" double copysign(double x, double y);
extern "C" double rint(double x);
extern "C" float fabsf(float x);
extern "C" double floor(double x);

extern "C" {

double sin(double x) { return fp_sin(x); }
double cos(double x) { return fp_cos(x); }
double atan2(double y, double x) { return fp_atan2(y, x); }
double fmax(double x, double y) { return x > y ? x : y; }
double fmin(double x, double y) { return x < y ? x : y; }

float sinf(float x) { return fp_sin(x); }
float cosf(float x) { return fp_cos(x); }
float fmaxf(float x, float y) { return x > y ? x : y; }
float fminf(float x, float y) { return x < y ? x : y; }

double modf(double x, double *iptr) {
  *iptr = floor(x);
  return x - *iptr;
}

} // extern "C"
