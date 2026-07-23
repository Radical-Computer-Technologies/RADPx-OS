/* Userland C++ runtime support for linking freestanding Slint into a userland
 * process. Mirrors the kernel's RADKernel/runtime/freestanding_runtime.cpp but
 * backs allocation with libradc's malloc/free instead of the kernel allocator.
 * Built -fno-exceptions -fno-rtti, so the throw handlers report and abort. */
#include <cstddef>
#include <cstdint>
#include <new>
#include <typeinfo>
#include <memory>

extern "C" void *malloc(size_t);
extern "C" void free(void *);

// Direct syscall entry (int 0x80). Provided here so the freestanding client
// needs no libc syscall glue beyond libradc.
extern "C" long rad_syscall6(long n, long a, long b, long c, long d, long e, long f) {
    long ret;
    register long r10 asm("r10") = d;
    register long r8 asm("r8") = e;
    register long r9 asm("r9") = f;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
                 : "memory");
    return ret;
}

[[noreturn]] static void support_abort(const char *msg) {
    if (msg) {
        size_t n = 0;
        while (msg[n]) ++n;
        rad_syscall6(1 /*write*/, 2 /*stderr*/, (long)msg, (long)n, 0, 0, 0);
    }
    rad_syscall6(10 /*exit*/, 127, 0, 0, 0, 0, 0);
    for (;;) {}
}

// Slint (Rust, panic=abort) references the unwind personality + resume; they are
// never actually reached, so route to abort. bcmp is memcmp under another name.
extern "C" int rust_eh_personality(void) { support_abort("rust_eh_personality\n"); }
extern "C" void _Unwind_Resume(void *) { support_abort("unwind_resume\n"); }
extern "C" int bcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = static_cast<const unsigned char *>(a);
    const unsigned char *pb = static_cast<const unsigned char *>(b);
    for (size_t i = 0; i < n; ++i) {
        if (pa[i] != pb[i]) return pa[i] < pb[i] ? -1 : 1;
    }
    return 0;
}

extern "C" void __cxa_pure_virtual(void) { support_abort("pure virtual call\n"); }
extern "C" int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
extern "C" void *__dso_handle = nullptr;

// Aligned allocation: over-allocate and store the base pointer just before the
// returned block (Slint's software renderer requests over-aligned buffers).
static void *aligned_alloc_impl(size_t size, size_t alignment) {
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    const size_t total = size + alignment + sizeof(void *);
    void *base = malloc(total ? total : 1);
    if (!base) return nullptr;
    uintptr_t raw = reinterpret_cast<uintptr_t>(base) + sizeof(void *);
    uintptr_t aligned = (raw + (alignment - 1)) & ~(uintptr_t)(alignment - 1);
    reinterpret_cast<void **>(aligned)[-1] = base;
    return reinterpret_cast<void *>(aligned);
}
static void aligned_free_impl(void *ptr) {
    if (!ptr) return;
    free(reinterpret_cast<void **>(ptr)[-1]);
}

void *operator new(size_t size) { void *p = malloc(size ? size : 1); if (!p) support_abort("oom\n"); return p; }
void *operator new[](size_t size) { return operator new(size); }
void *operator new(size_t size, const std::nothrow_t &) noexcept { return malloc(size ? size : 1); }
void *operator new[](size_t size, const std::nothrow_t &) noexcept { return malloc(size ? size : 1); }
void *operator new(size_t size, std::align_val_t a) { void *p = aligned_alloc_impl(size, (size_t)a); if (!p) support_abort("oom\n"); return p; }
void *operator new[](size_t size, std::align_val_t a) { return operator new(size, a); }
void *operator new(size_t size, std::align_val_t a, const std::nothrow_t &) noexcept { return aligned_alloc_impl(size, (size_t)a); }
void *operator new[](size_t size, std::align_val_t a, const std::nothrow_t &) noexcept { return aligned_alloc_impl(size, (size_t)a); }

void operator delete(void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { free(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { free(p); }
void operator delete(void *p, std::align_val_t) noexcept { aligned_free_impl(p); }
void operator delete[](void *p, std::align_val_t) noexcept { aligned_free_impl(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept { aligned_free_impl(p); }
void operator delete[](void *p, size_t, std::align_val_t) noexcept { aligned_free_impl(p); }

// libstdc++ container error handlers (fatal here -- built without exceptions).
namespace std {
void __throw_bad_alloc() { support_abort("bad_alloc\n"); }
void __throw_length_error(const char *) { support_abort("length_error\n"); }
void __throw_bad_array_new_length() { support_abort("bad_array_new_length\n"); }
void __throw_out_of_range(const char *) { support_abort("out_of_range\n"); }
void __throw_out_of_range_fmt(const char *, ...) { support_abort("out_of_range\n"); }
void __throw_logic_error(const char *) { support_abort("logic_error\n"); }
void __throw_bad_function_call() { support_abort("bad_function_call\n"); }
bool _Sp_make_shared_tag::_S_eq(const type_info &) noexcept { return false; }
}

extern "C" { char __libc_single_threaded = 1; }
