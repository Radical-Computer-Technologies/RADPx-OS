#include <stddef.h>

void *memset(void *dest, int value, size_t count) {
    unsigned char *out = (unsigned char *)dest;
    for (size_t i = 0; i < count; ++i) out[i] = (unsigned char)value;
    return dest;
}

void *memcpy(void *dest, const void *src, size_t count) {
    unsigned char *out = (unsigned char *)dest;
    const unsigned char *in = (const unsigned char *)src;
    for (size_t i = 0; i < count; ++i) out[i] = in[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    unsigned char *out = (unsigned char *)dest;
    const unsigned char *in = (const unsigned char *)src;
    if (out < in) {
        for (size_t i = 0; i < count; ++i) out[i] = in[i];
    } else if (out > in) {
        for (size_t i = count; i > 0; --i) out[i - 1u] = in[i - 1u];
    }
    return dest;
}

int memcmp(const void *lhs, const void *rhs, size_t count) {
    const unsigned char *a = (const unsigned char *)lhs;
    const unsigned char *b = (const unsigned char *)rhs;
    for (size_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}

size_t strlen(const char *text) {
    size_t length = 0;
    while (text && text[length]) ++length;
    return length;
}

char *strcpy(char *dest, const char *src) {
    char *out = dest;
    while ((*out++ = *src++) != '\0') {}
    return dest;
}

char *strncpy(char *dest, const char *src, size_t count) {
    size_t i = 0;
    for (; i < count && src[i]; ++i) dest[i] = src[i];
    for (; i < count; ++i) dest[i] = '\0';
    return dest;
}

int strcmp(const char *lhs, const char *rhs) {
    while (*lhs && *lhs == *rhs) {
        ++lhs;
        ++rhs;
    }
    return (unsigned char)*lhs - (unsigned char)*rhs;
}

int strncmp(const char *lhs, const char *rhs, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const unsigned char a = (unsigned char)lhs[i];
        const unsigned char b = (unsigned char)rhs[i];
        if (a != b || !a || !b) return (int)a - (int)b;
    }
    return 0;
}

size_t strnlen(const char *text, size_t max_count) {
    size_t length = 0;
    while (length < max_count && text && text[length]) ++length;
    return length;
}

void *memchr(const void *ptr, int value, size_t count) {
    const unsigned char *bytes = (const unsigned char *)ptr;
    for (size_t i = 0; i < count; ++i) {
        if (bytes[i] == (unsigned char)value) return (void *)(bytes + i);
    }
    return 0;
}

char *strchr(const char *text, int value) {
    const char needle = (char)value;
    while (*text) {
        if (*text == needle) return (char *)text;
        ++text;
    }
    return needle == '\0' ? (char *)text : 0;
}

char *strrchr(const char *text, int value) {
    const char needle = (char)value;
    const char *last = 0;
    do {
        if (*text == needle) last = text;
    } while (*text++);
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; ++haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            ++h;
            ++n;
        }
        if (!*n) return (char *)haystack;
    }
    return 0;
}

char *strcat(char *dest, const char *src) {
    char *out = dest + strlen(dest);
    while ((*out++ = *src++) != '\0') {}
    return dest;
}

size_t strlcpy(char *dest, const char *src, size_t size) {
    const size_t src_len = strlen(src);
    if (size) {
        const size_t copy = src_len < size - 1u ? src_len : size - 1u;
        memcpy(dest, src, copy);
        dest[copy] = '\0';
    }
    return src_len;
}

static int ascii_lower(int ch) {
    return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch;
}

int strcasecmp(const char *lhs, const char *rhs) {
    while (*lhs && ascii_lower((unsigned char)*lhs) == ascii_lower((unsigned char)*rhs)) {
        ++lhs;
        ++rhs;
    }
    return ascii_lower((unsigned char)*lhs) - ascii_lower((unsigned char)*rhs);
}

int strncasecmp(const char *lhs, const char *rhs, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const int a = ascii_lower((unsigned char)lhs[i]);
        const int b = ascii_lower((unsigned char)rhs[i]);
        if (a != b || !a || !b) return a - b;
    }
    return 0;
}
