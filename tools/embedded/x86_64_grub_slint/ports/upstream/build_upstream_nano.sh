#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "usage: $0 <nano-source-dir> <build-dir>" >&2
    exit 2
fi

source_arg="$1"
build_dir="$2"
# Self-fetch the upstream GNU nano source if absent, so a clean checkout / fresh
# CI runner can build without a pre-staged tree (mirrors the ncurses download).
# Override with RADIX_NANO_VERSION / RADIX_NANO_URL.
if [[ ! -f "${source_arg}/configure" ]]; then
    nano_version="${RADIX_NANO_VERSION:-9.0}"
    nano_parent="$(dirname "${source_arg}")"
    nano_archive="${nano_parent}/nano-${nano_version}.tar.xz"
    nano_url="${RADIX_NANO_URL:-https://www.nano-editor.org/dist/v${nano_version%%.*}/nano-${nano_version}.tar.xz}"
    mkdir -p "${nano_parent}"
    [[ -f "${nano_archive}" ]] || curl -L --fail --show-error -o "${nano_archive}" "${nano_url}"
    rm -rf "${source_arg}"
    tar -xf "${nano_archive}" -C "${nano_parent}"
    if [[ ! -f "${source_arg}/configure" && -d "${nano_parent}/nano-${nano_version}" \
          && "${source_arg}" != "${nano_parent}/nano-${nano_version}" ]]; then
        mv "${nano_parent}/nano-${nano_version}" "${source_arg}"
    fi
fi
source_dir="$(cd "${source_arg}" && pwd)"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
target_dir="$(cd "${script_dir}/../.." && pwd)"
sysroot="${RADIX_SYSROOT_DIR:-${target_dir}/sysroot}"
ncurses_include="${RADIX_NCURSES_INCLUDE_DIR:-${sysroot}/include}"
lib_dir="${RADIX_USERSPACE_LIB_DIR:-${build_dir}/radix-libs}"
cc="${RADIX_CC:-x86_64-linux-gnu-gcc}"
objcopy="${RADIX_OBJCOPY:-x86_64-linux-gnu-objcopy}"

mkdir -p "${build_dir}" "${lib_dir}"

if [[ ! -f "${source_dir}/configure" ]]; then
    echo "nano source tree does not contain configure: ${source_dir}" >&2
    exit 3
fi

cat >"${build_dir}/radix-cc" <<EOF
#!/usr/bin/env bash
compile_only=0
preprocess_only=0
for arg in "\$@"; do
    case "\${arg}" in
        -c|-S|-E) compile_only=1 ;;
    esac
    case "\${arg}" in
        -E) preprocess_only=1 ;;
    esac
done
if [[ "\${compile_only}" == "1" ]]; then
    if [[ "\${preprocess_only}" == "1" ]]; then
        exec "${cc}" -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -nostdinc -U__linux__ -Ulinux -U__gnu_linux__ -I"${ncurses_include}" -I"${ncurses_include}/ncursesw" -I"${sysroot}/include" "\$@"
    fi
    exec "${cc}" -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -nostdinc -U__linux__ -Ulinux -U__gnu_linux__ -I"${ncurses_include}" -I"${ncurses_include}/ncursesw" -I"${sysroot}/include" -include "${build_dir}/radix_gnulib_overrides.h" "\$@"
fi
exec "${cc}" -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -nostdinc -U__linux__ -Ulinux -U__gnu_linux__ -I"${ncurses_include}" -I"${ncurses_include}/ncursesw" -I"${sysroot}/include" -nostdlib -static -no-pie -Wl,-Ttext=0x40000000 -Wl,--build-id=none "${build_dir}/radix_crt0.o" "\$@" -L"${lib_dir}" -lncursesw -ltinfo -lradixc
EOF
chmod +x "${build_dir}/radix-cc"

cat >"${build_dir}/radix_crt0.S" <<'EOF'
.section .text
.code64
.global _start
.type _start, @function
_start:
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    lea 16(%rsp,%rdi,8), %rdx
    call main
    mov %rax, %rdi
    mov $10, %rax
    int $0x80
1:
    jmp 1b
EOF

"${cc}" -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -c "${build_dir}/radix_crt0.S" -o "${build_dir}/radix_crt0.o"

cat >"${build_dir}/radix_gnulib_overrides.h" <<'EOF'
#ifndef RADIX_GNULIB_OVERRIDES_H
#define RADIX_GNULIB_OVERRIDES_H
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <wchar.h>
#include <sys/wait.h>
#ifndef static_assert
#define static_assert(cond) _Static_assert(cond, #cond)
#endif
int strcmp(const char *a, const char *b);
#ifndef streq
#define streq(a, b) (strcmp((a), (b)) == 0)
#endif
#ifndef SETLOCALE_NULL_MAX
#define SETLOCALE_NULL_MAX 257
#endif
int setlocale_null_r(int category, char *buf, size_t bufsize);
int _gl_register_fd(int fd, const char *filename);
void _gl_unregister_fd(int fd);
int _gl_register_dup(int oldfd, int newfd);
const char *_gl_directory_name(int fd);
bool memeq(const void *a, const void *b, size_t n);
ptrdiff_t vsnzprintf(char *restrict str, size_t size, const char *restrict format, va_list args);
double frexp(double x, int *expptr);
long double frexpl(long double x, int *expptr);
int raise(int signum);
void gl_unreachable(void) __attribute__((noreturn));
#ifndef _GL_ATTRIBUTE_FORMAT
#define _GL_ATTRIBUTE_FORMAT(spec)
#endif
#ifndef _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD
#define _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(a, b)
#endif
#ifndef _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM
#define _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM(a, b)
#endif
#ifndef _GL_ARG_NONNULL
#define _GL_ARG_NONNULL(a)
#endif
#ifndef GNULIB_defined_F_DUPFD_CLOEXEC
#define GNULIB_defined_F_DUPFD_CLOEXEC 0
#endif
#endif
EOF

cp "${RADIX_LIBC_ARCHIVE:-${lib_dir}/libradixc.a}" "${lib_dir}/libradixc.a" 2>/dev/null || true
cp "${RADIX_NCURSESW_ARCHIVE:-${lib_dir}/libncursesw.a}" "${lib_dir}/libncursesw.a" 2>/dev/null || true
cp "${RADIX_TINFO_ARCHIVE:-${lib_dir}/libtinfo.a}" "${lib_dir}/libtinfo.a" 2>/dev/null || true

if [[ ! -f "${lib_dir}/libradixc.a" || ! -f "${lib_dir}/libncursesw.a" || ! -f "${lib_dir}/libtinfo.a" ]]; then
    echo "missing RADix userspace libraries in ${lib_dir}; build radix_userspace_libs first" >&2
    exit 4
fi

"${objcopy}" --weaken "${lib_dir}/libradixc.a"

cd "${build_dir}"
CONFIG_SITE="${script_dir}/radix-config.site" \
CC="${build_dir}/radix-cc" \
CFLAGS="-Os -D__radix__=1" \
CPPFLAGS="-I${ncurses_include} -I${ncurses_include}/ncursesw -I${sysroot}/include" \
LDFLAGS="" \
PKG_CONFIG=false \
NCURSESW_CFLAGS="-I${ncurses_include} -I${ncurses_include}/ncursesw" \
NCURSESW_LIBS="-lncursesw -ltinfo" \
NCURSES_CFLAGS="-I${ncurses_include} -I${ncurses_include}/ncursesw" \
NCURSES_LIBS="-lncursesw -ltinfo" \
"${source_dir}/configure" \
    --host=x86_64-none \
    --build=x86_64-pc-linux-gnu \
    --prefix=/usr \
    --enable-utf8 \
    --disable-nls

make V=1
