cmake_minimum_required(VERSION 3.15)

project(rad_os_shell LANGUAGES CXX)

set(RADPX_OS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH "Path to RADPx-OS")
if(NOT RADLIB_ROOT)
    get_filename_component(RADPX_OS_PARENT "${RADPX_OS_ROOT}/.." ABSOLUTE)
    set(RADLIB_ROOT "${RADPX_OS_PARENT}/RADLib" CACHE PATH "Path to RADLib")
endif()

add_compile_definitions(
    RADLIB_LICENSE_ACCEPTED=1
    RADLIB_LICENSE_TIER_INTERNAL=1
    RADLIB_EDITION_COMMUNITY=1
    RADLIB_EDITION_REQUIRES_LICENSE_KEY=0
)

set(RADLIB_EMBEDDED ON CACHE BOOL "" FORCE)
set(BUILD_RADCore ON CACHE BOOL "" FORCE)
set(BUILD_RADUi ON CACHE BOOL "" FORCE)
set(BUILD_RADKernel ON CACHE BOOL "" FORCE)
set(RAD_KERNEL_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(RAD_KERNEL_BACKEND linux_sim CACHE STRING "" FORCE)
set(RADUI_ENABLE_SLINT ON CACHE BOOL "" FORCE)
set(RADUI_ENABLE_EMBEDDED_FRAMEBUFFER ON CACHE BOOL "" FORCE)
if(NOT RADUI_SLINT_PROVIDER)
    set(RADUI_SLINT_PROVIDER binary CACHE STRING "" FORCE)
endif()
if(RADUI_SLINT_PROVIDER STREQUAL "binary" AND NOT TARGET Slint::Slint)
    if(NOT SLINT_SDK_DIR)
        set(SLINT_SDK_DIR "/media/jvincent/Kingspec512/SDKs/Slint-cpp-1.17.0-Linux-x86_64" CACHE PATH "Path to the Slint C++ SDK")
    endif()
    find_package(Slint CONFIG REQUIRED
        PATHS "${SLINT_SDK_DIR}/lib/cmake/Slint"
        NO_DEFAULT_PATH
    )
endif()

add_subdirectory("${RADPX_OS_ROOT}/RADKernel" "${CMAKE_BINARY_DIR}/RADKernel")
add_subdirectory("${RADLIB_ROOT}/RADCore" "${CMAKE_BINARY_DIR}/RADCore")
add_subdirectory("${RADLIB_ROOT}/RADUi" "${CMAKE_BINARY_DIR}/RADUi")

add_executable(rad_os_shell
    main.cpp
    "${RADPX_OS_ROOT}/services/RADCompositor/RADCompositorModel.cpp"
    "${RADPX_OS_ROOT}/services/RADCompositor/RADCompositorCore.cpp"
)

target_compile_features(rad_os_shell PRIVATE cxx_std_20)
target_include_directories(rad_os_shell PRIVATE
    "${RADPX_OS_ROOT}/RADKernel/boot"
    "${RADPX_OS_ROOT}/services/RADCompositor"
)
target_link_libraries(rad_os_shell PRIVATE RADKernel RADCore RADUi Slint::Slint)

target_compile_definitions(rad_os_shell PRIVATE SLINT_BACKEND=1)

SLINT_TARGET_SOURCES(rad_os_shell
    "${RADPX_OS_ROOT}/services/RADCompositor/RADCompositor.slint"
    "${RADPX_OS_ROOT}/services/RADCompositor/RADDesktop.slint")


if(DEFINED SLINT_SDK_DIR)
    set_target_properties(rad_os_shell PROPERTIES
        BUILD_RPATH "${SLINT_SDK_DIR}/lib;${CMAKE_BINARY_DIR}/lib"
    )
endif()
