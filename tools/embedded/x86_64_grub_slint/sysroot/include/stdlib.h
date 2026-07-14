#ifndef RADIX_SYSROOT_STDLIB_H
#define RADIX_SYSROOT_STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define MB_CUR_MAX 4

void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);
int atoi(const char *nptr);
long atol(const char *nptr);
double atof(const char *nptr);
int abs(int n);
long labs(long n);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
char *getenv(const char *name);
int putenv(char *string);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
const char *getprogname(void);
void setprogname(const char *name);
char *realpath(const char *path, char *resolved_path);
int mkstemp(char *template);
int mkstemps(char *template, int suffixlen);

#endif
