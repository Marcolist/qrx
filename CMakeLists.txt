cmake_minimum_required(VERSION 3.16)

# Root wrapper. The actual QRX Core project lives in qrx-core/.
# This allows both:
#   cmake -S . -B build
# and:
#   cd qrx-core && cmake -S . -B build
project(qrx_root C)

# Genesis hardening (Finding 8): without enable_testing() at the root, a root
# build reported "No tests were found" and ctest exited 0, so CI could report
# green while the whole Core suite never ran. Tests are registered in the
# qrx-core subdirectory; enable_testing() must be called here so ctest sees
# them from the root build tree as well.
option(QRX_BUILD_TESTS "Build and register the QRX Core test suite" OFF)
if(QRX_BUILD_TESTS)
    enable_testing()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qrx-core/CMakeLists.txt")
    add_subdirectory(qrx-core)
else()
    message(FATAL_ERROR "qrx-core/CMakeLists.txt not found. Please keep the qrx-core directory in the repository root.")
endif()
