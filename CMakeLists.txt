cmake_minimum_required(VERSION 3.15)

project(RADixOS LANGUAGES C CXX)

option(RADIX_OS_BUILD_TESTS "Build RADPx OS host tests" ON)

add_subdirectory(RADixKernel)

if(RADIX_OS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
