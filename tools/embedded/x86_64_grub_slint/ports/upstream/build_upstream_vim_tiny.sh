#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "usage: $0 <vim-source-dir> <build-dir>" >&2
    exit 2
fi

source_dir="$(cd "$1" && pwd)"
build_dir="$2"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
target_dir="$(cd "${script_dir}/../.." && pwd)"
sysroot="${RADIX_SYSROOT_DIR:-${target_dir}/sysroot}"
ncurses_include="${RADIX_NCURSES_INCLUDE_DIR:-${sysroot}/include}"
lib_dir="${RADIX_USERSPACE_LIB_DIR:-${build_dir}/radix-libs}"
cc="${RADIX_CC:-x86_64-linux-gnu-gcc}"

if [[ -f "${source_dir}/src/configure" ]]; then
    configure_rel="src/configure"
elif [[ -f "${source_dir}/configure" ]]; then
    configure_rel="configure"
else
    echo "vim source tree does not contain configure: ${source_dir}" >&2
    exit 3
fi

rm -rf "${build_dir}"
mkdir -p "$(dirname "${build_dir}")" "${build_dir}" "${lib_dir}"
cp -a "${source_dir}/." "${build_dir}/"

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
    exec "${cc}" -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -nostdinc -U__linux__ -Ulinux -U__gnu_linux__ -I"${ncurses_include}" -I"${ncurses_include}/ncursesw" -I"${sysroot}/include" "\$@"
fi
filtered=()
for arg in "\$@"; do
    case "\${arg}" in
        -lm) continue ;;
    esac
    filtered+=("\${arg}")
done
exec "${cc}" -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -nostdinc -U__linux__ -Ulinux -U__gnu_linux__ -I"${ncurses_include}" -I"${ncurses_include}/ncursesw" -I"${sysroot}/include" -nostdlib -static -no-pie -Wl,-Ttext=0x40000000 -Wl,--build-id=none "${build_dir}/radix_crt0.o" -L"${lib_dir}" "\${filtered[@]}" -lncursesw -ltinfo -lradixc
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

cp "${RADIX_LIBC_ARCHIVE:-${lib_dir}/libradixc.a}" "${lib_dir}/libradixc.a" 2>/dev/null || true
cp "${RADIX_NCURSESW_ARCHIVE:-${lib_dir}/libncursesw.a}" "${lib_dir}/libncursesw.a" 2>/dev/null || true
cp "${RADIX_TINFO_ARCHIVE:-${lib_dir}/libtinfo.a}" "${lib_dir}/libtinfo.a" 2>/dev/null || true

if [[ ! -f "${lib_dir}/libradixc.a" || ! -f "${lib_dir}/libncursesw.a" || ! -f "${lib_dir}/libtinfo.a" ]]; then
    echo "missing RADix userspace libraries in ${lib_dir}; build radix_userspace_libs first" >&2
    exit 4
fi

cd "${build_dir}/$(dirname "${configure_rel}")"
CONFIG_SITE="${RADIX_VIM_CONFIG_SITE:-${script_dir}/radix-config.site}" \
CC="${build_dir}/radix-cc" \
CFLAGS="-Os -D__radix__=1" \
CPPFLAGS="-I${ncurses_include} -I${ncurses_include}/ncursesw -I${sysroot}/include" \
LDFLAGS="" \
PKG_CONFIG=false \
./"$(basename "${configure_rel}")" \
    --host=x86_64-none \
    --build=x86_64-pc-linux-gnu \
    --prefix=/usr \
    --with-features=tiny \
    --disable-gui \
    --without-x \
    --disable-nls \
    --disable-acl \
    --disable-gpm \
    --disable-selinux \
    --disable-netbeans \
    --disable-channel \
    --disable-terminal \
    --disable-cscope \
    --with-tlib=ncursesw

make -j"${RADIX_BUILD_JOBS:-2}" vim
test -f "${build_dir}/src/vim" -o -f "${build_dir}/vim"
