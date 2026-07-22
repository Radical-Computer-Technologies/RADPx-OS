cmake_minimum_required(VERSION 3.21)

project(radkernel_x86_64_grub C CXX ASM)

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
    message(FATAL_ERROR "RADPx-OS rootfs packaging requires x86_64-linux-gnu-gcc, mkfs.ext2, mkfs.ext4, mkfs.fat, debugfs, and python3")
endif()
if(NOT TIC_EXECUTABLE)
    message(FATAL_ERROR "RADPx-OS ncurses compatibility requires tic from ncurses-bin")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RAD_X86_UI_PROFILE "wm" CACHE STRING "RADPx-OS x86_64 UI profile: wm or terminal")
set_property(CACHE RAD_X86_UI_PROFILE PROPERTY STRINGS wm terminal)
if(NOT RAD_X86_UI_PROFILE STREQUAL "wm" AND NOT RAD_X86_UI_PROFILE STREQUAL "terminal")
    message(FATAL_ERROR "RAD_X86_UI_PROFILE must be wm or terminal")
endif()
set(RAD_X86_KERNEL_NAME "radkernel-x86-64-grub-${RAD_X86_UI_PROFILE}")
set(RAD_RKCONFIG_DIR "${CMAKE_BINARY_DIR}/rkconfig")
set(RAD_RKCONFIG_HOSTNAME "$ENV{RAD_RKCONFIG_HOSTNAME}" CACHE STRING "RADPx-OS hostname")
if(NOT RAD_RKCONFIG_HOSTNAME)
    set(RAD_RKCONFIG_HOSTNAME "rad")
endif()
set(RAD_RKCONFIG_ROOT_PASSWORD "$ENV{RAD_RKCONFIG_ROOT_PASSWORD}" CACHE STRING "RADPx-OS root dev password")
if(NOT RAD_RKCONFIG_ROOT_PASSWORD)
    set(RAD_RKCONFIG_ROOT_PASSWORD "rad")
endif()
set(RAD_RKCONFIG_ROOTFS_SIZE_MB "$ENV{RAD_RKCONFIG_ROOTFS_SIZE_MB}" CACHE STRING "RADPx-OS ext4 rootfs size in MiB")
if(NOT RAD_RKCONFIG_ROOTFS_SIZE_MB)
    set(RAD_RKCONFIG_ROOTFS_SIZE_MB "256")
endif()
set(RAD_RKCONFIG_TERMINAL_SCALE "$ENV{RAD_RKCONFIG_TERMINAL_SCALE}" CACHE STRING "RADPx-OS terminal scale")
if(NOT RAD_RKCONFIG_TERMINAL_SCALE)
    set(RAD_RKCONFIG_TERMINAL_SCALE "auto")
endif()
set(RAD_RKCONFIG_TERMINAL_FONT "$ENV{RAD_RKCONFIG_TERMINAL_FONT}" CACHE STRING "RADPx-OS terminal font")
if(NOT RAD_RKCONFIG_TERMINAL_FONT)
    set(RAD_RKCONFIG_TERMINAL_FONT "rad-default")
endif()
set(RAD_RKCONFIG_TERMINAL_THEME "$ENV{RAD_RKCONFIG_TERMINAL_THEME}" CACHE STRING "RADPx-OS terminal theme")
if(NOT RAD_RKCONFIG_TERMINAL_THEME)
    set(RAD_RKCONFIG_TERMINAL_THEME "rad-dark")
endif()
set(RAD_RKCONFIG_TERMINAL_AUTOCOMPLETE "$ENV{RAD_RKCONFIG_TERMINAL_AUTOCOMPLETE}" CACHE STRING "RADPx-OS terminal autocomplete")
if(NOT RAD_RKCONFIG_TERMINAL_AUTOCOMPLETE)
    set(RAD_RKCONFIG_TERMINAL_AUTOCOMPLETE "true")
endif()
set(RAD_RKCONFIG_TERMINAL_POSIX_COMPAT "$ENV{RAD_RKCONFIG_TERMINAL_POSIX_COMPAT}" CACHE STRING "RADPx-OS terminal POSIX compatibility")
if(NOT RAD_RKCONFIG_TERMINAL_POSIX_COMPAT)
    set(RAD_RKCONFIG_TERMINAL_POSIX_COMPAT "true")
endif()
set(RAD_RKCONFIG_TERMINAL_NCURSES "$ENV{RAD_RKCONFIG_TERMINAL_NCURSES}" CACHE STRING "RADPx-OS terminal ncurses compatibility assets")
if(NOT RAD_RKCONFIG_TERMINAL_NCURSES)
    set(RAD_RKCONFIG_TERMINAL_NCURSES "true")
endif()
set(RAD_RKCONFIG_TERMINAL_NANO "$ENV{RAD_RKCONFIG_TERMINAL_NANO}" CACHE STRING "RADPx-OS terminal nano package")
if(NOT RAD_RKCONFIG_TERMINAL_NANO)
    set(RAD_RKCONFIG_TERMINAL_NANO "false")
endif()
set(RAD_RKCONFIG_TERMINAL_NANO_VARIANT "$ENV{RAD_RKCONFIG_TERMINAL_NANO_VARIANT}" CACHE STRING "RADPx-OS terminal nano variant: none, tiny, full, both")
if(NOT RAD_RKCONFIG_TERMINAL_NANO_VARIANT)
    set(RAD_RKCONFIG_TERMINAL_NANO_VARIANT "none")
endif()
set(RAD_RKCONFIG_TERMINAL_NANO_ENABLED FALSE)
if((RAD_RKCONFIG_TERMINAL_NANO STREQUAL "true" OR RAD_RKCONFIG_TERMINAL_NANO STREQUAL "1" OR RAD_RKCONFIG_TERMINAL_NANO STREQUAL "ON")
   AND NOT RAD_RKCONFIG_TERMINAL_NANO_VARIANT STREQUAL "none")
    set(RAD_RKCONFIG_TERMINAL_NANO_ENABLED TRUE)
endif()
set(RAD_X86_TERMINAL_AUTOTEST_NANO "$ENV{RAD_X86_TERMINAL_AUTOTEST_NANO}" CACHE STRING "Run deterministic PTY nano smoke on terminal boot")
if(NOT RAD_X86_TERMINAL_AUTOTEST_NANO)
    set(RAD_X86_TERMINAL_AUTOTEST_NANO "false")
endif()
set(RAD_X86_TERMINAL_AUTOLOGIN "$ENV{RAD_X86_TERMINAL_AUTOLOGIN}" CACHE STRING "Start /bin/rash directly on the boot terminal")
if(NOT RAD_X86_TERMINAL_AUTOLOGIN)
    set(RAD_X86_TERMINAL_AUTOLOGIN "false")
endif()
set(RAD_X86_MAX_USER_PROCESSES "$ENV{RAD_X86_MAX_USER_PROCESSES}" CACHE STRING "Maximum x86 userspace processes tracked by the architecture backend")
if(NOT RAD_X86_MAX_USER_PROCESSES)
    set(RAD_X86_MAX_USER_PROCESSES "128")
endif()
set(RAD_RKCONFIG_KERNEL_MAX_TASKS "$ENV{RAD_RKCONFIG_KERNEL_MAX_TASKS}" CACHE STRING "RADPx-OS kernel task table entries")
if(NOT RAD_RKCONFIG_KERNEL_MAX_TASKS)
    set(RAD_RKCONFIG_KERNEL_MAX_TASKS "128")
endif()
set(RAD_RKCONFIG_KERNEL_MAX_PROCESSES "$ENV{RAD_RKCONFIG_KERNEL_MAX_PROCESSES}" CACHE STRING "RADPx-OS kernel process table entries")
if(NOT RAD_RKCONFIG_KERNEL_MAX_PROCESSES)
    set(RAD_RKCONFIG_KERNEL_MAX_PROCESSES "128")
endif()
set(RAD_RKCONFIG_KERNEL_TASK_STACK_BYTES "$ENV{RAD_RKCONFIG_KERNEL_TASK_STACK_BYTES}" CACHE STRING "Default RADPx-OS kernel task stack bytes")
if(NOT RAD_RKCONFIG_KERNEL_TASK_STACK_BYTES)
    set(RAD_RKCONFIG_KERNEL_TASK_STACK_BYTES "524288")
endif()
set(RAD_RKCONFIG_KERNEL_TASK_STACK_POLICY "$ENV{RAD_RKCONFIG_KERNEL_TASK_STACK_POLICY}" CACHE STRING "RADPx-OS kernel task stack policy: dynamic or static")
if(NOT RAD_RKCONFIG_KERNEL_TASK_STACK_POLICY)
    set(RAD_RKCONFIG_KERNEL_TASK_STACK_POLICY "dynamic")
endif()
set(RAD_RKCONFIG_TERMINAL_VIM "$ENV{RAD_RKCONFIG_TERMINAL_VIM}" CACHE STRING "RADPx-OS terminal vim package")
if(NOT RAD_RKCONFIG_TERMINAL_VIM)
    set(RAD_RKCONFIG_TERMINAL_VIM "false")
endif()
set(RAD_RKCONFIG_TERMINAL_VIM_VARIANT "$ENV{RAD_RKCONFIG_TERMINAL_VIM_VARIANT}" CACHE STRING "RADPx-OS terminal vim variant: none, tiny")
if(NOT RAD_RKCONFIG_TERMINAL_VIM_VARIANT)
    set(RAD_RKCONFIG_TERMINAL_VIM_VARIANT "none")
endif()
set(RAD_RKCONFIG_NETWORK_ENABLED "$ENV{RAD_RKCONFIG_NETWORK_ENABLED}" CACHE STRING "RADPx-OS static networking enable flag")
if(NOT RAD_RKCONFIG_NETWORK_ENABLED)
    set(RAD_RKCONFIG_NETWORK_ENABLED "true")
endif()
set(RAD_RKCONFIG_NET_IPV4 "$ENV{RAD_RKCONFIG_NET_IPV4}" CACHE STRING "RADPx-OS static IPv4 address")
if(NOT RAD_RKCONFIG_NET_IPV4)
    set(RAD_RKCONFIG_NET_IPV4 "10.0.2.15")
endif()
set(RAD_RKCONFIG_NET_NETMASK "$ENV{RAD_RKCONFIG_NET_NETMASK}" CACHE STRING "RADPx-OS static IPv4 netmask")
if(NOT RAD_RKCONFIG_NET_NETMASK)
    set(RAD_RKCONFIG_NET_NETMASK "255.255.255.0")
endif()
set(RAD_RKCONFIG_NET_GATEWAY "$ENV{RAD_RKCONFIG_NET_GATEWAY}" CACHE STRING "RADPx-OS static IPv4 gateway")
if(NOT RAD_RKCONFIG_NET_GATEWAY)
    set(RAD_RKCONFIG_NET_GATEWAY "10.0.2.2")
endif()
set(RAD_RKCONFIG_NET_NTP_SERVER "$ENV{RAD_RKCONFIG_NET_NTP_SERVER}" CACHE STRING "RADPx-OS NTP/SNTP server hostname or IPv4 address")
if(NOT RAD_RKCONFIG_NET_NTP_SERVER)
    set(RAD_RKCONFIG_NET_NTP_SERVER "time.google.com")
endif()
set(RAD_RKCONFIG_NET_NTP_PORT "$ENV{RAD_RKCONFIG_NET_NTP_PORT}" CACHE STRING "RADPx-OS NTP/SNTP UDP port")
if(NOT RAD_RKCONFIG_NET_NTP_PORT)
    set(RAD_RKCONFIG_NET_NTP_PORT "123")
endif()
set(RAD_RKCONFIG_NET_DNS_SERVER "$ENV{RAD_RKCONFIG_NET_DNS_SERVER}" CACHE STRING "RADPx-OS DNS resolver IPv4 address")
if(NOT RAD_RKCONFIG_NET_DNS_SERVER)
    set(RAD_RKCONFIG_NET_DNS_SERVER "10.0.2.3")
endif()
set(RAD_RKCONFIG_TIMEZONE "$ENV{RAD_RKCONFIG_TIMEZONE}" CACHE STRING "RADPx-OS default timezone")
if(NOT RAD_RKCONFIG_TIMEZONE)
    set(RAD_RKCONFIG_TIMEZONE "America/Los_Angeles")
endif()
set(RAD_RADPM_ROOTFS_DIR "$ENV{RAD_RADPM_ROOTFS_DIR}" CACHE PATH "Optional staged RADPx-OS .radpm rootfs payload directory")
set(RAD_RADPM_ROOTFS_STAMP "$ENV{RAD_RADPM_ROOTFS_STAMP}" CACHE FILEPATH "Optional staged RADPx-OS .radpm payload stamp")
set(RAD_RKCONFIG_DNS_SEARCH "$ENV{RAD_RKCONFIG_DNS_SEARCH}" CACHE STRING "RADPx-OS DNS search domain")
if(NOT RAD_RKCONFIG_DNS_SEARCH)
    set(RAD_RKCONFIG_DNS_SEARCH "local")
endif()
if((RAD_RKCONFIG_TERMINAL_VIM STREQUAL "true" OR RAD_RKCONFIG_TERMINAL_VIM STREQUAL "1" OR RAD_RKCONFIG_TERMINAL_VIM STREQUAL "ON")
   AND RAD_RKCONFIG_TERMINAL_VIM_VARIANT STREQUAL "none")
    set(RAD_RKCONFIG_TERMINAL_VIM_VARIANT "tiny")
endif()
set(RAD_RKCONFIG_TERMINAL_VIM_ENABLED FALSE)
if((RAD_RKCONFIG_TERMINAL_VIM STREQUAL "true" OR RAD_RKCONFIG_TERMINAL_VIM STREQUAL "1" OR RAD_RKCONFIG_TERMINAL_VIM STREQUAL "ON")
   AND NOT RAD_RKCONFIG_TERMINAL_VIM_VARIANT STREQUAL "none")
    set(RAD_RKCONFIG_TERMINAL_VIM_ENABLED TRUE)
endif()
set(RAD_X86_DIRECT_LOGIN "$ENV{RAD_X86_DIRECT_LOGIN}" CACHE STRING "Launch /bin/login directly for interactive terminal boots")
if(NOT RAD_X86_DIRECT_LOGIN)
    set(RAD_X86_DIRECT_LOGIN "false")
endif()
set(RAD_X86_TERMINAL_AUTOLOGIN_ENABLED FALSE)
if(RAD_X86_TERMINAL_AUTOLOGIN STREQUAL "true" OR RAD_X86_TERMINAL_AUTOLOGIN STREQUAL "1" OR RAD_X86_TERMINAL_AUTOLOGIN STREQUAL "ON")
    set(RAD_X86_TERMINAL_AUTOLOGIN_ENABLED TRUE)
endif()
set(RAD_X86_TERMINAL_AUTOTEST_ENABLED FALSE)
if(RAD_X86_TERMINAL_AUTOTEST_NANO STREQUAL "true" OR RAD_X86_TERMINAL_AUTOTEST_NANO STREQUAL "1" OR RAD_X86_TERMINAL_AUTOTEST_NANO STREQUAL "ON")
    set(RAD_X86_TERMINAL_AUTOTEST_ENABLED TRUE)
endif()
set(RAD_X86_DIRECT_LOGIN_ENABLED FALSE)
if(NOT RAD_X86_TERMINAL_AUTOTEST_ENABLED AND (RAD_X86_DIRECT_LOGIN STREQUAL "true" OR RAD_X86_DIRECT_LOGIN STREQUAL "1" OR RAD_X86_DIRECT_LOGIN STREQUAL "ON"))
    set(RAD_X86_DIRECT_LOGIN_ENABLED TRUE)
endif()
set(RAD_ENABLE_DEBUG_TRACE "$ENV{RAD_ENABLE_DEBUG_TRACE}" CACHE STRING "Compile verbose RADPx-OS kernel debug traces")
if(NOT RAD_ENABLE_DEBUG_TRACE)
    set(RAD_ENABLE_DEBUG_TRACE "false")
endif()
set(RAD_UPSTREAM_NCURSES_SOURCE "$ENV{RAD_UPSTREAM_NCURSES_SOURCE}" CACHE PATH "Path to extracted upstream ncurses source")
set(RAD_UPSTREAM_NANO_SOURCE "$ENV{RAD_UPSTREAM_NANO_SOURCE}" CACHE PATH "Path to extracted upstream GNU nano source")
set(RAD_UPSTREAM_VIM_SOURCE "$ENV{RAD_UPSTREAM_VIM_SOURCE}" CACHE PATH "Path to extracted upstream Vim source")

set(RADPX_OS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH "Path to RADPx-OS")
set(RAD_X86_PLATFORM_SOURCE_DIR "${RADPX_OS_ROOT}/RADKernel/platforms/x86_64")
set(RAD_FS_SOURCE_DIR "${RADPX_OS_ROOT}/RADKernel/fs")
set(RAD_SERVICE_MANAGER_SOURCE_DIR "${RADPX_OS_ROOT}/services/RADServiceManager")
set(RAD_NETWORK_SERVICE_SOURCE_DIR "${RADPX_OS_ROOT}/services/RADNetwork")
set(RAD_DNS_RESOLVER_SOURCE_DIR "${RADPX_OS_ROOT}/services/RADDNSResolver")
set(RAD_TIME_SYNC_SOURCE_DIR "${RADPX_OS_ROOT}/services/RADTimeSync")
set(RAD_USERLAND_RUNTIME_SOURCE_DIR "${RADPX_OS_ROOT}/userland/runtime")
set(RAD_USERLAND_INIT_SOURCE_DIR "${RADPX_OS_ROOT}/userland/core/init")
set(RAD_USERLAND_RASH_SOURCE_DIR "${RADPX_OS_ROOT}/userland/core/rash")
set(RAD_USERLAND_LOGIN_SOURCE_DIR "${RADPX_OS_ROOT}/userland/core/login")
set(RAD_USERLAND_RADGFX_SOURCE_DIR "${RADPX_OS_ROOT}/userland/core/radgfx")
set(RAD_USERLAND_RADWC_DEMO_SOURCE_DIR "${RADPX_OS_ROOT}/userland/core/radwc-demo")
set(RAD_LIBRADCOMPOSITOR_SOURCE_DIR "${RADPX_OS_ROOT}/userland/lib/radcompositor")
set(RAD_USERSPACE_TEST_SOURCE_DIR "${RADPX_OS_ROOT}/tests/userspace")
set(RAD_LIBRADC_SOURCE_DIR "${RADPX_OS_ROOT}/libs/libradc")
set(RAD_LIBRADC_INCLUDE_DIR "${RAD_LIBRADC_SOURCE_DIR}/include")
set(RAD_LIBRADC_SOURCE "${RAD_LIBRADC_SOURCE_DIR}/rad_libc.c")
if(NOT RAD_UPSTREAM_NCURSES_SOURCE)
    set(RAD_UPSTREAM_NCURSES_SOURCE "${RADPX_OS_ROOT}/.radbuild/ports/src/ncurses-6.6")
endif()
if(NOT RAD_UPSTREAM_NANO_SOURCE)
    set(RAD_UPSTREAM_NANO_SOURCE "${RADPX_OS_ROOT}/.radbuild/ports/src/nano-9.0")
endif()
if(NOT RAD_UPSTREAM_VIM_SOURCE)
    set(RAD_UPSTREAM_VIM_SOURCE "${RADPX_OS_ROOT}/.radbuild/ports/src/vim")
endif()
if(RAD_X86_TERMINAL_AUTOLOGIN_ENABLED)
    set(RAD_BOOT_SESSION_EXEC "/bin/rash")
else()
    set(RAD_BOOT_SESSION_EXEC "/bin/login")
endif()
if(RAD_X86_TERMINAL_AUTOTEST_ENABLED)
    set(RAD_TERMINAL_STRESS_AUTOSTART "false")
    set(RAD_TERMINAL_STRESS_ORDER "35")
    set(RAD_USERSPACE_SHELL_AUTOSTART "false")
    set(RAD_BOOT_SESSION_ORDER "36")
    set(RAD_BOOT_SESSION_AFTER "\"base-terminal\"")
else()
    set(RAD_TERMINAL_STRESS_AUTOSTART "false")
    set(RAD_TERMINAL_STRESS_ORDER "60")
    set(RAD_USERSPACE_SHELL_AUTOSTART "false")
    set(RAD_BOOT_SESSION_ORDER "70")
    set(RAD_BOOT_SESSION_AFTER "\"terminal-stress\"")
endif()
set(RAD_GENERATED_SERVICES_DIR "${CMAKE_BINARY_DIR}/services")
file(MAKE_DIRECTORY "${RAD_GENERATED_SERVICES_DIR}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/services/boot-session.json.in" "${RAD_GENERATED_SERVICES_DIR}/boot-session.json" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/services/terminal-stress.json.in" "${RAD_GENERATED_SERVICES_DIR}/terminal-stress.json" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/services/userspace-shell.json.in" "${RAD_GENERATED_SERVICES_DIR}/userspace-shell.json" @ONLY)
if(NOT RADLIB_ROOT)
    get_filename_component(RADPX_OS_PARENT "${RADPX_OS_ROOT}/.." ABSOLUTE)
    set(RADLIB_ROOT "${RADPX_OS_PARENT}/RADLib" CACHE PATH "Path to RADLib")
endif()

set(RADLIB_EMBEDDED ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(RAD_X86_UI_SOURCES)
set(RAD_X86_UI_INCLUDE_DIRS)
set(RAD_X86_UI_DEFINITIONS RAD_X86_UI_PROFILE_TERMINAL=1)
if(RAD_X86_UI_PROFILE STREQUAL "wm")
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
    # -C opt-level=2 is REQUIRED, not just an optimization: the unoptimized (cargo dev)
    # Slint build produces enormous Rust stack frames, and a deep freestanding render
    # (RadDesktopSurface::create / text layout / render_by_line) overflows the kernel boot
    # stack. Because a single >4KB frame steps over the lone 4KB stack guard page, the
    # overflow silently corrupts adjacent memory instead of trapping -> intermittent wild
    # #GP/#PF and Slint panics at shifting locations. opt-level=2 shrinks the frames so the
    # render fits the stack. RUSTFLAGS -C opt-level wins over the dev profile's opt-level=0.
    if(DEFINED ENV{RUSTFLAGS})
        set(ENV{RUSTFLAGS} "$ENV{RUSTFLAGS} -C panic=abort -C opt-level=2")
    else()
        set(ENV{RUSTFLAGS} "-C panic=abort -C opt-level=2")
    endif()
    add_subdirectory("${SLINT_SOURCE_DIR}/api/cpp" "${CMAKE_BINARY_DIR}/slint-cpp")
    if(TARGET slint_cpp-static)
        set_property(TARGET slint_cpp-static PROPERTY INTERFACE_LINK_LIBRARIES "")
    endif()
    set(RAD_X86_UI_SOURCES
        "${RADPX_OS_ROOT}/services/RADCompositor/SlintShell.cpp"
        "${RADPX_OS_ROOT}/services/RADCompositor/RADCompositorCore.cpp"
        "${RADPX_OS_ROOT}/services/RADCompositor/RADCompositorTier.cpp"
    )
    set(RAD_X86_UI_INCLUDE_DIRS "${RADPX_OS_ROOT}/services/RADCompositor")
    set(RAD_X86_UI_DEFINITIONS SLINT_FEATURE_FREESTANDING SLINT_BACKEND=1 RAD_X86_UI_PROFILE_WM=1)
endif()

add_executable(radkernel_x86_64_grub_slint
    "${RAD_X86_PLATFORM_SOURCE_DIR}/boot.S"
    "${RAD_X86_PLATFORM_SOURCE_DIR}/x86_cpu.S"
    "${RAD_X86_PLATFORM_SOURCE_DIR}/kernel.cpp"
    ${RAD_X86_UI_SOURCES}
    "${RAD_X86_PLATFORM_SOURCE_DIR}/x86_cpu.cpp"
    "${RAD_X86_PLATFORM_SOURCE_DIR}/x86_vm.cpp"
    "${RAD_X86_PLATFORM_SOURCE_DIR}/x86_storage.cpp"
    "${RAD_FS_SOURCE_DIR}/ext2.cpp"
    "${RAD_FS_SOURCE_DIR}/ext4.cpp"
    "${RAD_FS_SOURCE_DIR}/fat.cpp"
    "${RAD_X86_PLATFORM_SOURCE_DIR}/x86_user.cpp"
    "${RAD_X86_PLATFORM_SOURCE_DIR}/hal_x86_64.cpp"
    "${RADPX_OS_ROOT}/RADKernel/runtime/freestanding_runtime.cpp"
    "${RADPX_OS_ROOT}/RADKernel/boot/radboot.cpp"
    "${RADPX_OS_ROOT}/RADKernel/subsystems/RADOverlayRuntime.cpp"
    "${RADPX_OS_ROOT}/RADKernel/core/RADKernel_runtime.cpp"
)

target_include_directories(radkernel_x86_64_grub_slint PRIVATE
    "${RADPX_OS_ROOT}"
    "${RAD_X86_PLATFORM_SOURCE_DIR}"
    "${RAD_FS_SOURCE_DIR}"
    "${RADPX_OS_ROOT}/RADKernel/headers"
    "${RADPX_OS_ROOT}/RADKernel/boot"
    "${RAD_RKCONFIG_DIR}"
    ${RAD_X86_UI_INCLUDE_DIRS}
)

if(RAD_X86_UI_PROFILE STREQUAL "wm")
    target_link_libraries(radkernel_x86_64_grub_slint PRIVATE Slint::Slint)
    SLINT_TARGET_SOURCES(radkernel_x86_64_grub_slint
        "${RADPX_OS_ROOT}/services/RADCompositor/RADCompositor.slint"
    )
endif()

target_compile_options(radkernel_x86_64_grub_slint PRIVATE
    -m64 -ffreestanding -fno-pic -fno-pie -fno-rtti -fno-exceptions
    -fno-stack-protector -mno-red-zone -Wall -Wextra
)

target_compile_definitions(radkernel_x86_64_grub_slint PRIVATE
    ${RAD_X86_UI_DEFINITIONS}
    RADLIB_EMBEDDED_NO_CPP_WRAPPERS=1
    RAD_KERNEL_BACKEND_x86_64_grub=1
    RAD_KERNEL_MAX_DEVICES=32
    RAD_KERNEL_MAX_TASKS=${RAD_RKCONFIG_KERNEL_MAX_TASKS}
    RAD_KERNEL_MAX_PROCESSES=${RAD_RKCONFIG_KERNEL_MAX_PROCESSES}
    RAD_KERNEL_TASK_STACK_BYTES=${RAD_RKCONFIG_KERNEL_TASK_STACK_BYTES}
    RAD_X86_MAX_USER_PROCESSES=${RAD_X86_MAX_USER_PROCESSES}
)
if(RAD_RKCONFIG_KERNEL_TASK_STACK_POLICY STREQUAL "dynamic")
    target_compile_definitions(radkernel_x86_64_grub_slint PRIVATE RAD_KERNEL_TASK_DYNAMIC_STACKS=1)
else()
    target_compile_definitions(radkernel_x86_64_grub_slint PRIVATE RAD_KERNEL_TASK_DYNAMIC_STACKS=0)
endif()
if(RAD_X86_TERMINAL_AUTOTEST_NANO STREQUAL "true" OR RAD_X86_TERMINAL_AUTOTEST_NANO STREQUAL "1" OR RAD_X86_TERMINAL_AUTOTEST_NANO STREQUAL "ON")
    target_compile_definitions(radkernel_x86_64_grub_slint PRIVATE RAD_X86_TERMINAL_AUTOTEST_NANO=1)
endif()
if(RAD_X86_DIRECT_LOGIN_ENABLED)
    target_compile_definitions(radkernel_x86_64_grub_slint PRIVATE RAD_X86_DIRECT_LOGIN=1)
endif()
if(RAD_X86_TERMINAL_AUTOLOGIN STREQUAL "true" OR RAD_X86_TERMINAL_AUTOLOGIN STREQUAL "1" OR RAD_X86_TERMINAL_AUTOLOGIN STREQUAL "ON")
    target_compile_definitions(radkernel_x86_64_grub_slint PRIVATE RAD_X86_TERMINAL_AUTOLOGIN=1)
endif()
if(RAD_ENABLE_DEBUG_TRACE STREQUAL "true" OR RAD_ENABLE_DEBUG_TRACE STREQUAL "1" OR RAD_ENABLE_DEBUG_TRACE STREQUAL "ON")
    target_compile_definitions(radkernel_x86_64_grub_slint PRIVATE RAD_ENABLE_DEBUG_TRACE=1)
endif()

target_link_options(radkernel_x86_64_grub_slint PRIVATE
    -m64 -nostdlib -static -static-libgcc -no-pie
    -Wl,-T,${RAD_X86_PLATFORM_SOURCE_DIR}/linker.ld
    -Wl,--no-dynamic-linker
    -Wl,--build-id=none
    -lgcc
)

set_target_properties(radkernel_x86_64_grub_slint PROPERTIES
    OUTPUT_NAME "${RAD_X86_KERNEL_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set(ISO_ROOT "${CMAKE_BINARY_DIR}/iso")
set(ISO_PATH "${CMAKE_BINARY_DIR}/${RAD_X86_KERNEL_NAME}.iso")
set(RAD_INIT_ELF "${CMAKE_BINARY_DIR}/rad-init.elf")
set(RAD_RADINIT_ELF "${CMAKE_BINARY_DIR}/rad-radinit.elf")
set(RAD_RADSH_ELF "${CMAKE_BINARY_DIR}/rad-radsh.elf")
set(RAD_LOGIN_ELF "${CMAKE_BINARY_DIR}/rad-login.elf")
set(RAD_NETWORK_SERVICE_ELF "${CMAKE_BINARY_DIR}/rad-network-service.elf")
set(RAD_DNS_RESOLVER_ELF "${CMAKE_BINARY_DIR}/rad-dns-resolver.elf")
set(RAD_TIME_SYNC_ELF "${CMAKE_BINARY_DIR}/rad-time-sync.elf")
set(RAD_SH_ELF "${CMAKE_BINARY_DIR}/rad-sh.elf")
set(RAD_RADGFX_ELF "${CMAKE_BINARY_DIR}/rad-radgfx-smoke.elf")
set(RAD_RADWC_DEMO_ELF "${CMAKE_BINARY_DIR}/rad-radwc-demo.elf")
set(RAD_TTY_STRESS_ELF "${CMAKE_BINARY_DIR}/rad-tty-stress.elf")
set(RAD_SLEEP_STRESS_ELF "${CMAKE_BINARY_DIR}/rad-sleep-stress.elf")
set(RAD_SIGNAL_STRESS_ELF "${CMAKE_BINARY_DIR}/rad-signal-stress.elf")
set(RAD_POSIX_STRESS_ELF "${CMAKE_BINARY_DIR}/rad-posix-stress.elf")
set(RAD_TUI_STRESS_ELF "${CMAKE_BINARY_DIR}/rad-tui-stress.elf")
set(RAD_TUI_STRESS_DYN_ELF "${CMAKE_BINARY_DIR}/rad-tui-stress-dyn.elf")
set(RAD_LIBC_RSO_STRESS_ELF "${CMAKE_BINARY_DIR}/rad-libc-rso-stress.elf")
set(RAD_TINFO_RSO_STRESS_ELF "${CMAKE_BINARY_DIR}/rad-tinfo-rso-stress.elf")
set(RAD_TERMINAL_STRESS_ELF "${CMAKE_BINARY_DIR}/rad-terminal-stress.elf")
set(RAD_HOST_TTY_STRESS "${CMAKE_BINARY_DIR}/host-tty-stress")
set(RAD_HOST_SLEEP_STRESS "${CMAKE_BINARY_DIR}/host-sleep-stress")
set(RAD_HOST_SIGNAL_STRESS "${CMAKE_BINARY_DIR}/host-signal-stress")
set(RAD_HOST_POSIX_STRESS "${CMAKE_BINARY_DIR}/host-posix-stress")
set(RAD_HOST_TUI_STRESS "${CMAKE_BINARY_DIR}/host-tui-stress")
set(RAD_NANO_TINY_ELF "${CMAKE_BINARY_DIR}/rad-nano-tiny.elf")
set(RAD_NANO_FULL_ELF "${CMAKE_BINARY_DIR}/rad-nano-full.elf")
set(RAD_UPSTREAM_NANO_ELF "${CMAKE_BINARY_DIR}/upstream-nano/src/nano")
set(RAD_UPSTREAM_VIM_ELF "${CMAKE_BINARY_DIR}/upstream-vim/src/vim")
set(RAD_UPSTREAM_VIM_LICENSE "${RAD_UPSTREAM_VIM_SOURCE}/LICENSE")
set(RAD_ROOTFS_DIR "${CMAKE_BINARY_DIR}/rootfs")
set(RAD_RKCONFIG_STAMP "${RAD_RKCONFIG_DIR}/rkconfig")
set(RAD_TERMINFO_DIR "${CMAKE_BINARY_DIR}/terminfo")
set(RAD_TERMINFO_STAMP "${RAD_TERMINFO_DIR}/.rad-terminfo.stamp")
set(RAD_SYSROOT_DIR "${CMAKE_BINARY_DIR}/rad-sysroot")
set(RAD_SYSROOT_STAMP "${RAD_SYSROOT_DIR}/.rad-sysroot.stamp")
set(RAD_LIBC_OBJECT "${CMAKE_BINARY_DIR}/rad_libc.o")
set(RAD_LIBC_PIC_OBJECT "${CMAKE_BINARY_DIR}/rad_libc.pic.o")
set(RAD_LIBC_ARCHIVE "${CMAKE_BINARY_DIR}/libradc.a")
set(RAD_LIBC_RSO "${CMAKE_BINARY_DIR}/libradc.rso")
set(RAD_NCURSES_BUILD_DIR "${CMAKE_BINARY_DIR}/upstream-ncurses")
set(RAD_NCURSES_INSTALL_DIR "${CMAKE_BINARY_DIR}/upstream-ncurses-install")
set(RAD_NCURSESW_ARCHIVE "${RAD_NCURSES_INSTALL_DIR}/usr/lib/libncursesw.a")
set(RAD_TINFO_ARCHIVE "${RAD_NCURSES_INSTALL_DIR}/usr/lib/libtinfo.a")
set(RAD_NCURSES_STAMP "${RAD_NCURSES_INSTALL_DIR}/.rad-ncurses.stamp")
set(RAD_TINFO_RSO "${CMAKE_BINARY_DIR}/libtinfo.rso")
set(RAD_LIBNCURSES_RSO "${CMAKE_BINARY_DIR}/libncurses.rso")
set(RAD_LIBNCURSESW_RSO "${CMAKE_BINARY_DIR}/libncursesw.rso")
set(RAD_EXT2_ROOTFS_IMAGE "${CMAKE_BINARY_DIR}/rad-rootfs.ext2")
set(RAD_ROOTFS_IMAGE "${CMAKE_BINARY_DIR}/rad-rootfs.ext4")
set(RAD_EXT4_WRITE_SEED "${CMAKE_BINARY_DIR}/rad-ext4-write-seed.txt")
set(RAD_FAT_IMAGE "${CMAKE_BINARY_DIR}/rad-fat32.img")
set(RAD_RADPM_ROOTFS_DEPENDS "")
if(RAD_RADPM_ROOTFS_STAMP)
    list(APPEND RAD_RADPM_ROOTFS_DEPENDS "${RAD_RADPM_ROOTFS_STAMP}")
endif()

add_custom_command(
    OUTPUT "${RAD_RKCONFIG_STAMP}" "${RAD_RKCONFIG_DIR}/rkconfig.h" "${RAD_RKCONFIG_DIR}/issue" "${RAD_RKCONFIG_DIR}/hostname" "${RAD_RKCONFIG_DIR}/hosts" "${RAD_RKCONFIG_DIR}/resolv.conf" "${RAD_RKCONFIG_DIR}/network/netinterfaces.json" "${RAD_RKCONFIG_DIR}/network/dns-resolver.json" "${RAD_RKCONFIG_DIR}/time/time-sync.json" "${RAD_RKCONFIG_DIR}/time/timezone" "${RAD_RKCONFIG_DIR}/time/localtime" "${RAD_RKCONFIG_DIR}/zoneinfo/${RAD_RKCONFIG_TIMEZONE}" "${RAD_RKCONFIG_DIR}/terminal.theme" "${RAD_RKCONFIG_DIR}/radpm-index.json" "${RAD_RKCONFIG_DIR}/passwd" "${RAD_RKCONFIG_DIR}/passwords" "${RAD_RKCONFIG_DIR}/rko-note.txt" "${RAD_RKCONFIG_DIR}/rso-note.txt"
    COMMAND "${PYTHON3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/make_rkconfig.py"
            --out "${RAD_RKCONFIG_DIR}"
            --hostname "${RAD_RKCONFIG_HOSTNAME}"
            --root-password "${RAD_RKCONFIG_ROOT_PASSWORD}"
            --rootfs-size-mb "${RAD_RKCONFIG_ROOTFS_SIZE_MB}"
            --terminal-scale "${RAD_RKCONFIG_TERMINAL_SCALE}"
            --terminal-font "${RAD_RKCONFIG_TERMINAL_FONT}"
            --terminal-theme "${RAD_RKCONFIG_TERMINAL_THEME}"
            --terminal-autocomplete "${RAD_RKCONFIG_TERMINAL_AUTOCOMPLETE}"
            --terminal-posix-compat "${RAD_RKCONFIG_TERMINAL_POSIX_COMPAT}"
            --terminal-ncurses "${RAD_RKCONFIG_TERMINAL_NCURSES}"
            --terminal-nano "${RAD_RKCONFIG_TERMINAL_NANO}"
            --terminal-nano-variant "${RAD_RKCONFIG_TERMINAL_NANO_VARIANT}"
            --terminal-vim "${RAD_RKCONFIG_TERMINAL_VIM}"
            --terminal-vim-variant "${RAD_RKCONFIG_TERMINAL_VIM_VARIANT}"
            --network-enabled "${RAD_RKCONFIG_NETWORK_ENABLED}"
            --net-ipv4 "${RAD_RKCONFIG_NET_IPV4}"
            --net-netmask "${RAD_RKCONFIG_NET_NETMASK}"
            --net-gateway "${RAD_RKCONFIG_NET_GATEWAY}"
            --net-ntp-server "${RAD_RKCONFIG_NET_NTP_SERVER}"
            --net-ntp-port "${RAD_RKCONFIG_NET_NTP_PORT}"
            --net-dns-server "${RAD_RKCONFIG_NET_DNS_SERVER}"
            --timezone "${RAD_RKCONFIG_TIMEZONE}"
            --dns-search "${RAD_RKCONFIG_DNS_SEARCH}"
            --kernel-max-tasks "${RAD_RKCONFIG_KERNEL_MAX_TASKS}"
            --kernel-max-processes "${RAD_RKCONFIG_KERNEL_MAX_PROCESSES}"
            --kernel-task-stack-bytes "${RAD_RKCONFIG_KERNEL_TASK_STACK_BYTES}"
            --kernel-task-stack-policy "${RAD_RKCONFIG_KERNEL_TASK_STACK_POLICY}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/make_rkconfig.py"
    VERBATIM
)

add_custom_target(rad_rkconfig DEPENDS "${RAD_RKCONFIG_STAMP}")
add_dependencies(radkernel_x86_64_grub_slint rad_rkconfig)

add_custom_command(
    OUTPUT "${RAD_TERMINFO_STAMP}" "${RAD_TERMINFO_DIR}/r/rad"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${RAD_TERMINFO_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RAD_TERMINFO_DIR}"
    COMMAND "${TIC_EXECUTABLE}" -x -o "${RAD_TERMINFO_DIR}" "${CMAKE_CURRENT_LIST_DIR}/terminfo/rad.terminfo"
    COMMAND ${CMAKE_COMMAND} -E touch "${RAD_TERMINFO_STAMP}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/terminfo/rad.terminfo"
    VERBATIM
)

file(GLOB RAD_SYSROOT_HEADERS CONFIGURE_DEPENDS
    "${RAD_LIBRADC_INCLUDE_DIR}/arpa/*.h"
    "${RAD_LIBRADC_INCLUDE_DIR}/*.h"
    "${RAD_LIBRADC_INCLUDE_DIR}/netinet/*.h"
    "${RAD_LIBRADC_INCLUDE_DIR}/sys/*.h"
    "${RAD_LIBRADC_INCLUDE_DIR}/rad/*.h"
)

add_custom_command(
    OUTPUT "${RAD_SYSROOT_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${RAD_SYSROOT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RAD_SYSROOT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RAD_SYSROOT_DIR}/include"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${RAD_LIBRADC_INCLUDE_DIR}" "${RAD_SYSROOT_DIR}/include"
    COMMAND ${CMAKE_COMMAND} -E touch "${RAD_SYSROOT_STAMP}"
    DEPENDS
        ${RAD_SYSROOT_HEADERS}
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_LIBC_OBJECT}" "${RAD_LIBC_PIC_OBJECT}" "${RAD_LIBC_ARCHIVE}" "${RAD_LIBC_RSO}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}"
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -nostdinc
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -c "${RAD_LIBRADC_SOURCE}"
            -o "${RAD_LIBC_OBJECT}"
    COMMAND "${CMAKE_AR}" crs "${RAD_LIBC_ARCHIVE}" "${RAD_LIBC_OBJECT}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}"
            -ffreestanding -fPIC -fno-stack-protector -fcf-protection=none -mno-red-zone -nostdinc
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -c "${RAD_LIBRADC_SOURCE}"
            -o "${RAD_LIBC_PIC_OBJECT}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -shared
            -Wl,--hash-style=sysv
            -Wl,--build-id=none
            -Wl,-soname,libradc.rso
            -o "${RAD_LIBC_RSO}"
            "${RAD_LIBC_PIC_OBJECT}"
    DEPENDS "${RAD_LIBRADC_SOURCE}" ${RAD_SYSROOT_HEADERS}
    VERBATIM
)

add_custom_target(rad_terminfo DEPENDS "${RAD_TERMINFO_STAMP}")
add_custom_target(rad_sysroot DEPENDS "${RAD_SYSROOT_STAMP}")
add_custom_target(rad_userspace_libc DEPENDS "${RAD_LIBC_ARCHIVE}" "${RAD_LIBC_RSO}")

add_custom_command(
    OUTPUT "${RAD_NCURSES_STAMP}" "${RAD_NCURSESW_ARCHIVE}" "${RAD_TINFO_ARCHIVE}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${RAD_NCURSES_BUILD_DIR}" "${RAD_NCURSES_INSTALL_DIR}"
    COMMAND ${CMAKE_COMMAND} -E env
            "RAD_LIBC_ARCHIVE=${RAD_LIBC_ARCHIVE}"
            "RAD_NCURSES_PIC=1"
            "RAD_BUILD_JOBS=$ENV{RADLIB_BUILD_JOBS}"
            "TIC_EXECUTABLE=${TIC_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/ports/upstream/build_upstream_ncurses.sh"
            "${RAD_UPSTREAM_NCURSES_SOURCE}"
            "${RAD_NCURSES_BUILD_DIR}"
            "${RAD_NCURSES_INSTALL_DIR}"
            "${RAD_SYSROOT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E touch "${RAD_NCURSES_STAMP}"
    DEPENDS rad_userspace_libc rad_sysroot "${CMAKE_CURRENT_LIST_DIR}/ports/upstream/build_upstream_ncurses.sh" "${CMAKE_CURRENT_LIST_DIR}/terminfo/rad.terminfo"
    VERBATIM
)

add_custom_target(rad_ncurses DEPENDS "${RAD_NCURSES_STAMP}" "${RAD_NCURSESW_ARCHIVE}" "${RAD_TINFO_ARCHIVE}")

add_custom_command(
    OUTPUT "${RAD_TINFO_RSO}" "${RAD_LIBNCURSES_RSO}" "${RAD_LIBNCURSESW_RSO}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -shared
            -Wl,--hash-style=sysv
            -Wl,--build-id=none
            -Wl,-soname,libtinfo.rso
            -Wl,--no-as-needed
            -o "${RAD_TINFO_RSO}"
            -Wl,--whole-archive "${RAD_TINFO_ARCHIVE}" -Wl,--no-whole-archive
            -L "${CMAKE_BINARY_DIR}"
            -Wl,-l:libradc.rso
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -shared
            -Wl,--hash-style=sysv
            -Wl,--build-id=none
            -Wl,-soname,libncursesw.rso
            -Wl,--no-as-needed
            -o "${RAD_LIBNCURSESW_RSO}"
            -Wl,--whole-archive "${RAD_NCURSESW_ARCHIVE}" -Wl,--no-whole-archive
            -L "${CMAKE_BINARY_DIR}"
            -Wl,-l:libtinfo.rso
            -Wl,-l:libradc.rso
    COMMAND ${CMAKE_COMMAND} -E copy "${RAD_LIBNCURSESW_RSO}" "${RAD_LIBNCURSES_RSO}"
    DEPENDS rad_ncurses "${RAD_NCURSES_STAMP}" "${RAD_NCURSESW_ARCHIVE}" "${RAD_TINFO_ARCHIVE}" "${RAD_LIBC_RSO}"
    VERBATIM
)

add_custom_target(rad_libncurses_rso DEPENDS "${RAD_TINFO_RSO}" "${RAD_LIBNCURSES_RSO}" "${RAD_LIBNCURSESW_RSO}")
add_dependencies(rad_libncurses_rso rad_ncurses)
add_custom_target(rad_userspace_libs DEPENDS "${RAD_LIBC_ARCHIVE}" "${RAD_LIBC_RSO}" rad_ncurses rad_libncurses_rso)

# The build script self-fetches the nano source (tarball) if it is not already
# present, so this always builds -- no configure-time gate. (nano is descoped by
# default; this only runs when terminal_nano is enabled.)
add_custom_target(rad_upstream_nano
    COMMAND ${CMAKE_COMMAND} -E env
        "RAD_SYSROOT_DIR=${RAD_SYSROOT_DIR}"
        "RAD_NCURSES_INCLUDE_DIR=${RAD_NCURSES_INSTALL_DIR}/usr/include"
        "RAD_USERSPACE_LIB_DIR=${CMAKE_BINARY_DIR}/upstream-port-libs"
        "RAD_LIBC_ARCHIVE=${RAD_LIBC_ARCHIVE}"
        "RAD_NCURSESW_ARCHIVE=${RAD_NCURSESW_ARCHIVE}"
        "RAD_TINFO_ARCHIVE=${RAD_TINFO_ARCHIVE}"
        "${CMAKE_CURRENT_LIST_DIR}/ports/upstream/build_upstream_nano.sh"
        "${RAD_UPSTREAM_NANO_SOURCE}"
        "${CMAKE_BINARY_DIR}/upstream-nano"
    DEPENDS rad_userspace_libs rad_sysroot
    VERBATIM
)

# The build script self-fetches the Vim source (git clone at a pinned tag) if it
# is not already present, so this always builds -- no configure-time gate.
add_custom_target(rad_upstream_vim_tiny
    COMMAND ${CMAKE_COMMAND} -E env
        "RAD_SYSROOT_DIR=${RAD_SYSROOT_DIR}"
        "RAD_NCURSES_INCLUDE_DIR=${RAD_NCURSES_INSTALL_DIR}/usr/include"
        "RAD_USERSPACE_LIB_DIR=${CMAKE_BINARY_DIR}/upstream-port-libs"
        "RAD_LIBC_ARCHIVE=${RAD_LIBC_ARCHIVE}"
        "RAD_NCURSESW_ARCHIVE=${RAD_NCURSESW_ARCHIVE}"
        "RAD_TINFO_ARCHIVE=${RAD_TINFO_ARCHIVE}"
        "${CMAKE_CURRENT_LIST_DIR}/ports/upstream/build_upstream_vim_tiny.sh"
        "${RAD_UPSTREAM_VIM_SOURCE}"
        "${CMAKE_BINARY_DIR}/upstream-vim"
    DEPENDS rad_userspace_libs rad_sysroot
    VERBATIM
)

set(RAD_NANO_DEPENDS)
set(RAD_NANO_EXT2_WRITE_COMMANDS)
set(RAD_NANO_EXT4_WRITE_COMMANDS)
if(RAD_RKCONFIG_TERMINAL_NANO_ENABLED)
    list(APPEND RAD_NANO_DEPENDS "${RAD_NANO_TINY_ELF}" "${RAD_NANO_FULL_ELF}" rad_upstream_nano)
    list(APPEND RAD_NANO_EXT2_WRITE_COMMANDS
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_NANO_TINY_ELF} /usr/bin/nano-tiny" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_NANO_FULL_ELF} /usr/bin/nano-full" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_UPSTREAM_NANO_ELF} /usr/bin/nano" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad-nano.meta /etc/rad/packages/rad-nano.radpm" "${RAD_EXT2_ROOTFS_IMAGE}"
    )
    list(APPEND RAD_NANO_EXT4_WRITE_COMMANDS
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_NANO_TINY_ELF} /usr/bin/nano-tiny" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_NANO_FULL_ELF} /usr/bin/nano-full" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_UPSTREAM_NANO_ELF} /usr/bin/nano" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad-nano.meta /etc/rad/packages/rad-nano.radpm" "${RAD_ROOTFS_IMAGE}"
    )
endif()

set(RAD_VIM_DEPENDS)
set(RAD_VIM_EXT2_WRITE_COMMANDS)
set(RAD_VIM_EXT4_WRITE_COMMANDS)
if(RAD_RKCONFIG_TERMINAL_VIM_ENABLED)
    list(APPEND RAD_VIM_DEPENDS rad_upstream_vim_tiny "${CMAKE_CURRENT_LIST_DIR}/ports/vim/runtime/defaults.vim" "${CMAKE_CURRENT_LIST_DIR}/ports/vim/rad-vim-tiny.meta" "${RAD_UPSTREAM_VIM_LICENSE}")
    list(APPEND RAD_VIM_EXT2_WRITE_COMMANDS
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/vim" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/vim/vim92" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/licenses/vim" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_UPSTREAM_VIM_ELF} /usr/bin/vim" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_UPSTREAM_VIM_ELF} /usr/bin/vi" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/vim/runtime/defaults.vim /usr/share/vim/vim92/defaults.vim" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_UPSTREAM_VIM_LICENSE} /usr/share/licenses/vim/LICENSE" "${RAD_EXT2_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/vim/rad-vim-tiny.meta /etc/rad/packages/rad-vim-tiny.radpm" "${RAD_EXT2_ROOTFS_IMAGE}"
    )
    list(APPEND RAD_VIM_EXT4_WRITE_COMMANDS
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/vim" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/vim/vim92" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/licenses/vim" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_UPSTREAM_VIM_ELF} /usr/bin/vim" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_UPSTREAM_VIM_ELF} /usr/bin/vi" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/vim/runtime/defaults.vim /usr/share/vim/vim92/defaults.vim" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_UPSTREAM_VIM_LICENSE} /usr/share/licenses/vim/LICENSE" "${RAD_ROOTFS_IMAGE}"
        COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/vim/rad-vim-tiny.meta /etc/rad/packages/rad-vim-tiny.radpm" "${RAD_ROOTFS_IMAGE}"
    )
endif()

add_custom_command(
    OUTPUT "${RAD_INIT_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_INIT_ELF}"
            "${RAD_USERLAND_INIT_SOURCE_DIR}/init.S"
    DEPENDS "${RAD_USERLAND_INIT_SOURCE_DIR}/init.S"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_RADSH_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -I "${RAD_USERLAND_LOGIN_SOURCE_DIR}"
            -I "${RAD_RKCONFIG_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_RADSH_ELF}"
            "${RAD_USERLAND_RASH_SOURCE_DIR}/radsh_entry.S"
            "${RAD_USERLAND_RASH_SOURCE_DIR}/radsh.c"
            "${RAD_USERLAND_LOGIN_SOURCE_DIR}/auth.c"
    DEPENDS "${RAD_USERLAND_RASH_SOURCE_DIR}/radsh_entry.S" "${RAD_USERLAND_RASH_SOURCE_DIR}/radsh.c" "${RAD_USERLAND_LOGIN_SOURCE_DIR}/auth.c" "${RAD_USERLAND_LOGIN_SOURCE_DIR}/auth.h" "${RAD_RKCONFIG_STAMP}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_LOGIN_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -I "${RAD_USERLAND_LOGIN_SOURCE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_LOGIN_ELF}"
            "${RAD_USERLAND_LOGIN_SOURCE_DIR}/login_entry.S"
            "${RAD_USERLAND_LOGIN_SOURCE_DIR}/login.c"
            "${RAD_USERLAND_LOGIN_SOURCE_DIR}/auth.c"
    DEPENDS "${RAD_USERLAND_LOGIN_SOURCE_DIR}/login_entry.S" "${RAD_USERLAND_LOGIN_SOURCE_DIR}/login.c" "${RAD_USERLAND_LOGIN_SOURCE_DIR}/auth.c" "${RAD_USERLAND_LOGIN_SOURCE_DIR}/auth.h"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_NETWORK_SERVICE_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_NETWORK_SERVICE_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_NETWORK_SERVICE_SOURCE_DIR}/network_service.c"
            "${RAD_LIBC_ARCHIVE}"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_NETWORK_SERVICE_SOURCE_DIR}/network_service.c" "${RAD_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_DNS_RESOLVER_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_DNS_RESOLVER_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_DNS_RESOLVER_SOURCE_DIR}/dns_resolver_service.c"
            "${RAD_LIBC_ARCHIVE}"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_DNS_RESOLVER_SOURCE_DIR}/dns_resolver_service.c" "${RAD_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_TIME_SYNC_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_TIME_SYNC_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_TIME_SYNC_SOURCE_DIR}/time_sync_service.c"
            "${RAD_LIBC_ARCHIVE}"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_TIME_SYNC_SOURCE_DIR}/time_sync_service.c" "${RAD_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_RADINIT_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_RADINIT_ELF}"
            "${RAD_SERVICE_MANAGER_SOURCE_DIR}/radinit_entry.S"
            "${RAD_SERVICE_MANAGER_SOURCE_DIR}/radinit.c"
    DEPENDS "${RAD_SERVICE_MANAGER_SOURCE_DIR}/radinit_entry.S" "${RAD_SERVICE_MANAGER_SOURCE_DIR}/radinit.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_SH_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_SH_ELF}"
            "${RAD_USERLAND_RASH_SOURCE_DIR}/sh.S"
    DEPENDS "${RAD_USERLAND_RASH_SOURCE_DIR}/sh.S"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_RADGFX_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_RADGFX_ELF}"
            "${RAD_USERLAND_RADGFX_SOURCE_DIR}/radgfx.S"
    DEPENDS "${RAD_USERLAND_RADGFX_SOURCE_DIR}/radgfx.S"
    VERBATIM
)

# radwc-demo: a userland C client of the client/server compositor. Renders into a
# shared-memory surface and reacts to routed input. Links the libradcompositor
# client library; freestanding (no libc), uses <rad/syscalls.h> only.
add_custom_command(
    OUTPUT "${RAD_RADWC_DEMO_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -I "${RAD_LIBRADCOMPOSITOR_SOURCE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_RADWC_DEMO_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERLAND_RADWC_DEMO_SOURCE_DIR}/radwc_demo.c"
            "${RAD_LIBRADCOMPOSITOR_SOURCE_DIR}/radcompositor.c"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERLAND_RADWC_DEMO_SOURCE_DIR}/radwc_demo.c"
            "${RAD_LIBRADCOMPOSITOR_SOURCE_DIR}/radcompositor.c"
            "${RAD_LIBRADCOMPOSITOR_SOURCE_DIR}/radcompositor.h"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_TTY_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -DRAD_GUEST_STRESS_DIRECT=1
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_TTY_STRESS_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/tty_stress.c"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_USERSPACE_TEST_SOURCE_DIR}/tty_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_SLEEP_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -DRAD_GUEST_STRESS_DIRECT=1
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_SLEEP_STRESS_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/sleep_stress.c"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_USERSPACE_TEST_SOURCE_DIR}/sleep_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_SIGNAL_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_SIGNAL_STRESS_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/signal_stress.c"
            "${RAD_LIBC_ARCHIVE}"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_USERSPACE_TEST_SOURCE_DIR}/signal_stress.c" "${RAD_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_POSIX_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_POSIX_STRESS_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/posix_stress.c"
            "${RAD_LIBC_ARCHIVE}"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_USERSPACE_TEST_SOURCE_DIR}/posix_stress.c" "${RAD_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_TUI_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -I "${RAD_NCURSES_INSTALL_DIR}/usr/include"
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_TUI_STRESS_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/tui_stress.c"
            "${RAD_NCURSESW_ARCHIVE}"
            "${RAD_TINFO_ARCHIVE}"
            "${RAD_LIBC_ARCHIVE}"
            "${RAD_LIBC_ARCHIVE}"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_USERSPACE_TEST_SOURCE_DIR}/tui_stress.c" "${RAD_NCURSESW_ARCHIVE}" "${RAD_TINFO_ARCHIVE}" "${RAD_LIBC_ARCHIVE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_TUI_STRESS_DYN_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -shared
            -ffreestanding -fPIC -fno-stack-protector -fcf-protection=none -mno-red-zone -nostdinc
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -I "${RAD_NCURSES_INSTALL_DIR}/usr/include"
            -L "${CMAKE_BINARY_DIR}"
            -Wl,--hash-style=sysv
            -Wl,--build-id=none
            -Wl,-e,_start
            -Wl,--no-as-needed
            -Wl,-rpath,/usr/lib
            -o "${RAD_TUI_STRESS_DYN_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/tui_stress_dyn.c"
            -Wl,-l:libncursesw.rso
            -Wl,-l:libtinfo.rso
            -Wl,-l:libradc.rso
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_USERSPACE_TEST_SOURCE_DIR}/tui_stress_dyn.c" "${RAD_LIBNCURSESW_RSO}" "${RAD_TINFO_RSO}" "${RAD_LIBC_RSO}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_LIBC_RSO_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -shared
            -ffreestanding -fPIC -fno-stack-protector -fcf-protection=none -mno-red-zone -nostdinc
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -L "${CMAKE_BINARY_DIR}"
            -Wl,--hash-style=sysv
            -Wl,--build-id=none
            -Wl,-e,_start
            -Wl,--no-as-needed
            -Wl,-rpath,/usr/lib
            -o "${RAD_LIBC_RSO_STRESS_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/libc_rso_stress.c"
            -Wl,-l:libradc.rso
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_USERSPACE_TEST_SOURCE_DIR}/libc_rso_stress.c" "${RAD_LIBC_RSO}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_TINFO_RSO_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -shared
            -ffreestanding -fPIC -fno-stack-protector -fcf-protection=none -mno-red-zone -nostdinc
            -I "${RAD_LIBRADC_INCLUDE_DIR}"
            -I "${RAD_NCURSES_INSTALL_DIR}/usr/include"
            -L "${CMAKE_BINARY_DIR}"
            -Wl,--hash-style=sysv
            -Wl,--build-id=none
            -Wl,-e,_start
            -Wl,--no-as-needed
            -Wl,-rpath,/usr/lib
            -o "${RAD_TINFO_RSO_STRESS_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/tinfo_rso_stress.c"
            -Wl,-l:libtinfo.rso
            -Wl,-l:libradc.rso
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_USERSPACE_TEST_SOURCE_DIR}/tinfo_rso_stress.c" "${RAD_TINFO_RSO}" "${RAD_LIBC_RSO}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_TERMINAL_STRESS_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_TERMINAL_STRESS_ELF}"
            "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S"
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/terminal_stress.c"
    DEPENDS "${RAD_USERLAND_RUNTIME_SOURCE_DIR}/c_entry.S" "${RAD_USERSPACE_TEST_SOURCE_DIR}/terminal_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_HOST_TTY_STRESS}"
    COMMAND "${CMAKE_C_COMPILER}" -Wall -Wextra -O2 -D_DEFAULT_SOURCE
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/tty_stress.c"
            -o "${RAD_HOST_TTY_STRESS}"
    DEPENDS "${RAD_USERSPACE_TEST_SOURCE_DIR}/tty_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_HOST_SLEEP_STRESS}"
    COMMAND "${CMAKE_C_COMPILER}" -Wall -Wextra -O2
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/sleep_stress.c"
            -o "${RAD_HOST_SLEEP_STRESS}"
    DEPENDS "${RAD_USERSPACE_TEST_SOURCE_DIR}/sleep_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_HOST_SIGNAL_STRESS}"
    COMMAND "${CMAKE_C_COMPILER}" -Wall -Wextra -O2
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/signal_stress.c"
            -o "${RAD_HOST_SIGNAL_STRESS}"
    DEPENDS "${RAD_USERSPACE_TEST_SOURCE_DIR}/signal_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_HOST_POSIX_STRESS}"
    COMMAND "${CMAKE_C_COMPILER}" -Wall -Wextra -O2 -D_DEFAULT_SOURCE -D_GNU_SOURCE
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/posix_stress.c"
            -o "${RAD_HOST_POSIX_STRESS}"
    DEPENDS "${RAD_USERSPACE_TEST_SOURCE_DIR}/posix_stress.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_HOST_TUI_STRESS}"
    COMMAND "${CMAKE_C_COMPILER}" -Wall -Wextra -O2 -D_DEFAULT_SOURCE
            "${RAD_USERSPACE_TEST_SOURCE_DIR}/tui_stress.c"
            -o "${RAD_HOST_TUI_STRESS}"
            -lncursesw
    DEPENDS "${RAD_USERSPACE_TEST_SOURCE_DIR}/tui_stress.c"
    VERBATIM
)

add_custom_target(rad_host_tui_stress DEPENDS "${RAD_HOST_TUI_STRESS}")
add_custom_target(rad_host_terminal_stress
    DEPENDS "${RAD_HOST_TTY_STRESS}" "${RAD_HOST_SLEEP_STRESS}" "${RAD_HOST_SIGNAL_STRESS}" "${RAD_HOST_POSIX_STRESS}" "${RAD_HOST_TUI_STRESS}"
)
add_custom_target(rad_host_terminal_stress_run
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/run_host_terminal_stress.sh"
            "${RAD_HOST_TTY_STRESS}"
            "${RAD_HOST_SLEEP_STRESS}"
            "${RAD_HOST_SIGNAL_STRESS}"
            "${RAD_HOST_POSIX_STRESS}"
            "${RAD_HOST_TUI_STRESS}"
            "${CMAKE_BINARY_DIR}/host-terminal-stress.log"
    DEPENDS rad_host_terminal_stress "${CMAKE_CURRENT_LIST_DIR}/run_host_terminal_stress.sh"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_NANO_TINY_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -DRAD_NANO_FULL=0
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_NANO_TINY_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad_nano_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad_nano.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad_nano_entry.S" "${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad_nano.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_NANO_FULL_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -DRAD_NANO_FULL=1
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RAD_NANO_FULL_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad_nano_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad_nano.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad_nano_entry.S" "${CMAKE_CURRENT_LIST_DIR}/ports/nano/rad_nano.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RAD_ROOTFS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E touch "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND truncate -s 32M "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${MKFS_EXT2_EXECUTABLE}" -q -F "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /bin" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /sbin" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /dev" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad/services" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad/network" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad/time" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad/packages" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /home" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /home/root" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /mnt" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /mnt/fat" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/bin" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/include" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/lib" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/licenses" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/zoneinfo" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/zoneinfo/America" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/terminfo" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/terminfo/r" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib/rad" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib/rad/modules" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /tmp" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log/rad" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /run" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADINIT_ELF} /bin/init" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADINIT_ELF} /sbin/radinit" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_NETWORK_SERVICE_ELF} /sbin/network-service" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_DNS_RESOLVER_ELF} /sbin/dns-resolver" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TIME_SYNC_ELF} /sbin/time-sync" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADSH_ELF} /bin/radsh" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADSH_ELF} /bin/rash" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADSH_ELF} /bin/sh" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADSH_ELF} /bin/tui-smoke" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LOGIN_ELF} /bin/login" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADGFX_ELF} /bin/radgfx-smoke" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADWC_DEMO_ELF} /bin/radwc-demo" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TTY_STRESS_ELF} /usr/bin/tty-stress" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_SLEEP_STRESS_ELF} /usr/bin/sleep-stress" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_SIGNAL_STRESS_ELF} /usr/bin/signal-stress" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_POSIX_STRESS_ELF} /usr/bin/posix-stress" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TUI_STRESS_ELF} /usr/bin/tui-stress" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TUI_STRESS_DYN_ELF} /usr/bin/tui-stress-dyn" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBC_RSO_STRESS_ELF} /usr/bin/libc-rso-stress" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TINFO_RSO_STRESS_ELF} /usr/bin/tinfo-rso-stress" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TERMINAL_STRESS_ELF} /usr/bin/terminal-stress" "${RAD_EXT2_ROOTFS_IMAGE}"
    ${RAD_NANO_EXT2_WRITE_COMMANDS}
    ${RAD_VIM_EXT2_WRITE_COMMANDS}
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/rkconfig /etc/rad/rkconfig" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/terminal.theme /etc/rad/terminal.theme" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/radpm-index.json /etc/rad/radpm-index.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/issue /etc/issue" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/hostname /etc/hostname" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/hosts /etc/hosts" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/resolv.conf /etc/resolv.conf" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/network/netinterfaces.json /etc/rad/network/netinterfaces.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/network/dns-resolver.json /etc/rad/network/dns-resolver.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/time/time-sync.json /etc/rad/time/time-sync.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/time/timezone /etc/timezone" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/time/localtime /etc/localtime" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/zoneinfo/${RAD_RKCONFIG_TIMEZONE} /usr/share/zoneinfo/${RAD_RKCONFIG_TIMEZONE}" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/passwd /etc/passwd" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/passwords /etc/passwords" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/rko-note.txt /lib/rad/modules/README.rko" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/rso-note.txt /lib/README.rso" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBC_ARCHIVE} /usr/lib/libradc.a" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBC_RSO} /usr/lib/libradc.rso" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_NCURSESW_ARCHIVE} /usr/lib/libncursesw.a" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TINFO_ARCHIVE} /usr/lib/libtinfo.a" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TINFO_RSO} /usr/lib/libtinfo.rso" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBNCURSES_RSO} /usr/lib/libncurses.rso" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBNCURSESW_RSO} /usr/lib/libncursesw.rso" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBRADC_INCLUDE_DIR}/curses.h /usr/include/curses.h" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBRADC_INCLUDE_DIR}/ncurses.h /usr/include/ncurses.h" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/ncurses/rad-ncurses.meta /etc/rad/packages/rad-ncurses.radpm" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/rad-network.meta /etc/rad/packages/rad-network.radpm" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/rad-dns-resolver.meta /etc/rad/packages/rad-dns-resolver.radpm" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/rad-time-sync.meta /etc/rad/packages/rad-time-sync.radpm" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/rad-tzdata.meta /etc/rad/packages/rad-tzdata.radpm" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TERMINFO_DIR}/r/rad /usr/share/terminfo/r/rad" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/test_script.sh /bin/test.sh" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json /etc/rad/services/storage-root.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network.json /etc/rad/services/network.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/dns-resolver.json /etc/rad/services/dns-resolver.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/time-sync.json /etc/rad/services/time-sync.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_GENERATED_SERVICES_DIR}/userspace-shell.json /etc/rad/services/userspace-shell.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json /etc/rad/services/base-terminal.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_GENERATED_SERVICES_DIR}/terminal-stress.json /etc/rad/services/terminal-stress.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_GENERATED_SERVICES_DIR}/boot-session.json /etc/rad/services/boot-session.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json /etc/rad/services/network-smoke.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json /etc/rad/services/fatfs.json" "${RAD_EXT2_ROOTFS_IMAGE}"
    DEPENDS "${RAD_RADINIT_ELF}" "${RAD_INIT_ELF}" "${RAD_RADSH_ELF}" "${RAD_LOGIN_ELF}" "${RAD_NETWORK_SERVICE_ELF}" "${RAD_DNS_RESOLVER_ELF}" "${RAD_TIME_SYNC_ELF}" "${RAD_SH_ELF}" "${RAD_RADGFX_ELF}" "${RAD_RADWC_DEMO_ELF}" "${RAD_TTY_STRESS_ELF}" "${RAD_SLEEP_STRESS_ELF}" "${RAD_SIGNAL_STRESS_ELF}" "${RAD_POSIX_STRESS_ELF}" "${RAD_TUI_STRESS_ELF}" "${RAD_TUI_STRESS_DYN_ELF}" "${RAD_LIBC_RSO_STRESS_ELF}" "${RAD_TINFO_RSO_STRESS_ELF}" "${RAD_TERMINAL_STRESS_ELF}" ${RAD_NANO_DEPENDS} ${RAD_VIM_DEPENDS} "${RAD_LIBC_ARCHIVE}" "${RAD_LIBC_RSO}" "${RAD_NCURSESW_ARCHIVE}" "${RAD_TINFO_ARCHIVE}" "${RAD_TINFO_RSO}" "${RAD_LIBNCURSES_RSO}" "${RAD_LIBNCURSESW_RSO}" "${RAD_RKCONFIG_STAMP}" "${RAD_TERMINFO_STAMP}" "${RAD_SYSROOT_STAMP}" "${CMAKE_CURRENT_LIST_DIR}/test_script.sh"
            "${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json" "${RAD_GENERATED_SERVICES_DIR}/userspace-shell.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/network.json" "${CMAKE_CURRENT_LIST_DIR}/services/dns-resolver.json" "${CMAKE_CURRENT_LIST_DIR}/services/time-sync.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json" "${RAD_GENERATED_SERVICES_DIR}/terminal-stress.json" "${RAD_GENERATED_SERVICES_DIR}/boot-session.json" "${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RAD_ROOTFS_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${RAD_ROOTFS_IMAGE}" "${RAD_EXT4_WRITE_SEED}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RAD_ROOTFS_DIR}"
    COMMAND /bin/sh -c "printf 'RADPx-OS ext4 write path seed padding for in-place write test\\n' > '${RAD_EXT4_WRITE_SEED}'"
    COMMAND ${CMAKE_COMMAND} -E touch "${RAD_ROOTFS_IMAGE}"
    COMMAND truncate -s "${RAD_RKCONFIG_ROOTFS_SIZE_MB}M" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${MKFS_EXT4_EXECUTABLE}" -q -F -O extent,^has_journal,^64bit,^metadata_csum "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /bin" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /sbin" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /dev" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad/services" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad/network" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad/time" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/rad/packages" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /home" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /home/root" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /mnt" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /mnt/fat" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/bin" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/include" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/lib" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/licenses" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/zoneinfo" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/zoneinfo/America" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/terminfo" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /usr/share/terminfo/r" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib/rad" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /lib/rad/modules" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /tmp" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log/rad" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /run" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADINIT_ELF} /bin/init" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADINIT_ELF} /sbin/radinit" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_NETWORK_SERVICE_ELF} /sbin/network-service" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_DNS_RESOLVER_ELF} /sbin/dns-resolver" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TIME_SYNC_ELF} /sbin/time-sync" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADSH_ELF} /bin/radsh" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADSH_ELF} /bin/rash" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADSH_ELF} /bin/sh" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADSH_ELF} /bin/tui-smoke" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LOGIN_ELF} /bin/login" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADGFX_ELF} /bin/radgfx-smoke" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RADWC_DEMO_ELF} /bin/radwc-demo" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TTY_STRESS_ELF} /usr/bin/tty-stress" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_SLEEP_STRESS_ELF} /usr/bin/sleep-stress" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_SIGNAL_STRESS_ELF} /usr/bin/signal-stress" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_POSIX_STRESS_ELF} /usr/bin/posix-stress" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TUI_STRESS_ELF} /usr/bin/tui-stress" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TUI_STRESS_DYN_ELF} /usr/bin/tui-stress-dyn" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBC_RSO_STRESS_ELF} /usr/bin/libc-rso-stress" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TINFO_RSO_STRESS_ELF} /usr/bin/tinfo-rso-stress" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TERMINAL_STRESS_ELF} /usr/bin/terminal-stress" "${RAD_ROOTFS_IMAGE}"
    ${RAD_NANO_EXT4_WRITE_COMMANDS}
    ${RAD_VIM_EXT4_WRITE_COMMANDS}
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/rkconfig /etc/rad/rkconfig" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/terminal.theme /etc/rad/terminal.theme" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/radpm-index.json /etc/rad/radpm-index.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/issue /etc/issue" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/hostname /etc/hostname" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/hosts /etc/hosts" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/resolv.conf /etc/resolv.conf" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/network/netinterfaces.json /etc/rad/network/netinterfaces.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/network/dns-resolver.json /etc/rad/network/dns-resolver.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/time/time-sync.json /etc/rad/time/time-sync.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/time/timezone /etc/timezone" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/time/localtime /etc/localtime" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/zoneinfo/${RAD_RKCONFIG_TIMEZONE} /usr/share/zoneinfo/${RAD_RKCONFIG_TIMEZONE}" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/passwd /etc/passwd" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/passwords /etc/passwords" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/rko-note.txt /lib/rad/modules/README.rko" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_RKCONFIG_DIR}/rso-note.txt /lib/README.rso" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBC_ARCHIVE} /usr/lib/libradc.a" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBC_RSO} /usr/lib/libradc.rso" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_NCURSESW_ARCHIVE} /usr/lib/libncursesw.a" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TINFO_ARCHIVE} /usr/lib/libtinfo.a" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TINFO_RSO} /usr/lib/libtinfo.rso" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBNCURSES_RSO} /usr/lib/libncurses.rso" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBNCURSESW_RSO} /usr/lib/libncursesw.rso" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBRADC_INCLUDE_DIR}/curses.h /usr/include/curses.h" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_LIBRADC_INCLUDE_DIR}/ncurses.h /usr/include/ncurses.h" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/ports/ncurses/rad-ncurses.meta /etc/rad/packages/rad-ncurses.radpm" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/rad-network.meta /etc/rad/packages/rad-network.radpm" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/rad-dns-resolver.meta /etc/rad/packages/rad-dns-resolver.radpm" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/rad-time-sync.meta /etc/rad/packages/rad-time-sync.radpm" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/rad-tzdata.meta /etc/rad/packages/rad-tzdata.radpm" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_TERMINFO_DIR}/r/rad /usr/share/terminfo/r/rad" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/test_script.sh /bin/test.sh" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json /etc/rad/services/storage-root.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network.json /etc/rad/services/network.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/dns-resolver.json /etc/rad/services/dns-resolver.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/time-sync.json /etc/rad/services/time-sync.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_GENERATED_SERVICES_DIR}/userspace-shell.json /etc/rad/services/userspace-shell.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json /etc/rad/services/base-terminal.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_GENERATED_SERVICES_DIR}/terminal-stress.json /etc/rad/services/terminal-stress.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_GENERATED_SERVICES_DIR}/boot-session.json /etc/rad/services/boot-session.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json /etc/rad/services/network-smoke.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json /etc/rad/services/fatfs.json" "${RAD_ROOTFS_IMAGE}"
    COMMAND "${PYTHON3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/install_rootfs_tree.py" "${RAD_ROOTFS_IMAGE}" "${RAD_RADPM_ROOTFS_DIR}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RAD_EXT4_WRITE_SEED} /tmp/radwrite.txt" "${RAD_ROOTFS_IMAGE}"
    DEPENDS "${RAD_RADINIT_ELF}" "${RAD_INIT_ELF}" "${RAD_RADSH_ELF}" "${RAD_LOGIN_ELF}" "${RAD_NETWORK_SERVICE_ELF}" "${RAD_DNS_RESOLVER_ELF}" "${RAD_TIME_SYNC_ELF}" "${RAD_SH_ELF}" "${RAD_RADGFX_ELF}" "${RAD_RADWC_DEMO_ELF}" "${RAD_TTY_STRESS_ELF}" "${RAD_SLEEP_STRESS_ELF}" "${RAD_SIGNAL_STRESS_ELF}" "${RAD_POSIX_STRESS_ELF}" "${RAD_TUI_STRESS_ELF}" "${RAD_TUI_STRESS_DYN_ELF}" "${RAD_LIBC_RSO_STRESS_ELF}" "${RAD_TINFO_RSO_STRESS_ELF}" "${RAD_TERMINAL_STRESS_ELF}" ${RAD_NANO_DEPENDS} ${RAD_VIM_DEPENDS} "${RAD_LIBC_ARCHIVE}" "${RAD_LIBC_RSO}" "${RAD_NCURSESW_ARCHIVE}" "${RAD_TINFO_ARCHIVE}" "${RAD_TINFO_RSO}" "${RAD_LIBNCURSES_RSO}" "${RAD_LIBNCURSESW_RSO}" "${RAD_RKCONFIG_STAMP}" "${RAD_TERMINFO_STAMP}" "${RAD_SYSROOT_STAMP}" "${CMAKE_CURRENT_LIST_DIR}/test_script.sh"
            "${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json" "${RAD_GENERATED_SERVICES_DIR}/userspace-shell.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/network.json" "${CMAKE_CURRENT_LIST_DIR}/services/dns-resolver.json" "${CMAKE_CURRENT_LIST_DIR}/services/time-sync.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json" "${RAD_GENERATED_SERVICES_DIR}/terminal-stress.json" "${RAD_GENERATED_SERVICES_DIR}/boot-session.json" "${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json" "${CMAKE_CURRENT_LIST_DIR}/install_rootfs_tree.py" ${RAD_RADPM_ROOTFS_DEPENDS}
    VERBATIM
)

add_custom_target(rad_rootfs ALL DEPENDS "${RAD_ROOTFS_IMAGE}" "${RAD_EXT2_ROOTFS_IMAGE}" rad_terminfo rad_sysroot)

add_custom_command(
    OUTPUT "${RAD_FAT_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${RAD_FAT_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E touch "${RAD_FAT_IMAGE}"
    COMMAND truncate -s 64M "${RAD_FAT_IMAGE}"
    COMMAND "${MKFS_FAT_EXECUTABLE}" -F 32 -n RADFAT "${RAD_FAT_IMAGE}"
    VERBATIM
)

add_custom_target(rad_test_volumes ALL DEPENDS "${RAD_FAT_IMAGE}")

add_custom_command(
    OUTPUT "${ISO_PATH}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${ISO_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_ROOT}/boot/grub"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:radkernel_x86_64_grub_slint>" "${ISO_ROOT}/boot/radkernel-x86-64-grub.elf"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/grub.cfg" "${ISO_ROOT}/boot/grub/grub.cfg"
    COMMAND "${GRUB_MKRESCUE_EXECUTABLE}" -o "${ISO_PATH}" "${ISO_ROOT}"
    DEPENDS radkernel_x86_64_grub_slint "${CMAKE_CURRENT_LIST_DIR}/grub.cfg"
    VERBATIM
)

add_custom_target(radkernel_x86_64_grub_slint_iso ALL DEPENDS "${ISO_PATH}" rad_rootfs rad_test_volumes)
