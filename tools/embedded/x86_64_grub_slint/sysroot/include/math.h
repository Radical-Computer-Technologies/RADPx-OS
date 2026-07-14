#ifndef RADIX_SYSROOT_MATH_H
#define RADIX_SYSROOT_MATH_H

#define HUGE_VAL (__builtin_huge_val())
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))

double floor(double x);
double ceil(double x);
double fabs(double x);
double pow(double x, double y);
double log10(double x);
int isinf(double x);
int isnan(double x);
int signbit(double x);

#endif
