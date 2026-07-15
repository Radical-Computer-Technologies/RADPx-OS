cmake_minimum_required(VERSION 3.21)

project(radixkernel_x86_64_grub C CXX ASM)

find_program(GRUB_MKRESCUE_EXECUTABLE grub-mkrescue)
find_program(MFORMAT_EXECUTABLE mformat)
find_program(X86_64_LINUX_GNU_GCC_EXECUTABLE x86_64-linux-gnu-gcc)
find_program(MKFS_EXT2_EXECUTABLE mkfs.ext2)
find_program(MKFS_EXT4_EXECUTABLE mkfs.ext4)
find_program(MKFS_FAT_EXECUTABLE mkfs.fat)
find_program(DEBUGFS_EXECUTABLE debugfs)
find_program(PYTHON3_EXECUTABLE python3)
find_program(TIC_EXECUTABLE tic)
if(NOT GRUB_MKRESCUE_EXECUTABLE OR NOT MFORMAT_EXECUTABLE)
    message(FATAL_ERROR "ISO packaging requires grub-mkrescue and mformat from mtools")
endif()
if(NOT X86_64_LINUX_GNU_GCC_EXECUTABLE OR NOT MKFS_EXT2_EXECUTABLE OR NOT MKFS_EXT4_EXECUTABLE OR NOT MKFS_FAT_EXECUTABLE OR NOT DEBUGFS_EXECUTABLE OR NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "RADix rootfs packaging requires x86_64-linux-gnu-gcc, mkfs.ext2, mkfs.ext4, mkfs.fat, debugfs, and python3")
endif()
if(NOT TIC_EXECUTABLE)
    message(FATAL_ERROR "RADix ncurses compatibility requires tic from ncurses-bin")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RADIX_X86_UI_PROFILE "wm" CACHE STRING "RADix x86_64 UI profile: wm or terminal")
set_property(CACHE RADIX_X86_UI_PROFILE PROPERTY STRINGS wm terminal)
if(NOT RADIX_X86_UI_PROFILE STREQUAL "wm" AND NOT RADIX_X86_UI_PROFILE STREQUAL "terminal")
    message(FATAL_ERROR "RADIX_X86_UI_PROFILE must be wm or terminal")
endif()
set(RADIX_X86_KERNEL_NAME "radixkernel-x86-64-grub-${RADIX_X86_UI_PROFILE}")
set(RADIX_RKCONFIG_DIR "${CMAKE_BINARY_DIR}/rkconfig")
set(RADIX_RKCONFIG_HOSTNAME "$ENV{RADIX_RKCONFIG_HOSTNAME}" CACHE STRING "RADix hostname")
if(NOT RADIX_RKCONFIG_HOSTNAME)
    set(RADIX_RKCONFIG_HOSTNAME "radix")
endif()
set(RADIX_RKCONFIG_ROOT_PASSWORD "$ENV{RADIX_RKCONFIG_ROOT_PASSWORD}" CACHE STRING "RADix root dev password")
if(NOT RADIX_RKCONFIG_ROOT_PASSWORD)
    set(RADIX_RKCONFIG_ROOT_PASSWORD "radix")
endif()
set(RADIX_RKCONFIG_ROOTFS_SIZE_MB "$ENV{RADIX_RKCONFIG_ROOTFS_SIZE_MB}" CACHE STRING "RADix ext4 rootfs size in MiB")
if(NOT RADIX_RKCONFIG_ROOTFS_SIZE_MB)
    set(RADIX_RKCONFIG_ROOTFS_SIZE_MB "256")
endif()
set(RADIX_RKCONFIG_TERMINAL_SCALE "$ENV{RADIX_RKCONFIG_TERMINAL_SCALE}" CACHE STRING "RADix terminal scale")
if(NOT RADIX_RKCONFIG_TERMINAL_SCALE)
    set(RADIX_RKCONFIG_TERMINAL_SCALE "auto")
endif()
set(RADIX_RKCONFIG_TERMINAL_FONT "$ENV{RADIX_RKCONFIG_TERMINAL_FONT}" CACHE STRING "RADix terminal font")
if(NOT RADIX_RKCONFIG_TERMINAL_FONT)
    set(RADIX_RKCONFIG_TERMINAL_FONT "radix-default")
endif()
set(RADIX_RKCONFIG_TERMINAL_THEME "$ENV{RADIX_RKCONFIG_TERMINAL_THEME}" CACHE STRING "RADix terminal theme")
if(NOT RADIX_RKCONFIG_TERMINAL_THEME)
    set(RADIX_RKCONFIG_TERMINAL_THEME "radix-dark")
endif()
set(RADIX_RKCONFIG_TERMINAL_AUTOCOMPLETE "$ENV{RADIX_RKCONFIG_TERMINAL_AUTOCOMPLETE}" CACHE STRING "RADix terminal autocomplete")
if(NOT RADIX_RKCONFIG_TERMINAL_AUTOCOMPLETE)
    set(RADIX_RKCONFIG_TERMINAL_AUTOCOMPLETE "true")
endif()
set(RADIX_RKCONFIG_TERMINAL_POSIX_COMPAT "$ENV{RADIX_RKCONFIG_TERMINAL_POSIX_COMPAT}" CACHE STRING "RADix terminal POSIX compatibility")
if(NOT RADIX_RKCONFIG_TERMINAL_POSIX_COMPAT)
    set(RADIX_RKCONFIG_TERMINAL_POSIX_COMPAT "true")
endif()
set(RADIX_RKCONFIG_TERMINAL_NCURSES "$ENV{RADIX_RKCONFIG_TERMINAL_NCURSES}" CACHE STRING "RADix terminal ncurses compatibility assets")
if(NOT RADIX_RKCONFIG_TERMINAL_NCURSES)
    set(RADIX_RKCONFIG_TERMINAL_NCURSES "true")
endif()
set(RADIX_RKCONFIG_TERMINAL_NANO "$ENV{RADIX_RKCONFIG_TERMINAL_NANO}" CACHE STRING "RADix terminal nano package")
if(NOT RADIX_RKCONFIG_TERMINAL_NANO)
    set(RADIX_RKCONFIG_TERMINAL_NANO "false")
endif()
set(RADIX_RKCONFIG_TERMINAL_NANO_VARIANT "$ENV{RADIX_RKCONFIG_TERMINAL_NANO_VARIANT}" CACHE STRING "RADix terminal nano variant: none, tiny, full, both")
if(NOT RADIX_RKCONFIG_TERMINAL_NANO_VARIANT)
    set(RADIX_RKCONFIG_TERMINAL_NANO_VARIANT "none")
endif()
set(RADIX_RKCONFIG_TERMINAL_NANO_ENABLED FALSE)
if((RADIX_RKCONFIG_TERMINAL_NANO STREQUAL "true" OR RADIX_RKCONFIG_TERMINAL_NANO STREQUAL "1" OR RADIX_RKCONFIG_TERMINAL_NANO STREQUAL "ON")
   AND NOT RADIX_RKCONFIG_TERMINAL_NANO_VARIANT STREQUAL "none")
    set(RADIX_RKCONFIG_TERMINAL_NANO_ENABLED TRUE)
endif()
set(RADIX_X86_TERMINAL_AUTOTEST_NANO "$ENV{RADIX_X86_TERMINAL_AUTOTEST_NANO}" CACHE STRING "Run deterministic PTY nano smoke on terminal boot")
if(NOT RADIX_X86_TERMINAL_AUTOTEST_NANO)
    set(RADIX_X86_TERMINAL_AUTOTEST_NANO "false")
endif()
set(RADIX_X86_TERMINAL_AUTOLOGIN "$ENV{RADIX_X86_TERMINAL_AUTOLOGIN}" CACHE STRING "Start /bin/rash directly on the boot terminal")
if(NOT RADIX_X86_TERMINAL_AUTOLOGIN)
    set(RADIX_X86_TERMINAL_AUTOLOGIN "false")
endif()
set(RADIX_X86_MAX_USER_PROCESSES "$ENV{RADIX_X86_MAX_USER_PROCESSES}" CACHE STRING "Maximum x86 userspace processes tracked by the architecture backend")
if(NOT RADIX_X86_MAX_USER_PROCESSES)
    set(RADIX_X86_MAX_USER_PROCESSES "128")
endif()
set(RADIX_RKCONFIG_KERNEL_MAX_TASKS "$ENV{RADIX_RKCONFIG_KERNEL_MAX_TASKS}" CACHE STRING "RADix kernel task table entries")
if(NOT RADIX_RKCONFIG_KERNEL_MAX_TASKS)
    set(RADIX_RKCONFIG_KERNEL_MAX_TASKS "128")
endif()
set(RADIX_RKCONFIG_KERNEL_MAX_PROCESSES "$ENV{RADIX_RKCONFIG_KERNEL_MAX_PROCESSES}" CACHE STRING "RADix kernel process table entries")
if(NOT RADIX_RKCONFIG_KERNEL_MAX_PROCESSES)
    set(RADIX_RKCONFIG_KERNEL_MAX_PROCESSES "128")
endif()
set(RADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES "$ENV{RADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES}" CACHE STRING "Default RADix kernel task stack bytes")
if(NOT RADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES)
    set(RADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES "524288")
endif()
set(RADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY "$ENV{RADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY}" CACHE STRING "RADix kernel task stack policy: dynamic or static")
if(NOT RADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY)
    set(RADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY "dynamic")
endif()
set(RADIX_RKCONFIG_TERMINAL_VIM "$ENV{RADIX_RKCONFIG_TERMINAL_VIM}" CACHE STRING "RADix terminal vim package")
if(NOT RADIX_RKCONFIG_TERMINAL_VIM)
    set(RADIX_RKCONFIG_TERMINAL_VIM "false")
endif()
set(RADIX_RKCONFIG_TERMINAL_VIM_VARIANT "$ENV{RADIX_RKCONFIG_TERMINAL_VIM_VARIANT}" CACHE STRING "RADix terminal vim variant: none, tiny")
if(NOT RADIX_RKCONFIG_TERMINAL_VIM_VARIANT)
    set(RADIX_RKCONFIG_TERMINAL_VIM_VARIANT "none")
endif()
set(RADIX_RKCONFIG_NETWORK_ENABLED "$ENV{RADIX_RKCONFIG_NETWORK_ENABLED}" CACHE STRING "RADix static networking enable flag")
if(NOT RADIX_RKCONFIG_NETWORK_ENABLED)
    set(RADIX_RKCONFIG_NETWORK_ENABLED "true")
endif()
set(RADIX_RKCONFIG_NET_IPV4 "$ENV{RADIX_RKCONFIG_NET_IPV4}" CACHE STRING "RADix static IPv4 address")
if(NOT RADIX_RKCONFIG_NET_IPV4)
    set(RADIX_RKCONFIG_NET_IPV4 "10.0.2.15")
endif()
set(RADIX_RKCONFIG_NET_NETMASK "$ENV{RADIX_RKCONFIG_NET_NETMASK}" CACHE STRING "RADix static IPv4 netmask")
if(NOT RADIX_RKCONFIG_NET_NETMASK)
    set(RADIX_RKCONFIG_NET_NETMASK "255.255.255.0")
endif()
set(RADIX_RKCONFIG_NET_GATEWAY "$ENV{RADIX_RKCONFIG_NET_GATEWAY}" CACHE STRING "RADix static IPv4 gateway")
if(NOT RADIX_RKCONFIG_NET_GATEWAY)
    set(RADIX_RKCONFIG_NET_GATEWAY "10.0.2.2")
endif()
set(RADIX_RKCONFIG_NET_NTP_SERVER "$ENV{RADIX_RKCONFIG_NET_NTP_SERVER}" CACHE STRING "RADix NTP/SNTP server hostname or IPv4 address")
if(NOT RADIX_RKCONFIG_NET_NTP_SERVER)
    set(RADIX_RKCONFIG_NET_NTP_SERVER "time.google.com")
endif()
set(RADIX_RKCONFIG_NET_NTP_PORT "$ENV{RADIX_RKCONFIG_NET_NTP_PORT}" CACHE STRING "RADix NTP/SNTP UDP port")
if(NOT RADIX_RKCONFIG_NET_NTP_PORT)
    set(RADIX_RKCONFIG_NET_NTP_PORT "123")
endif()
set(RADIX_RKCONFIG_NET_DNS_SERVER "$ENV{RADIX_RKCONFIG_NET_DNS_SERVER}" CACHE STRING "RADix DNS resolver IPv4 address")
if(NOT RADIX_RKCONFIG_NET_DNS_SERVER)
    set(RADIX_RKCONFIG_NET_DNS_SERVER "10.0.2.3")
endif()
set(RADIX_RKCONFIG_TIMEZONE "$ENV{RADIX_RKCONFIG_TIMEZONE}" CACHE STRING "RADix default timezone")
if(NOT RADIX_RKCONFIG_TIMEZONE)
    set(RADIX_RKCONFIG_TIMEZONE "America/Los_Angeles")
endif()
set(RADIX_RKCONFIG_DNS_SEARCH "$ENV{RADIX_RKCONFIG_DNS_SEARCH}" CACHE STRING "RADix DNS search domain")
if(NOT RADIX_RKCONFIG_DNS_SEARCH)
    set(RADIX_RKCONFIG_DNS_SEARCH "local")
endif()
if((RADIX_RKCONFIG_TERMINAL_VIM STREQUAL "true" OR RADIX_RKCONFIG_TERMINAL_VIM STREQUAL "1" OR RADIX_RKCONFIG_TERMINAL_VIM STREQUAL "ON")
   AND RADIX_RKCONFIG_TERMINAL_VIM_VARIANT STREQUAL "none")
    set(RADIX_RKCONFIG_TERMINAL_VIM_VARIANT "tiny")
endif()
set(RADIX_RKCONFIG_TERMINAL_VIM_ENABLED FALSE)
if((RADIX_RKCONFIG_TERMINAL_VIM STREQUAL "true" OR RADIX_RKCONFIG_TERMINAL_VIM STREQUAL "1" OR RADIX_RKCONFIG_TERMINAL_VIM STREQUAL "ON")
   AND NOT RADIX_RKCONFIG_TERMINAL_VIM_VARIANT STREQUAL "none")
    set(RADIX_RKCONFIG_TERMINAL_VIM_ENABLED TRUE)
endif()
set(RADIX_X86_DIRECT_LOGIN "$ENV{RADIX_X86_DIRECT_LOGIN}" CACHE STRING "Launch /bin/login directly for interactive terminal boots")
if(NOT RADIX_X86_DIRECT_LOGIN)
    set(RADIX_X86_DIRECT_LOGIN "false")
endif()
set(RADIX_X86_TERMINAL_AUTOLOGIN_ENABLED FALSE)
if(RADIX_X86_TERMINAL_AUTOLOGIN STREQUAL "true" OR RADIX_X86_TERMINAL_AUTOLOGIN STREQUAL "1" OR RADIX_X86_TERMINAL_AUTOLOGIN STREQUAL "ON")
    set(RADIX_X86_TERMINAL_AUTOLOGIN_ENABLED TRUE)
endif()
set(RADIX_X86_TERMINAL_AUTOTEST_ENABLED FALSE)
if(RADIX_X86_TERMINAL_AUTOTEST_NANO STREQUAL "true" OR RADIX_X86_TERMINAL_AUTOTEST_NANO STREQUAL "1" OR RADIX_X86_TERMINAL_AUTOTEST_NANO STREQUAL "ON")
    set(RADIX_X86_TERMINAL_AUTOTEST_ENABLED TRUE)
endif()
set(RADIX_X86_DIRECT_LOGIN_ENABLED FALSE)
if(NOT RADIX_X86_TERMINAL_AUTOTEST_ENABLED AND (RADIX_X86_DIRECT_LOGIN STREQUAL "true" OR RADIX_X86_DIRECT_LOGIN STREQUAL "1" OR RADIX_X86_DIRECT_LOGIN STREQUAL "ON"))
    set(RADIX_X86_DIRECT_LOGIN_ENABLED TRUE)
endif()
set(RADIX_ENABLE_DEBUG_TRACE "$ENV{RADIX_ENABLE_DEBUG_TRACE}" CACHE STRING "Compile verbose RADix kernel debug traces")
if(NOT RADIX_ENABLE_DEBUG_TRACE)
    set(RADIX_ENABLE_DEBUG_TRACE "false")
endif()
set(RADIX_UPSTREAM_NCURSES_SOURCE "$ENV{RADIX_UPSTREAM_NCURSES_SOURCE}" CACHE PATH "Path to extracted upstream ncurses source")
set(RADIX_UPSTREAM_NANO_SOURCE "$ENV{RADIX_UPSTREAM_NANO_SOURCE}" CACHE PATH "Path to extracted upstream GNU nano source")
set(RADIX_UPSTREAM_VIM_SOURCE "$ENV{RADIX_UPSTREAM_VIM_SOURCE}" CACHE PATH "Path to extracted upstream Vim source")

set(RADIX_OS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH "Path to RADix-OS")
if(NOT RADIX_UPSTREAM_NCURSES_SOURCE)
    set(RADIX_UPSTREAM_NCURSES_SOURCE "${RADIX_OS_ROOT}/.radbuild/ports/src/ncurses-6.6")
endif()
if(NOT RADIX_UPSTREAM_NANO_SOURCE)
    set(RADIX_UPSTREAM_NANO_SOURCE "${RADIX_OS_ROOT}/.radbuild/ports/src/nano-9.0")
endif()
if(NOT RADIX_UPSTREAM_VIM_SOURCE)
    set(RADIX_UPSTREAM_VIM_SOURCE "${RADIX_OS_ROOT}/.radbuild/ports/src/vim")
endif()
if(RADIX_X86_TERMINAL_AUTOLOGIN_ENABLED)
    set(RADIX_BOOT_SESSION_EXEC "/bin/rash")
else()
    set(RADIX_BOOT_SESSION_EXEC "/bin/login")
endif()
if(RADIX_X86_TERMINAL_AUTOTEST_ENABLED)
    set(RADIX_TERMINAL_STRESS_AUTOSTART "true")
    set(RADIX_USERSPACE_SHELL_AUTOSTART "true")
else()
    set(RADIX_TERMINAL_STRESS_AUTOSTART "false")
    set(RADIX_USERSPACE_SHELL_AUTOSTART "false")
endif()
set(RADIX_GENERATED_SERVICES_DIR "${CMAKE_BINARY_DIR}/services")
file(MAKE_DIRECTORY "${RADIX_GENERATED_SERVICES_DIR}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/services/boot-session.json.in" "${RADIX_GENERATED_SERVICES_DIR}/boot-session.json" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/services/terminal-stress.json.in" "${RADIX_GENERATED_SERVICES_DIR}/terminal-stress.json" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/services/userspace-shell.json.in" "${RADIX_GENERATED_SERVICES_DIR}/userspace-shell.json" @ONLY)
if(NOT RADLIB_ROOT)
    get_filename_component(RADIX_OS_PARENT "${RADIX_OS_ROOT}/.." ABSOLUTE)
    set(RADLIB_ROOT "${RADIX_OS_PARENT}/RADLib" CACHE PATH "Path to RADLib")
endif()

set(RADLIB_EMBEDDED ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(RADIX_X86_UI_SOURCES)
set(RADIX_X86_UI_INCLUDE_DIRS)
set(RADIX_X86_UI_DEFINITIONS RADIX_X86_UI_PROFILE_TERMINAL=1)
if(RADIX_X86_UI_PROFILE STREQUAL "wm")
    set(SLINT_FEATURE_FREESTANDING ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_SOFTWARE ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_BACKEND_WINIT OFF CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_BACKEND_QT OFF CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_FEMTOVG OFF CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_SKIA OFF CACHE BOOL "" FORCE)
    set(DEFAULT_SLINT_EMBED_RESOURCES embed-for-software-renderer CACHE STRING "" FORCE)
    set(SLINT_SOURCE_DIR "${RADLIB_ROOT}/.radbuild/slint-src" CACHE PATH "Cached Slint source tree")
    if(NOT EXISTS "${SLINT_SOURCE_DIR}/api/cpp/CMakeLists.txt")
        message(FATAL_ERROR "Slint source tree not found at ${SLINT_SOURCE_DIR}; clone Slint into .radbuild/slint-src")
    endif()
    if(DEFINED ENV{RUSTFLAGS})
        set(ENV{RUSTFLAGS} "$ENV{RUSTFLAGS} -C panic=abort")
    else()
        set(ENV{RUSTFLAGS} "-C panic=abort")
    endif()
    add_subdirectory("${SLINT_SOURCE_DIR}/api/cpp" "${CMAKE_BINARY_DIR}/slint-cpp")
    if(TARGET slint_cpp-static)
        set_property(TARGET slint_cpp-static PROPERTY INTERFACE_LINK_LIBRARIES "")
    endif()
    set(RADIX_X86_UI_SOURCES
        slint_shell.cpp
        "${RADIX_OS_ROOT}/services/RADCompositor/RADCompositorCore.cpp"
    )
    set(RADIX_X86_UI_INCLUDE_DIRS "${RADIX_OS_ROOT}/services/RADCompositor")
    set(RADIX_X86_UI_DEFINITIONS SLINT_FEATURE_FREESTANDING SLINT_BACKEND=1 RADIX_X86_UI_PROFILE_WM=1)
endif()

add_executable(radixkernel_x86_64_grub_slint
    boot.S
    x86_cpu.S
    kernel.cpp
    ${RADIX_X86_UI_SOURCES}
    x86_cpu.cpp
    x86_vm.cpp
    x86_storage.cpp
    x86_ext2.cpp
    x86_ext4.cpp
    x86_fat.cpp
    x86_user.cpp
    hal_x86_64.cpp
    runtime.cpp
    "${RADIX_OS_ROOT}/RADixKernel/boot/radboot.cpp"
    "${RADIX_OS_ROOT}/RADixKernel/subsystems/RADixOverlayRuntime.cpp"
    "${RADIX_OS_ROOT}/RADixKernel/core/RADixKernel_runtime.cpp"
)

target_include_directories(radixkernel_x86_64_grub_slint PRIVATE
    "${RADIX_OS_ROOT}"
    "${RADIX_OS_ROOT}/RADixKernel/headers"
    "${RADIX_OS_ROOT}/RADixKernel/boot"
    "${RADIX_RKCONFIG_DIR}"
    ${RADIX_X86_UI_INCLUDE_DIRS}
)

if(RADIX_X86_UI_PROFILE STREQUAL "wm")
    target_link_libraries(radixkernel_x86_64_grub_slint PRIVATE Slint::Slint)
    SLINT_TARGET_SOURCES(radixkernel_x86_64_grub_slint
        "${RADIX_OS_ROOT}/services/RADCompositor/RADCompositor.slint"
    )
endif()

target_compile_options(radixkernel_x86_64_grub_slint PRIVATE
    -m64 -ffreestanding -fno-pic -fno-pie -fno-rtti -fno-exceptions
    -fno-stack-protector -mno-red-zone -Wall -Wextra
)

target_compile_definitions(radixkernel_x86_64_grub_slint PRIVATE
    ${RADIX_X86_UI_DEFINITIONS}
    RADLIB_EMBEDDED_NO_CPP_WRAPPERS=1
    RADIX_KERNEL_BACKEND_x86_64_grub=1
    RADIX_KERNEL_MAX_DEVICES=32
    RADIX_KERNEL_MAX_TASKS=${RADIX_RKCONFIG_KERNEL_MAX_TASKS}
    RADIX_KERNEL_MAX_PROCESSES=${RADIX_RKCONFIG_KERNEL_MAX_PROCESSES}
    RADIX_KERNEL_TASK_STACK_BYTES=${RADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES}
    RADIX_X86_MAX_USER_PROCESSES=${RADIX_X86_MAX_USER_PROCESSES}
)
if(RADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY STREQUAL "dynamic")
    target_compile_definitions(radixkernel_x86_64_grub_slint PRIVATE RADIX_KERNEL_TASK_DYNAMIC_STACKS=1)
else()
    target_compile_definitions(radixkernel_x86_64_grub_slint PRIVATE RADIX_KERNEL_TASK_DYNAMIC_STACKS=0)
endif()
if(RADIX_X86_TERMINAL_AUTOTEST_NANO STREQUAL "true" OR RADIX_X86_TERMINAL_AUTOTEST_NANO STREQUAL "1" OR RADIX_X86_TERMINAL_AUTOTEST_NANO STREQUAL "ON")
    target_compile_definitions(radixkernel_x86_64_grub_slint PRIVATE RADIX_X86_TERMINAL_AUTOTEST_NANO=1)
endif()
if(RADIX_X86_DIRECT_LOGIN_ENABLED)
    target_compile_definitions(radixkernel_x86_64_grub_slint PRIVATE RADIX_X86_DIRECT_LOGIN=1)
endif()
if(RADIX_X86_TERMINAL_AUTOLOGIN STREQUAL "true" OR RADIX_X86_TERMINAL_AUTOLOGIN STREQUAL "1" OR RADIX_X86_TERMINAL_AUTOLOGIN STREQUAL "ON")
    target_compile_definitions(radixkernel_x86_64_grub_slint PRIVATE RADIX_X86_TERMINAL_AUTOLOGIN=1)
endif()
if(RADIX_ENABLE_DEBUG_TRACE STREQUAL "true" OR RADIX_ENABLE_DEBUG_TRACE STREQUAL "1" OR RADIX_ENABLE_DEBUG_TRACE STREQUAL "ON")
    target_compile_definitions(radixkernel_x86_64_grub_slint PRIVATE RADIX_ENABLE_DEBUG_TRACE=1)
endif()

target_link_options(radixkernel_x86_64_grub_slint PRIVATE
    -m64 -nostdlib -static -static-libgcc -no-pie
    -Wl,-T,${CMAKE_CURRENT_LIST_DIR}/linker.ld
    -Wl,--no-dynamic-linker
    -Wl,--build-id=none
    -lgcc
)

set_target_properties(radixkernel_x86_64_grub_slint PROPERTIES
    OUTPUT_NAME "${RADIX_X86_KERNEL_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set(ISO_ROOT "${CMAKE_BINARY_DIR}/iso")
set(ISO_PATH "${CMAKE_BINARY_DIR}/${RADIX_X86_KERNEL_NAME}.iso")
set(RADIX_INIT_ELF "${CMAKE_BINARY_DIR}/radix-init.elf")
set(RADIX_RADINIT_ELF "${CMAKE_BINARY_DIR}/radix-radinit.elf")
set(RADIX_RADSH_ELF "${CMAKE_BINARY_DIR}/radix-radsh.elf")
set(RADIX_LOGIN_ELF "${CMAKE_BINARY_DIR}/radix-login.elf")
set(RADIX_NETWORK_SERVICE_ELF "${CMAKE_BINARY_DIR}/radix-network-service.elf")
set(RADIX_DNS_RESOLVER_ELF "${CMAKE_BINARY_DIR}/radix-dns-resolver.elf")
set(RADIX_TIME_SYNC_ELF "${CMAKE_BINARY_DIR}/radix-time-sync.elf")
set(RADIX_SH_ELF "${CMAKE_BINARY_DIR}/radix-sh.elf")
set(RADIX_RADGFX_ELF "${CMAKE_BINARY_DIR}/radix-radgfx-smoke.elf")
set(RADIX_TTY_STRESS_ELF "${CMAKE_BINARY_DIR}/radix-tty-stress.elf")
set(RADIX_SLEEP_STRESS_ELF "${CMAKE_BINARY_DIR}/radix-sleep-stress.elf")
set(RADIX_SIGNAL_STRESS_ELF "${CMAKE_BINARY_DIR}/radix-signal-stress.elf")
set(RADIX_TUI_STRESS_ELF "${CMAKE_BINARY_DIR}/radix-tui-stress.elf")
set(RADIX_TERMINAL_STRESS_ELF "${CMAKE_BINARY_DIR}/radix-terminal-stress.elf")
set(RADIX_HOST_TTY_STRESS "${CMAKE_BINARY_DIR}/host-tty-stress")
set(RADIX_HOST_SLEEP_STRESS "${CMAKE_BINARY_DIR}/host-sleep-stress")
set(RADIX_HOST_SIGNAL_STRESS "${CMAKE_BINARY_DIR}/host-signal-stress")
set(RADIX_HOST_TUI_STRESS "${CMAKE_BINARY_DIR}/host-tui-stress")
set(RADIX_NANO_TINY_ELF "${CMAKE_BINARY_DIR}/radix-nano-tiny.elf")
set(RADIX_NANO_FULL_ELF "${CMAKE_BINARY_DIR}/radix-nano-full.elf")
set(RADIX_UPSTREAM_NANO_ELF "${CMAKE_BINARY_DIR}/upstream-nano/src/nano")
set(RADIX_UPSTREAM_VIM_ELF "${CMAKE_BINARY_DIR}/upstream-vim/src/vim")
set(RADIX_UPSTREAM_VIM_LICENSE "${RADIX_UPSTREAM_VIM_SOURCE}/LICENSE")
set(RADIX_ROOTFS_DIR "${CMAKE_BINARY_DIR}/rootfs")
set(RADIX_RKCONFIG_STAMP "${RADIX_RKCONFIG_DIR}/rkconfig")
set(RADIX_TERMINFO_DIR "${CMAKE_BINARY_DIR}/terminfo")
set(RADIX_TERMINFO_STAMP "${RADIX_TERMINFO_DIR}/.radix-terminfo.stamp")
set(RADIX_SYSROOT_DIR "${CMAKE_BINARY_DIR}/radix-sysroot")
set(RADIX_SYSROOT_STAMP "${RADIX_SYSROOT_DIR}/.radix-sysroot.stamp")
set(RADIX_LIBC_OBJECT "${CMAKE_BINARY_DIR}/radix_libc.o")
set(RADIX_LIBC_ARCHIVE "${CMAKE_BINARY_DIR}/libradixc.a")
set(RADIX_NCURSES_BUILD_DIR "${CMAKE_BINARY_DIR}/upstream-ncurses")
set(RADIX_NCURSES_INSTALL_DIR "${CMAKE_BINARY_DIR}/upstream-ncurses-install")
set(RADIX_NCURSESW_ARCHIVE "${RADIX_NCURSES_INSTALL_DIR}/usr/lib/libncursesw.a")
set(RADIX_TINFO_ARCHIVE "${RADIX_NCURSES_INSTALL_DIR}/usr/lib/libtinfo.a")
set(RADIX_NCURSES_STAMP "${RADIX_NCURSES_INSTALL_DIR}/.radix-ncurses.stamp")
set(RADIX_EXT2_ROOTFS_IMAGE "${CMAKE_BINARY_DIR}/radix-rootfs.ext2")
set(RADIX_ROOTFS_IMAGE "${CMAKE_BINARY_DIR}/radix-rootfs.ext4")
set(RADIX_EXT4_WRITE_SEED "${CMAKE_BINARY_DIR}/radix-ext4-write-seed.txt")
set(RADIX_FAT_IMAGE "${CMAKE_BINARY_DIR}/radix-fat32.img")

add_custom_command(
    OUTPUT "${RADIX_RKCONFIG_STAMP}" "${RADIX_RKCONFIG_DIR}/rkconfig.h" "${RADIX_RKCONFIG_DIR}/issue" "${RADIX_RKCONFIG_DIR}/hostname" "${RADIX_RKCONFIG_DIR}/hosts" "${RADIX_RKCONFIG_DIR}/resolv.conf" "${RADIX_RKCONFIG_DIR}/network/netinterfaces.json" "${RADIX_RKCONFIG_DIR}/network/dns-resolver.json" "${RADIX_RKCONFIG_DIR}/time/time-sync.json" "${RADIX_RKCONFIG_DIR}/time/timezone" "${RADIX_RKCONFIG_DIR}/time/localtime" "${RADIX_RKCONFIG_DIR}/zoneinfo/${RADIX_RKCONFIG_TIMEZONE}" "${RADIX_RKCONFIG_DIR}/terminal.theme" "${RADIX_RKCONFIG_DIR}/radpm-index.json" "${RADIX_RKCONFIG_DIR}/passwd" "${RADIX_RKCONFIG_DIR}/passwords" "${RADIX_RKCONFIG_DIR}/rko-note.txt" "${RADIX_RKCONFIG_DIR}/rso-note.txt"
    COMMAND "${PYTHON3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/make_rkconfig.py"
            --out "${RADIX_RKCONFIG_DIR}"
            --hostname "${RADIX_RKCONFIG_HOSTNAME}"
            --root-password "${RADIX_RKCONFIG_ROOT_PASSWORD}"
            --rootfs-size-mb "${RADIX_RKCONFIG_ROOTFS_SIZE_MB}"
            --terminal-scale "${RADIX_RKCONFIG_TERMINAL_SCALE}"
            --terminal-font "${RADIX_RKCONFIG_TERMINAL_FONT}"
            --terminal-theme "${RADIX_RKCONFIG_TERMINAL_THEME}"
            --terminal-autocomplete "${RADIX_RKCONFIG_TERMINAL_AUTOCOMPLETE}"
            --terminal-posix-compat "${RADIX_RKCONFIG_TERMINAL_POSIX_COMPAT}"
            --terminal-ncurses "${RADIX_RKCONFIG_TERMINAL_NCURSES}"
            --terminal-nano "${RADIX_RKCONFIG_TERMINAL_NANO}"
            --terminal-nano-variant "${RADIX_RKCONFIG_TERMINAL_NANO_VARIANT}"
            --terminal-vim "${RADIX_RKCONFIG_TERMINAL_VIM}"
            --terminal-vim-variant "${RADIX_RKCONFIG_TERMINAL_VIM_VARIANT}"
            --network-enabled "${RADIX_RKCONFIG_NETWORK_ENABLED}"
            --net-ipv4 "${RADIX_RKCONFIG_NET_IPV4}"
            --net-netmask "${RADIX_RKCONFIG_NET_NETMASK}"
            --net-gateway "${RADIX_RKCONFIG_NET_GATEWAY}"
            --net-ntp-server "${RADIX_RKCONFIG_NET_NTP_SERVER}"
            --net-ntp-port "${RADIX_RKCONFIG_NET_NTP_PORT}"
            --net-dns-server "${RADIX_RKCONFIG_NET_DNS_SERVER}"
            --timezone "${RADIX_RKCONFIG_TIMEZONE}"
            --dns-search "${RADIX_RKCONFIG_DNS_SEARCH}"
            --kernel-max-tasks "${RADIX_RKCONFIG_KERNEL_MAX_TASKS}"
            --kernel-max-processes "${RADIX_RKCONFIG_KERNEL_MAX_PROCESSES}"
            --kernel-task-stack-bytes "${RADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES}"
            --kernel-task-stack-policy "${RADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/make_rkconfig.py"
    VERBATIM
)

add_custom_target(radix_rkconfig DEPENDS "${RADIX_RKCONFIG_STAMP}")
add_dependencies(radixkernel_x86_64_grub_slint radix_rkconfig)

add_custom_command(
    OUTPUT "${RADIX_TERMINFO_STAMP}" "${RADIX_TERMINFO_DIR}/r/radix"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${RADIX_TERMINFO_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RADIX_TERMINFO_DIR}"
    COMMAND "${TIC_EXECUTABLE}" -x -o "${RADIX_TERMINFO_DIR}" "${CMAKE_CURRENT_LIST_DIR}/terminfo/radix.terminfo"
    COMMAND ${CMAKE_COMMAND} -E touch "${RADIX_TERMINFO_STAMP}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/terminfo/radix.terminfo"
    VERBATIM
)

file(GLOB RADIX_SYSROOT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/sysroot/include/arpa/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/sysroot/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/sysroot/include/netinet/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/sysroot/include/sys/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/sysroot/include/radix/*.h"
)

add_custom_command(
    OUTPUT "${RADIX_SYSROOT_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${RADIX_SYSROOT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RADIX_SYSROOT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/sysroot" "${RADIX_SYSROOT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E touch "${RADIX_SYSROOT_STAMP}"
    DEPENDS
        ${RADIX_SYSROOT_HEADERS}
        "${CMAKE_CURRENT_LIST_DIR}/sysroot/lib/radix_libc.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_LIBC_OBJECT}" "${RADIX_LIBC_ARCHIVE}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}"
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -nostdinc
            -I "${CMAKE_CURRENT_LIST_DIR}/sysroot/include"
            -c "${CMAKE_CURRENT_LIST_DIR}/sysroot/lib/radix_libc.c"
            -o "${RADIX_LIBC_OBJECT}"
    COMMAND "${CMAKE_AR}" crs "${RADIX_LIBC_ARCHIVE}" "${RADIX_LIBC_OBJECT}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/sysroot/lib/radix_libc.c" ${RADIX_SYSROOT_HEADERS}
    VERBATIM
)

add_custom_target(radix_terminfo DEPENDS "${RADIX_TERMINFO_STAMP}")
add_custom_target(radix_sysroot DEPENDS "${RADIX_SYSROOT_STAMP}")
add_custom_target(radix_userspace_libc DEPENDS "${RADIX_LIBC_ARCHIVE}")

add_custom_command(
    OUTPUT "${RADIX_NCURSES_STAMP}" "${RADIX_NCURSESW_ARCHIVE}" "${RADIX_TINFO_ARCHIVE}"
    COMMAND ${CMAKE_COMMAND} -E env
            "RADIX_LIBC_ARCHIVE=${RADIX_LIBC_ARCHIVE}"
            "RADIX_BUILD_JOBS=$ENV{RADLIB_BUILD_JOBS}"
            "TIC_EXECUTABLE=${TIC_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/ports/upstream/build_upstream_ncurses.sh"
            "${RADIX_UPSTREAM_NCURSES_SOURCE}"
            "${RADIX_NCURSES_BUILD_DIR}"
            "${RADIX_NCURSES_INSTALL_DIR}"
            "${CMAKE_CURRENT_LIST_DIR}/sysroot"
    COMMAND ${CMAKE_COMMAND} -E touch "${RADIX_NCURSES_STAMP}"
    DEPENDS radix_userspace_libc radix_sysroot "${CMAKE_CURRENT_LIST_DIR}/ports/upstream/build_upstream_ncurses.sh" "${CMAKE_CURRENT_LIST_DIR}/terminfo/radix.terminfo"
    VERBATIM
)

add_custom_target(radix_ncurses DEPENDS "${RADIX_NCURSES_STAMP}" "${RADIX_NCURSESW_ARCHIVE}" "${RADIX_TINFO_ARCHIVE}")
add_custom_target(radix_userspace_libs DEPENDS "${RADIX_LIBC_ARCHIVE}" radix_ncurses)

if(EXISTS "${RADIX_UPSTREAM_NANO_SOURCE}/configure")
    add_custom_target(radix_upstream_nano
        COMMAND ${CMAKE_COMMAND} -E env
            "RADIX_SYSROOT_DIR=${CMAKE_CURRENT_LIST_DIR}/sysroot"
            "RADIX_NCURSES_INCLUDE_DIR=${RADIX_NCURSES_INSTALL_DIR}/usr/include"
            "RADIX_USERSPACE_LIB_DIR=${CMAKE_BINARY_DIR}/upstream-port-libs"
            "RADIX_LIBC_ARCHIVE=${RADIX_LIBC_ARCHIVE}"
            "RADIX_NCURSESW_ARCHIVE=${RADIX_NCURSESW_ARCHIVE}"
            "RADIX_TINFO_ARCHIVE=${RADIX_TINFO_ARCHIVE}"
            "${CMAKE_CURRENT_LIST_DIR}/ports/upstream/build_upstream_nano.sh"
            "${RADIX_UPSTREAM_NANO_SOURCE}"
            "${CMAKE_BINARY_DIR}/upstream-nano"
        DEPENDS radix_userspace_libs radix_sysroot
        VERBATIM
    )
else()
    add_custom_target(radix_upstream_nano
        COMMAND ${CMAKE_COMMAND} -E echo "RADix upstream nano source not found at ${RADIX_UPSTREAM_NANO_SOURCE}"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif()

if(EXISTS "${RADIX_UPSTREAM_VIM_SOURCE}/src/configure" OR EXISTS "${RADIX_UPSTREAM_VIM_SOURCE}/configure")
    add_custom_target(radix_upstream_vim_tiny
        COMMAND ${CMAKE_COMMAND} -E env
            "RADIX_SYSROOT_DIR=${CMAKE_CURRENT_LIST_DIR}/sysroot"
            "RADIX_NCURSES_INCLUDE_DIR=${RADIX_NCURSES_INSTALL_DIR}/usr/include"
            "RADIX_USERSPACE_LIB_DIR=${CMAKE_BINARY_DIR}/upstream-port-libs"
            "RADIX_LIBC_ARCHIVE=${RADIX_LIBC_ARCHIVE}"
            "RADIX_NCURSESW_ARCHIVE=${RADIX_NCURSESW_ARCHIVE}"
            "RADIX_TINFO_ARCHIVE=${RADIX_TINFO_ARCHIVE}"
            "${CMAKE_CURRENT_LIST_DIR}/ports/upstream/build_upstream_vim_tiny.sh"
            "${RADIX_UPSTREAM_VIM_SOURCE}"
            "${CMAKE_BINARY_DIR}/upstream-vim"
        DEPENDS radix_userspace_libs radix_sysroot
        VERBATIM
    )
else()
    add_custom_target(radix_upstream_vim_tiny
        COMMAND ${CMAKE_COMMAND} -E echo "RADix upstream Vim source not found at ${RADIX_UPSTREAM_VIM_SOURCE}"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif()

set(RADIX_NANO_DEPENDS)
set(RADIX_NANO_EXT2_WRITE_COMMANDS)
set(RADIX_NANO_EXT4_WRITE_COMMANDS)
if(RADIX_RKCONFIG_TERMINAL_NANO_ENABLED)
    list(APPEND RADIX_NANO_DEPENDS "${RADIX_NANO_TINY_ELF}" "${RADIX_NANO_FULL_ELF}" radix_upstream_nano)
    list(APPEND RADIX_NANO_EXT2_WRITE_COMMANDS
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_NANO_TINY_ELF} /usr/bin/nano-tiny" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_NANO_FULL_ELF} /usr/bin/nano-full" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_UPSTREAM_NANO_ELF} /usr/bin/nano" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix-nano.meta /etc/radix/packages/radix-nano.radpm" "${RADIX_EXT2_ROOTFS_IMAGE}"
    )
    list(APPEND RADIX_NANO_EXT4_WRITE_COMMANDS
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_NANO_TINY_ELF} /usr/bin/nano-tiny" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_NANO_FULL_ELF} /usr/bin/nano-full" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_UPSTREAM_NANO_ELF} /usr/bin/nano" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix-nano.meta /etc/radix/packages/radix-nano.radpm" "${RADIX_ROOTFS_IMAGE}"
    )
endif()

set(RADIX_VIM_DEPENDS)
set(RADIX_VIM_EXT2_WRITE_COMMANDS)
set(RADIX_VIM_EXT4_WRITE_COMMANDS)
if(RADIX_RKCONFIG_TERMINAL_VIM_ENABLED)
    list(APPEND RADIX_VIM_DEPENDS radix_upstream_vim_tiny "${CMAKE_CURRENT_LIST_DIR}/ports/vim/runtime/defaults.vim" "${CMAKE_CURRENT_LIST_DIR}/ports/vim/radix-vim-tiny.meta" "${RADIX_UPSTREAM_VIM_LICENSE}")
    list(APPEND RADIX_VIM_EXT2_WRITE_COMMANDS
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/vim" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/vim/vim92" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/licenses/vim" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_UPSTREAM_VIM_ELF} /usr/bin/vim" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_UPSTREAM_VIM_ELF} /usr/bin/vi" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/vim/runtime/defaults.vim /usr/share/vim/vim92/defaults.vim" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_UPSTREAM_VIM_LICENSE} /usr/share/licenses/vim/LICENSE" "${RADIX_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/vim/radix-vim-tiny.meta /etc/radix/packages/radix-vim-tiny.radpm" "${RADIX_EXT2_ROOTFS_IMAGE}"
    )
    list(APPEND RADIX_VIM_EXT4_WRITE_COMMANDS
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/vim" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/vim/vim92" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/licenses/vim" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_UPSTREAM_VIM_ELF} /usr/bin/vim" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_UPSTREAM_VIM_ELF} /usr/bin/vi" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/vim/runtime/defaults.vim /usr/share/vim/vim92/defaults.vim" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_UPSTREAM_VIM_LICENSE} /usr/share/licenses/vim/LICENSE" "${RADIX_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/vim/radix-vim-tiny.meta /etc/radix/packages/radix-vim-tiny.radpm" "${RADIX_ROOTFS_IMAGE}"
    )
endif()

add_custom_command(
    OUTPUT "${RADIX_INIT_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_INIT_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_init.S"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_init.S"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_RADSH_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${CMAKE_CURRENT_LIST_DIR}"
            -I "${RADIX_RKCONFIG_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_RADSH_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_radsh_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_radsh.c"
            "${CMAKE_CURRENT_LIST_DIR}/user_auth.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_radsh_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_radsh.c" "${CMAKE_CURRENT_LIST_DIR}/user_auth.c" "${CMAKE_CURRENT_LIST_DIR}/user_auth.h" "${RADIX_RKCONFIG_STAMP}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_LOGIN_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${CMAKE_CURRENT_LIST_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_LOGIN_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_login_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_login.c"
            "${CMAKE_CURRENT_LIST_DIR}/user_auth.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_login_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_login.c" "${CMAKE_CURRENT_LIST_DIR}/user_auth.c" "${CMAKE_CURRENT_LIST_DIR}/user_auth.h"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_NETWORK_SERVICE_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${CMAKE_CURRENT_LIST_DIR}/sysroot/include"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_NETWORK_SERVICE_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_network_service.c"
            "${RADIX_LIBC_ARCHIVE}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_network_service.c" "${RADIX_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_DNS_RESOLVER_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${CMAKE_CURRENT_LIST_DIR}/sysroot/include"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_DNS_RESOLVER_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_dns_resolver_service.c"
            "${RADIX_LIBC_ARCHIVE}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_dns_resolver_service.c" "${RADIX_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_TIME_SYNC_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${CMAKE_CURRENT_LIST_DIR}/sysroot/include"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_TIME_SYNC_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_time_sync_service.c"
            "${RADIX_LIBC_ARCHIVE}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_time_sync_service.c" "${RADIX_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_RADINIT_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_RADINIT_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_radinit_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_radinit.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_radinit_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_radinit.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_SH_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_SH_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_sh.S"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_sh.S"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_RADGFX_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_RADGFX_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_radgfx.S"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_radgfx.S"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_TTY_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -DRADIX_GUEST_STRESS_DIRECT=1
            -I "${CMAKE_CURRENT_LIST_DIR}/sysroot/include"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_TTY_STRESS_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_tty_stress.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_tty_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_SLEEP_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -DRADIX_GUEST_STRESS_DIRECT=1
            -I "${CMAKE_CURRENT_LIST_DIR}/sysroot/include"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_SLEEP_STRESS_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_sleep_stress.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_sleep_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_SIGNAL_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${CMAKE_CURRENT_LIST_DIR}/sysroot/include"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_SIGNAL_STRESS_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_signal_stress.c"
            "${RADIX_LIBC_ARCHIVE}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_signal_stress.c" "${RADIX_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_TUI_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${CMAKE_CURRENT_LIST_DIR}/sysroot/include"
            -I "${RADIX_NCURSES_INSTALL_DIR}/usr/include"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_TUI_STRESS_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_tui_stress.c"
            "${RADIX_NCURSESW_ARCHIVE}"
            "${RADIX_TINFO_ARCHIVE}"
            "${RADIX_LIBC_ARCHIVE}"
            "${RADIX_LIBC_ARCHIVE}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_tui_stress.c" "${RADIX_NCURSESW_ARCHIVE}" "${RADIX_TINFO_ARCHIVE}" "${RADIX_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_TERMINAL_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_TERMINAL_STRESS_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_terminal_stress.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_c_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_terminal_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_HOST_TTY_STRESS}"
    COMMAND "${CMAKE_C_COMPILER}" -Wall -Wextra -O2 -D_DEFAULT_SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/user_tty_stress.c"
            -o "${RADIX_HOST_TTY_STRESS}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_tty_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_HOST_SLEEP_STRESS}"
    COMMAND "${CMAKE_C_COMPILER}" -Wall -Wextra -O2
            "${CMAKE_CURRENT_LIST_DIR}/user_sleep_stress.c"
            -o "${RADIX_HOST_SLEEP_STRESS}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_sleep_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_HOST_SIGNAL_STRESS}"
    COMMAND "${CMAKE_C_COMPILER}" -Wall -Wextra -O2
            "${CMAKE_CURRENT_LIST_DIR}/user_signal_stress.c"
            -o "${RADIX_HOST_SIGNAL_STRESS}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_signal_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_HOST_TUI_STRESS}"
    COMMAND "${CMAKE_C_COMPILER}" -Wall -Wextra -O2 -D_DEFAULT_SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/user_tui_stress.c"
            -o "${RADIX_HOST_TUI_STRESS}"
            -lncursesw
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_tui_stress.c"
    VERBATIM
)

add_custom_target(radix_host_tui_stress DEPENDS "${RADIX_HOST_TUI_STRESS}")
add_custom_target(radix_host_terminal_stress
    DEPENDS "${RADIX_HOST_TTY_STRESS}" "${RADIX_HOST_SLEEP_STRESS}" "${RADIX_HOST_SIGNAL_STRESS}" "${RADIX_HOST_TUI_STRESS}"
)
add_custom_target(radix_host_terminal_stress_run
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/run_host_terminal_stress.sh"
            "${RADIX_HOST_TTY_STRESS}"
            "${RADIX_HOST_SLEEP_STRESS}"
            "${RADIX_HOST_SIGNAL_STRESS}"
            "${RADIX_HOST_TUI_STRESS}"
            "${CMAKE_BINARY_DIR}/host-terminal-stress.log"
    DEPENDS radix_host_terminal_stress "${CMAKE_CURRENT_LIST_DIR}/run_host_terminal_stress.sh"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_NANO_TINY_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -DRADIX_NANO_FULL=0
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_NANO_TINY_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix_nano_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix_nano.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix_nano_entry.S" "${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix_nano.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_NANO_FULL_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -DRADIX_NANO_FULL=1
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_NANO_FULL_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix_nano_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix_nano.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix_nano_entry.S" "${CMAKE_CURRENT_LIST_DIR}/ports/nano/radix_nano.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RADIX_ROOTFS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E touch "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND truncate -s 32M "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${MKFS_EXT2_EXECUTABLE}" -q -F "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /bin" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /sbin" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /dev" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/services" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/network" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/time" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/packages" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /home" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /home/root" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /mnt" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /mnt/fat" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/bin" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/lib" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/licenses" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/zoneinfo" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/zoneinfo/America" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/terminfo" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/terminfo/r" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib/radix" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib/radix/modules" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /tmp" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log/radix" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /run" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADINIT_ELF} /bin/init" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADINIT_ELF} /sbin/radinit" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_NETWORK_SERVICE_ELF} /sbin/network-service" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_DNS_RESOLVER_ELF} /sbin/dns-resolver" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TIME_SYNC_ELF} /sbin/time-sync" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/radsh" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/rash" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/sh" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/tui-smoke" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_LOGIN_ELF} /bin/login" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADGFX_ELF} /bin/radgfx-smoke" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TTY_STRESS_ELF} /usr/bin/tty-stress" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_SLEEP_STRESS_ELF} /usr/bin/sleep-stress" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_SIGNAL_STRESS_ELF} /usr/bin/signal-stress" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TUI_STRESS_ELF} /usr/bin/tui-stress" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TERMINAL_STRESS_ELF} /usr/bin/terminal-stress" "${RADIX_EXT2_ROOTFS_IMAGE}"
    ${RADIX_NANO_EXT2_WRITE_COMMANDS}
    ${RADIX_VIM_EXT2_WRITE_COMMANDS}
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/rkconfig /etc/radix/rkconfig" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/terminal.theme /etc/radix/terminal.theme" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/radpm-index.json /etc/radix/radpm-index.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/issue /etc/issue" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/hostname /etc/hostname" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/hosts /etc/hosts" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/resolv.conf /etc/resolv.conf" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/network/netinterfaces.json /etc/radix/network/netinterfaces.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/network/dns-resolver.json /etc/radix/network/dns-resolver.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/time/time-sync.json /etc/radix/time/time-sync.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/time/timezone /etc/timezone" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/time/localtime /etc/localtime" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/zoneinfo/${RADIX_RKCONFIG_TIMEZONE} /usr/share/zoneinfo/${RADIX_RKCONFIG_TIMEZONE}" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/passwd /etc/passwd" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/passwords /etc/passwords" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/rko-note.txt /lib/radix/modules/README.rko" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/rso-note.txt /lib/README.rso" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_LIBC_ARCHIVE} /usr/lib/libradixc.a" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_NCURSESW_ARCHIVE} /usr/lib/libncursesw.a" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TINFO_ARCHIVE} /usr/lib/libtinfo.a" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/ncurses/radix-ncurses.meta /etc/radix/packages/radix-ncurses.radpm" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/radix-network.meta /etc/radix/packages/radix-network.radpm" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/radix-dns-resolver.meta /etc/radix/packages/radix-dns-resolver.radpm" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/radix-time-sync.meta /etc/radix/packages/radix-time-sync.radpm" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/radix-tzdata.meta /etc/radix/packages/radix-tzdata.radpm" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TERMINFO_DIR}/r/radix /usr/share/terminfo/r/radix" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/test_script.sh /bin/test.sh" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json /etc/radix/services/storage-root.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network.json /etc/radix/services/network.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/dns-resolver.json /etc/radix/services/dns-resolver.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/time-sync.json /etc/radix/services/time-sync.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_GENERATED_SERVICES_DIR}/userspace-shell.json /etc/radix/services/userspace-shell.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json /etc/radix/services/base-terminal.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_GENERATED_SERVICES_DIR}/terminal-stress.json /etc/radix/services/terminal-stress.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_GENERATED_SERVICES_DIR}/boot-session.json /etc/radix/services/boot-session.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json /etc/radix/services/network-smoke.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json /etc/radix/services/fatfs.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    DEPENDS "${RADIX_RADINIT_ELF}" "${RADIX_INIT_ELF}" "${RADIX_RADSH_ELF}" "${RADIX_LOGIN_ELF}" "${RADIX_NETWORK_SERVICE_ELF}" "${RADIX_DNS_RESOLVER_ELF}" "${RADIX_TIME_SYNC_ELF}" "${RADIX_SH_ELF}" "${RADIX_RADGFX_ELF}" "${RADIX_TTY_STRESS_ELF}" "${RADIX_SLEEP_STRESS_ELF}" "${RADIX_SIGNAL_STRESS_ELF}" "${RADIX_TUI_STRESS_ELF}" "${RADIX_TERMINAL_STRESS_ELF}" ${RADIX_NANO_DEPENDS} ${RADIX_VIM_DEPENDS} "${RADIX_LIBC_ARCHIVE}" "${RADIX_NCURSESW_ARCHIVE}" "${RADIX_TINFO_ARCHIVE}" "${RADIX_RKCONFIG_STAMP}" "${RADIX_TERMINFO_STAMP}" "${RADIX_SYSROOT_STAMP}" "${CMAKE_CURRENT_LIST_DIR}/test_script.sh"
            "${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json" "${RADIX_GENERATED_SERVICES_DIR}/userspace-shell.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/network.json" "${CMAKE_CURRENT_LIST_DIR}/services/dns-resolver.json" "${CMAKE_CURRENT_LIST_DIR}/services/time-sync.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json" "${RADIX_GENERATED_SERVICES_DIR}/terminal-stress.json" "${RADIX_GENERATED_SERVICES_DIR}/boot-session.json" "${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_ROOTFS_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${RADIX_ROOTFS_IMAGE}" "${RADIX_EXT4_WRITE_SEED}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RADIX_ROOTFS_DIR}"
    COMMAND /bin/sh -c "printf 'RADix ext4 write path seed padding for in-place write test\\n' > '${RADIX_EXT4_WRITE_SEED}'"
    COMMAND ${CMAKE_COMMAND} -E touch "${RADIX_ROOTFS_IMAGE}"
    COMMAND truncate -s "${RADIX_RKCONFIG_ROOTFS_SIZE_MB}M" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${MKFS_EXT4_EXECUTABLE}" -q -F -O extent,^has_journal,^64bit,^metadata_csum "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /bin" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /sbin" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /dev" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/services" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/network" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/time" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/packages" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /home" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /home/root" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /mnt" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /mnt/fat" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/bin" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/lib" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/licenses" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/zoneinfo" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/zoneinfo/America" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/terminfo" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/terminfo/r" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib/radix" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib/radix/modules" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /tmp" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log/radix" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /run" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADINIT_ELF} /bin/init" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADINIT_ELF} /sbin/radinit" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_NETWORK_SERVICE_ELF} /sbin/network-service" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_DNS_RESOLVER_ELF} /sbin/dns-resolver" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TIME_SYNC_ELF} /sbin/time-sync" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/radsh" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/rash" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/sh" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/tui-smoke" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_LOGIN_ELF} /bin/login" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADGFX_ELF} /bin/radgfx-smoke" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TTY_STRESS_ELF} /usr/bin/tty-stress" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_SLEEP_STRESS_ELF} /usr/bin/sleep-stress" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_SIGNAL_STRESS_ELF} /usr/bin/signal-stress" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TUI_STRESS_ELF} /usr/bin/tui-stress" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TERMINAL_STRESS_ELF} /usr/bin/terminal-stress" "${RADIX_ROOTFS_IMAGE}"
    ${RADIX_NANO_EXT4_WRITE_COMMANDS}
    ${RADIX_VIM_EXT4_WRITE_COMMANDS}
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/rkconfig /etc/radix/rkconfig" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/terminal.theme /etc/radix/terminal.theme" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/radpm-index.json /etc/radix/radpm-index.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/issue /etc/issue" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/hostname /etc/hostname" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/hosts /etc/hosts" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/resolv.conf /etc/resolv.conf" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/network/netinterfaces.json /etc/radix/network/netinterfaces.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/network/dns-resolver.json /etc/radix/network/dns-resolver.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/time/time-sync.json /etc/radix/time/time-sync.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/time/timezone /etc/timezone" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/time/localtime /etc/localtime" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/zoneinfo/${RADIX_RKCONFIG_TIMEZONE} /usr/share/zoneinfo/${RADIX_RKCONFIG_TIMEZONE}" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/passwd /etc/passwd" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/passwords /etc/passwords" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/rko-note.txt /lib/radix/modules/README.rko" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RKCONFIG_DIR}/rso-note.txt /lib/README.rso" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_LIBC_ARCHIVE} /usr/lib/libradixc.a" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_NCURSESW_ARCHIVE} /usr/lib/libncursesw.a" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TINFO_ARCHIVE} /usr/lib/libtinfo.a" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/ncurses/radix-ncurses.meta /etc/radix/packages/radix-ncurses.radpm" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/radix-network.meta /etc/radix/packages/radix-network.radpm" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/radix-dns-resolver.meta /etc/radix/packages/radix-dns-resolver.radpm" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/radix-time-sync.meta /etc/radix/packages/radix-time-sync.radpm" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/radix-tzdata.meta /etc/radix/packages/radix-tzdata.radpm" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_TERMINFO_DIR}/r/radix /usr/share/terminfo/r/radix" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/test_script.sh /bin/test.sh" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json /etc/radix/services/storage-root.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network.json /etc/radix/services/network.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/dns-resolver.json /etc/radix/services/dns-resolver.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/time-sync.json /etc/radix/services/time-sync.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_GENERATED_SERVICES_DIR}/userspace-shell.json /etc/radix/services/userspace-shell.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json /etc/radix/services/base-terminal.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_GENERATED_SERVICES_DIR}/terminal-stress.json /etc/radix/services/terminal-stress.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_GENERATED_SERVICES_DIR}/boot-session.json /etc/radix/services/boot-session.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json /etc/radix/services/network-smoke.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json /etc/radix/services/fatfs.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_EXT4_WRITE_SEED} /tmp/radwrite.txt" "${RADIX_ROOTFS_IMAGE}"
    DEPENDS "${RADIX_RADINIT_ELF}" "${RADIX_INIT_ELF}" "${RADIX_RADSH_ELF}" "${RADIX_LOGIN_ELF}" "${RADIX_NETWORK_SERVICE_ELF}" "${RADIX_DNS_RESOLVER_ELF}" "${RADIX_TIME_SYNC_ELF}" "${RADIX_SH_ELF}" "${RADIX_RADGFX_ELF}" "${RADIX_TTY_STRESS_ELF}" "${RADIX_SLEEP_STRESS_ELF}" "${RADIX_SIGNAL_STRESS_ELF}" "${RADIX_TUI_STRESS_ELF}" "${RADIX_TERMINAL_STRESS_ELF}" ${RADIX_NANO_DEPENDS} ${RADIX_VIM_DEPENDS} "${RADIX_LIBC_ARCHIVE}" "${RADIX_NCURSESW_ARCHIVE}" "${RADIX_TINFO_ARCHIVE}" "${RADIX_RKCONFIG_STAMP}" "${RADIX_TERMINFO_STAMP}" "${RADIX_SYSROOT_STAMP}" "${CMAKE_CURRENT_LIST_DIR}/test_script.sh"
            "${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json" "${RADIX_GENERATED_SERVICES_DIR}/userspace-shell.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/network.json" "${CMAKE_CURRENT_LIST_DIR}/services/dns-resolver.json" "${CMAKE_CURRENT_LIST_DIR}/services/time-sync.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json" "${RADIX_GENERATED_SERVICES_DIR}/terminal-stress.json" "${RADIX_GENERATED_SERVICES_DIR}/boot-session.json" "${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json"
    VERBATIM
)

add_custom_target(radix_rootfs ALL DEPENDS "${RADIX_ROOTFS_IMAGE}" "${RADIX_EXT2_ROOTFS_IMAGE}" radix_terminfo radix_sysroot)

add_custom_command(
    OUTPUT "${RADIX_FAT_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${RADIX_FAT_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E touch "${RADIX_FAT_IMAGE}"
    COMMAND truncate -s 64M "${RADIX_FAT_IMAGE}"
    COMMAND "${MKFS_FAT_EXECUTABLE}" -F 32 -n RADFAT "${RADIX_FAT_IMAGE}"
    VERBATIM
)

add_custom_target(radix_test_volumes ALL DEPENDS "${RADIX_FAT_IMAGE}")

add_custom_command(
    OUTPUT "${ISO_PATH}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${ISO_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_ROOT}/boot/grub"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:radixkernel_x86_64_grub_slint>" "${ISO_ROOT}/boot/radixkernel-x86-64-grub.elf"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/grub.cfg" "${ISO_ROOT}/boot/grub/grub.cfg"
    COMMAND "${GRUB_MKRESCUE_EXECUTABLE}" -o "${ISO_PATH}" "${ISO_ROOT}"
    DEPENDS radixkernel_x86_64_grub_slint "${CMAKE_CURRENT_LIST_DIR}/grub.cfg"
    VERBATIM
)

add_custom_target(radixkernel_x86_64_grub_slint_iso ALL DEPENDS "${ISO_PATH}" radix_rootfs radix_test_volumes)
