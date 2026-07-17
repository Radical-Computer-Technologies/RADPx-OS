add_executable(RADixKernelTests
    radix_kernel_tests.cpp
)

target_compile_features(RADixKernelTests PRIVATE cxx_std_20)
target_link_libraries(RADixKernelTests PRIVATE RADixKernel)

# Run from the source root: the suite invokes repo-relative helpers (e.g.
# python3 tools/radoverlay_compile.py). Without this, ctest runs from the build
# tree and those relative paths fail (they resolve only when the binary is run
# from the repo root).
add_test(NAME RADixKernelTests COMMAND RADixKernelTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
