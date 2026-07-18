#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <new>

// Portable early-console + halt hooks (implemented by each platform HAL; weak so
// this freestanding runtime links on any target).
extern "C" void rad_hal_early_console_write(const char *text) __attribute__((weak));
extern "C" void rad_hal_cpu_halt_forever(void) __attribute__((weak));

// Portable CPU relax for spin-wait loops.
static inline void cpu_relax(void) {
#if defined(__x86_64__) || defined(__i386__)
    asm volatile("pause");
#elif defined(__aarch64__)
    asm volatile("yield");
#else
    asm volatile("" ::: "memory");
#endif
}

namespace {
struct alignas(64) HeapBlock {
    size_t size;
    HeapBlock *next;
    uint32_t free;
    uint8_t reserved[64u - sizeof(size_t) - sizeof(HeapBlock*) - sizeof(uint32_t)];
};

static_assert(sizeof(HeapBlock) == 64u);

struct AllocationHeader {
    HeapBlock *block;
    uint32_t magic;
    uint32_t reserved;
};

constexpr uint32_t AllocationMagic = 0x52414441u;

// Kernel heap size. x86 (which has no separate static page pool) defaults to a
// large heap; targets with their own big static allocations (e.g. the a53 page
// pool g_host_pages, 256 MiB) override this to a modest value via the build.
#ifndef RADIX_KERNEL_HEAP_BYTES
#define RADIX_KERNEL_HEAP_BYTES (64u * 1024u * 1024u)
#endif
alignas(64) uint8_t g_heap[RADIX_KERNEL_HEAP_BYTES];
HeapBlock *g_heap_head = nullptr;
volatile int g_heap_lock = 0;
int g_cpp_ctor_ran = 0;

struct CppRuntimeProbe {
    CppRuntimeProbe() { g_cpp_ctor_ran = 1; }
};

CppRuntimeProbe g_cpp_runtime_probe;

void heap_lock(void) {
    while (__atomic_test_and_set(&g_heap_lock, __ATOMIC_ACQUIRE)) cpu_relax();
}

void heap_unlock(void) {
    __atomic_clear(&g_heap_lock, __ATOMIC_RELEASE);
}

size_t align_up_size(size_t value, size_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

uintptr_t align_up_uintptr(uintptr_t value, size_t alignment) {
    return (value + alignment - 1u) & ~(static_cast<uintptr_t>(alignment) - 1u);
}

void heap_init(void) {
    if (g_heap_head) return;
    g_heap_head = reinterpret_cast<HeapBlock*>(g_heap);
    g_heap_head->size = sizeof(g_heap) - sizeof(HeapBlock);
    g_heap_head->next = nullptr;
    g_heap_head->free = 1;
}

void split_block(HeapBlock *block, size_t size) {
    if (!block || block->size < size + sizeof(HeapBlock) + 64u) return;
    auto *next = reinterpret_cast<HeapBlock*>(reinterpret_cast<uint8_t*>(block + 1) + size);
    next->size = block->size - size - sizeof(HeapBlock);
    next->next = block->next;
    next->free = 1;
    block->size = size;
    block->next = next;
}

void coalesce_heap(void) {
    for (HeapBlock *block = g_heap_head; block && block->next;) {
        if (block->free && block->next->free) {
            block->size += sizeof(HeapBlock) + block->next->size;
            block->next = block->next->next;
        } else {
            block = block->next;
        }
    }
}

void *allocate_aligned(size_t size, size_t alignment) {
    if (size == 0) size = 1;
    if (alignment < 16u) alignment = 16u;
    if ((alignment & (alignment - 1u)) != 0 || alignment > 4096u) return nullptr;
    const size_t total = align_up_size(size + alignment - 1u + sizeof(AllocationHeader), 64u);
    heap_lock();
    heap_init();
    for (HeapBlock *block = g_heap_head; block; block = block->next) {
        if (!block->free || block->size < total) continue;
        split_block(block, total);
        block->free = 0;
        const uintptr_t raw = reinterpret_cast<uintptr_t>(block + 1);
        const uintptr_t user = align_up_uintptr(raw + sizeof(AllocationHeader), alignment);
        auto *header = reinterpret_cast<AllocationHeader*>(user - sizeof(AllocationHeader));
        header->block = block;
        header->magic = AllocationMagic;
        header->reserved = 0;
        heap_unlock();
        return reinterpret_cast<void*>(user);
    }
    heap_unlock();
    return nullptr;
}

void put_char(char *buffer, size_t size, size_t& pos, char ch) {
    if (size && pos + 1 < size) buffer[pos] = ch;
    ++pos;
}

void put_text(char *buffer, size_t size, size_t& pos, const char *text) {
    if (!text) text = "(null)";
    while (*text) put_char(buffer, size, pos, *text++);
}

void put_unsigned(char *buffer, size_t size, size_t& pos, unsigned long long value, unsigned base, bool upper = false) {
    char tmp[32];
    size_t n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        tmp[n++] = digits[value % base];
        value /= base;
    } while (value && n < sizeof(tmp));
    while (n) put_char(buffer, size, pos, tmp[--n]);
}
}

extern "C" {

#if defined(__x86_64__) || defined(__i386__)
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();
#endif

void *memcpy(void *dst, const void *src, size_t n) {
    auto *d = static_cast<unsigned char*>(dst);
    const auto *s = static_cast<const unsigned char*>(src);
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    auto *d = static_cast<unsigned char*>(dst);
    const auto *s = static_cast<const unsigned char*>(src);
    if (d < s) {
        for (size_t i = 0; i < n; ++i) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; --i) d[i - 1] = s[i - 1];
    }
    return dst;
}

void *memset(void *dst, int value, size_t n) {
    auto *d = static_cast<unsigned char*>(dst);
    for (size_t i = 0; i < n; ++i) d[i] = static_cast<unsigned char>(value);
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const auto *aa = static_cast<const unsigned char*>(a);
    const auto *bb = static_cast<const unsigned char*>(b);
    for (size_t i = 0; i < n; ++i) {
        if (aa[i] != bb[i]) return aa[i] < bb[i] ? -1 : 1;
    }
    return 0;
}

int bcmp(const void *a, const void *b, size_t n) {
    return memcmp(a, b, n);
}

void *memchr(const void *s, int c, size_t n) {
    const auto *p = static_cast<const unsigned char*>(s);
    for (size_t i = 0; i < n; ++i) {
        if (p[i] == static_cast<unsigned char>(c)) return const_cast<unsigned char*>(p + i);
    }
    return nullptr;
}

size_t strlen(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

size_t strnlen(const char *s, size_t max) {
    size_t n = 0;
    if (!s) return 0;
    while (n < max && s[n]) ++n;
    return n;
}

char *strcpy(char *dst, const char *src) {
    char *out = dst;
    while ((*out++ = *src++) != '\0') {}
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i] || !a[i] || !b[i]) {
            return static_cast<unsigned char>(a[i]) - static_cast<unsigned char>(b[i]);
        }
    }
    return 0;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src && src[i]; ++i) dst[i] = src[i];
    for (; i < n; ++i) dst[i] = '\0';
    return dst;
}

char *strchr(const char *s, int c) {
    while (s && *s) {
        if (*s == static_cast<char>(c)) return const_cast<char*>(s);
        ++s;
    }
    return c == 0 && s ? const_cast<char*>(s) : nullptr;
}

char *strrchr(const char *s, int c) {
    const char *last = nullptr;
    while (s && *s) {
        if (*s == static_cast<char>(c)) last = s;
        ++s;
    }
    if (c == 0 && s) return const_cast<char*>(s);
    return const_cast<char*>(last);
}

char *strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return nullptr;
    if (!*needle) return const_cast<char*>(haystack);
    for (const char *h = haystack; *h; ++h) {
        const char *a = h;
        const char *b = needle;
        while (*a && *b && *a == *b) {
            ++a;
            ++b;
        }
        if (!*b) return const_cast<char*>(h);
    }
    return nullptr;
}

int isprint(int ch) {
    return ch >= 0x20 && ch <= 0x7e;
}

int snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    size_t pos = 0;
    for (const char *p = format; p && *p; ++p) {
        if (*p != '%') {
            put_char(buffer, size, pos, *p);
            continue;
        }
        ++p;
        bool long_long = false;
        bool long_one = false;
        bool size_t_arg = false;
        if (*p == 'z') {
            size_t_arg = true;
            ++p;
        } else if (*p == 'l') {
            long_one = true;
            ++p;
            if (*p == 'l') {
                long_long = true;
                ++p;
            }
        }
        switch (*p) {
        case '%': put_char(buffer, size, pos, '%'); break;
        case 'c': put_char(buffer, size, pos, static_cast<char>(va_arg(args, int))); break;
        case 's': put_text(buffer, size, pos, va_arg(args, const char*)); break;
        case 'd':
        case 'i': {
            long long value = long_long ? va_arg(args, long long)
                : long_one ? va_arg(args, long)
                : va_arg(args, int);
            if (value < 0) {
                put_char(buffer, size, pos, '-');
                value = -value;
            }
            put_unsigned(buffer, size, pos, static_cast<unsigned long long>(value), 10);
            break;
        }
        case 'u': {
            unsigned long long value = size_t_arg ? va_arg(args, size_t)
                : long_long ? va_arg(args, unsigned long long)
                : long_one ? va_arg(args, unsigned long)
                : va_arg(args, unsigned int);
            put_unsigned(buffer, size, pos, value, 10);
            break;
        }
        case 'x':
        case 'X': {
            unsigned long long value = size_t_arg ? va_arg(args, size_t)
                : long_long ? va_arg(args, unsigned long long)
                : long_one ? va_arg(args, unsigned long)
                : va_arg(args, unsigned int);
            put_unsigned(buffer, size, pos, value, 16, *p == 'X');
            break;
        }
        case 'p': {
            auto value = reinterpret_cast<uintptr_t>(va_arg(args, void*));
            put_text(buffer, size, pos, "0x");
            put_unsigned(buffer, size, pos, value, 16);
            break;
        }
        default:
            put_char(buffer, size, pos, '%');
            put_char(buffer, size, pos, *p);
            break;
        }
    }
    if (size) {
        buffer[pos < size ? pos : size - 1] = '\0';
    }
    va_end(args);
    return static_cast<int>(pos);
}

void *malloc(size_t size) {
    return allocate_aligned(size, 16u);
}

void free(void *pointer) {
    if (!pointer) return;
    auto *header = reinterpret_cast<AllocationHeader*>(static_cast<uint8_t*>(pointer) - sizeof(AllocationHeader));
    if (header->magic != AllocationMagic) return;
    auto *block = header->block;
    const uintptr_t block_addr = reinterpret_cast<uintptr_t>(block);
    const uintptr_t heap_begin = reinterpret_cast<uintptr_t>(g_heap);
    const uintptr_t heap_end = heap_begin + sizeof(g_heap);
    if (block_addr < heap_begin || block_addr >= heap_end) return;
    heap_lock();
    if (block->free) {
        heap_unlock();
        return;
    }
    header->magic = 0;
    block->free = 1;
    coalesce_heap();
    heap_unlock();
}

void abort(void) {
    if (rad_hal_early_console_write) rad_hal_early_console_write("abort\n");
    if (rad_hal_cpu_halt_forever) rad_hal_cpu_halt_forever();
    for (;;) cpu_relax();
}

void rust_eh_personality(void) {}
void _Unwind_Resume(void*) { abort(); }
void __assert_fail(const char*, const char*, unsigned int, const char*) { abort(); }

int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void *__dso_handle = nullptr;
void __cxa_pure_virtual() { abort(); }
int __cxa_guard_acquire(uint64_t *guard) { return !(*guard); }
void __cxa_guard_release(uint64_t *guard) { *guard = 1; }
void __cxa_guard_abort(uint64_t*) {}

#if defined(__x86_64__) || defined(__i386__)
// C++ global-constructor running is x86-only for now: the x86 GRUB image relies on
// it and its linker script defines __init_array_start/end. Other targets run any
// needed init explicitly and do not link this.
void x86_run_global_constructors(void) {
    for (auto ctor = __init_array_start; ctor != __init_array_end; ++ctor) {
        if (*ctor) (*ctor)();
    }
}

int x86_cpp_runtime_self_test(void) {
    void *aligned = allocate_aligned(64u, 64u);
    return g_cpp_ctor_ran && aligned && (reinterpret_cast<uintptr_t>(aligned) & 63u) == 0u;
}
#endif

}

void *operator new(size_t size) { return malloc(size); }
void *operator new[](size_t size) { return malloc(size); }
void *operator new(size_t size, const std::nothrow_t&) noexcept { return malloc(size); }
void *operator new[](size_t size, const std::nothrow_t&) noexcept { return malloc(size); }
void *operator new(size_t size, std::align_val_t alignment) { return allocate_aligned(size, static_cast<size_t>(alignment)); }
void *operator new[](size_t size, std::align_val_t alignment) { return allocate_aligned(size, static_cast<size_t>(alignment)); }
void *operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return allocate_aligned(size, static_cast<size_t>(alignment)); }
void *operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return allocate_aligned(size, static_cast<size_t>(alignment)); }
void operator delete(void *ptr) noexcept { free(ptr); }
void operator delete[](void *ptr) noexcept { free(ptr); }
void operator delete(void *ptr, size_t) noexcept { free(ptr); }
void operator delete[](void *ptr, size_t) noexcept { free(ptr); }
void operator delete(void *ptr, const std::nothrow_t&) noexcept { free(ptr); }
void operator delete[](void *ptr, const std::nothrow_t&) noexcept { free(ptr); }
void operator delete(void *ptr, std::align_val_t) noexcept { free(ptr); }
void operator delete[](void *ptr, std::align_val_t) noexcept { free(ptr); }
void operator delete(void *ptr, size_t, std::align_val_t) noexcept { free(ptr); }
void operator delete[](void *ptr, size_t, std::align_val_t) noexcept { free(ptr); }
void operator delete(void *ptr, std::align_val_t, const std::nothrow_t&) noexcept { free(ptr); }
void operator delete[](void *ptr, std::align_val_t, const std::nothrow_t&) noexcept { free(ptr); }
