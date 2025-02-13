#ifdef __cplusplus
extern "C" {
#endif

double sin(double x);
double cos(double x);
double atan2(double y, double x);

double sqrt(double x);
double fabs(double x);

double fmax(double x, double y);
double fmin(double x, double y);

float sinf(float x);
float cosf(float x);

float sqrtf(float x);
float fabsf(float x);

float fmaxf(float x, float y);
float fminf(float x, float y);

double copysign(double x, double y);

double rint(double x);

double modf(double x, double *iptr);

#ifdef __cplusplus
}
#endif
