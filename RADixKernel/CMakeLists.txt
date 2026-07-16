cmake_minimum_required(VERSION 3.15)

option(RADIX_KERNEL_BUILD_STATIC "Build RADixKernel as a static archive" OFF)
option(RADIX_KERNEL_ENABLE_SD_FAT "Enable RADixKernel SD/FatFs integration points" ON)
option(RADIX_KERNEL_ENABLE_SDIO_PIO "Enable optional RP235x PIO SDIO integration point" OFF)
option(RADIX_KERNEL_ENABLE_PSRAM "Register backend PSRAM regions when available" OFF)
option(RADIX_KERNEL_ENABLE_RP2350_HSTX_DVI "Expose the RP2350 HSTX/DVI framebuffer backend scaffold" OFF)
set(RADIX_KERNEL_BACKEND "linux_sim" CACHE STRING "RADixKernel backend: linux_sim, rp235x_pico, circle_pi, bcm283x_pi, or zynqmp_zuboard_1cg")
set(RADIX_KERNEL_SD_MODE "auto" CACHE STRING "RADixKernel SD mode: auto, spi, or pio_sdio")
set_property(CACHE RADIX_KERNEL_BACKEND PROPERTY STRINGS linux_sim rp235x_pico circle_pi bcm283x_pi zynqmp_zuboard_1cg)
set_property(CACHE RADIX_KERNEL_SD_MODE PROPERTY STRINGS auto spi pio_sdio)

set(RADIX_KERNEL_LIBRARY_TYPE SHARED)
if(RADIX_KERNEL_BUILD_STATIC OR RADLIB_EMBEDDED)
    set(RADIX_KERNEL_LIBRARY_TYPE STATIC)
endif()

set(RADIX_KERNEL_SOURCES
    boot/radboot.cpp
    subsystems/RADixOverlayRuntime.cpp
)

if(RADIX_KERNEL_BACKEND STREQUAL "linux_sim")
    list(APPEND RADIX_KERNEL_SOURCES
        core/RADixKernel.cpp
        platforms/a53/radixkernel_a53.cpp
    )
elseif(RADIX_KERNEL_BACKEND STREQUAL "rp235x_pico")
    set(RADIX_KERNEL_LIBRARY_TYPE STATIC)
    list(APPEND RADIX_KERNEL_SOURCES
        core/RADixKernel_runtime.cpp
        hal/rp235x_pico/radixkernel_hal_rp235x.cpp
    )
elseif(RADIX_KERNEL_BACKEND STREQUAL "circle_pi")
    set(RADIX_KERNEL_LIBRARY_TYPE STATIC)
    list(APPEND RADIX_KERNEL_SOURCES
        core/RADixKernel_runtime.cpp
        hal/circle/radixkernel_hal_circle.cpp
        hal/circle/radixkernel_runtime_newlib.cpp
    )
elseif(RADIX_KERNEL_BACKEND STREQUAL "bcm283x_pi")
    set(RADIX_KERNEL_LIBRARY_TYPE STATIC)
    list(APPEND RADIX_KERNEL_SOURCES
        core/RADixKernel_runtime.cpp
        platforms/a53/radixkernel_a53.cpp
        platforms/a53/bcm283x/radixkernel_hal_bcm283x.cpp
        hal/circle/radixkernel_runtime_newlib.cpp
    )
elseif(RADIX_KERNEL_BACKEND STREQUAL "zynqmp_zuboard_1cg")
    set(RADIX_KERNEL_LIBRARY_TYPE STATIC)
    list(APPEND RADIX_KERNEL_SOURCES
        core/RADixKernel_runtime.cpp
        platforms/a53/radixkernel_a53.cpp
        platforms/a53/zynqmp/radixkernel_hal_zynqmp.cpp
        hal/circle/radixkernel_runtime_newlib.cpp
    )
else()
    message(FATAL_ERROR "Unsupported RADIX_KERNEL_BACKEND '${RADIX_KERNEL_BACKEND}'")
endif()

add_library(RADixKernel ${RADIX_KERNEL_LIBRARY_TYPE}
    ${RADIX_KERNEL_SOURCES}
)

set(RADIX_KERNEL_BUILD_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include")
set(RADIX_KERNEL_BUILD_HEADER_DIR "${RADIX_KERNEL_BUILD_INCLUDE_DIR}/radixkernel")
file(MAKE_DIRECTORY "${RADIX_KERNEL_BUILD_HEADER_DIR}")
file(GLOB RADIX_KERNEL_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/headers/radixkernel/*.h")
foreach(RADIX_KERNEL_HEADER ${RADIX_KERNEL_HEADERS})
    get_filename_component(RADIX_KERNEL_HEADER_NAME "${RADIX_KERNEL_HEADER}" NAME)
    configure_file(
        "${RADIX_KERNEL_HEADER}"
        "${RADIX_KERNEL_BUILD_HEADER_DIR}/${RADIX_KERNEL_HEADER_NAME}"
        COPYONLY
    )
endforeach()
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/boot/radboot.h"
    "${RADIX_KERNEL_BUILD_INCLUDE_DIR}/radboot.h"
    COPYONLY
)

target_compile_features(RADixKernel PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(RADixKernel PRIVATE /O2 /MP /W4)
elseif(RADIX_KERNEL_BACKEND STREQUAL "linux_sim")
    target_compile_options(RADixKernel PRIVATE -O3 -Wall -Wextra -pthread -fPIC)
    target_link_libraries(RADixKernel PUBLIC pthread)
else()
    target_compile_options(RADixKernel PRIVATE -Os -Wall -Wextra -ffunction-sections -fdata-sections)
endif()

target_compile_definitions(RADixKernel
    PUBLIC
        RADIX_KERNEL_BACKEND_${RADIX_KERNEL_BACKEND}=1
        RADIX_KERNEL_ENABLE_SD_FAT=$<BOOL:${RADIX_KERNEL_ENABLE_SD_FAT}>
        RADIX_KERNEL_ENABLE_SDIO_PIO=$<BOOL:${RADIX_KERNEL_ENABLE_SDIO_PIO}>
        RADIX_KERNEL_ENABLE_PSRAM=$<BOOL:${RADIX_KERNEL_ENABLE_PSRAM}>
        RADIX_KERNEL_ENABLE_RP2350_HSTX_DVI=$<BOOL:${RADIX_KERNEL_ENABLE_RP2350_HSTX_DVI}>
        RADIX_KERNEL_SD_MODE="${RADIX_KERNEL_SD_MODE}"
)

if(NOT RADIX_KERNEL_BACKEND STREQUAL "linux_sim")
    target_compile_definitions(RADixKernel PUBLIC RADLIB_EMBEDDED_NO_CPP_WRAPPERS=1)
endif()

target_include_directories(RADixKernel
    PUBLIC
        "$<BUILD_INTERFACE:${RADIX_KERNEL_BUILD_INCLUDE_DIR}>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/headers"
        "${CMAKE_CURRENT_SOURCE_DIR}/boot"
        "${CMAKE_CURRENT_SOURCE_DIR}/platforms/a53"
)

if(RADIX_KERNEL_BACKEND STREQUAL "rp235x_pico" AND DEFINED PICO_SDK_PATH)
    target_include_directories(RADixKernel PRIVATE
        "${PICO_SDK_PATH}/src/common/pico_base_headers/include"
        "${PICO_SDK_PATH}/src/rp2_common/pico_stdio/include"
        "${PICO_SDK_PATH}/src/rp2_common/pico_stdio_uart/include"
        "${PICO_SDK_PATH}/src/rp2_common/pico_multicore/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_timer/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_gpio/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_i2c/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_spi/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_dma/include"
        "${PICO_SDK_PATH}/src/rp2350/hardware_regs/include"
    )
endif()

if(RADIX_KERNEL_BACKEND STREQUAL "rp235x_pico" AND TARGET pico_stdlib)
    target_link_libraries(RADixKernel PUBLIC pico_stdlib pico_multicore hardware_gpio hardware_i2c hardware_spi hardware_dma)
endif()

if(RADIX_KERNEL_BACKEND STREQUAL "circle_pi" AND DEFINED RADLIB_CIRCLE_ROOT)
    target_include_directories(RADixKernel PRIVATE
        "${RADLIB_CIRCLE_ROOT}/include"
    )
endif()
