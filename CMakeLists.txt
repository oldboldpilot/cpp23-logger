cmake_minimum_required(VERSION 3.28)

# ============================================================================
# Toolchain Configuration (before project())
# ============================================================================
# Configurable toolchain paths with fallback detection
# Can be overridden via: cmake -DCLANG_DEFAULT_PATH=/custom/path ...
# Or via environment: CC=/custom/clang CXX=/custom/clang++ cmake ...
# These variables are typically inherited from parent project when using FetchContent

# Default toolchain paths (can be overridden)
set(CLANG_DEFAULT_PATH "/usr/local/bin" CACHE STRING "Default path for Clang binaries")
set(LIBCXX_DEFAULT_INCLUDE "/usr/local/include/c++/v1" CACHE STRING "Default path for libc++ headers")
set(LIBCXX_DEFAULT_LIB "/usr/local/lib" CACHE STRING "Default path for libc++ libraries")

project(cpp23-logger
    VERSION 1.0.0
    DESCRIPTION "Production-grade C++23 logger with Mustache templates and fluent API"
    LANGUAGES CXX
)

# ============================================================================
# Project Options
# ============================================================================

option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks" OFF)
option(ENABLE_NATIVE_OPTIMIZATIONS "Enable native CPU optimizations (AVX-512, AVX2, etc) to match parent project" OFF)
option(ENABLE_OPENMP "Enable OpenMP parallel processing support" OFF)

# ============================================================================
# Configurable Library Settings (for embedding in parent projects)
# ============================================================================
# These options allow parent projects to customize the logger library to:
# 1. Prevent target name clashes when multiple projects use cpp23-logger
# 2. Control static vs shared library linking
# 3. Specify output directory for library files

# Target name prefix (e.g., "sgee_" -> target becomes "sgee_logger")
set(CPP23_LOGGER_TARGET_PREFIX "" CACHE STRING "Prefix for logger target name (e.g., 'sgee_' -> 'sgee_logger')")

# Shared vs static library build
option(CPP23_LOGGER_BUILD_SHARED "Build logger as shared library (default: follows BUILD_SHARED_LIBS)" OFF)

# Custom output directory for library files
set(CPP23_LOGGER_OUTPUT_DIR "" CACHE PATH "Custom output directory for logger library files")

# Derive the actual target name
set(CPP23_LOGGER_TARGET_NAME "${CPP23_LOGGER_TARGET_PREFIX}logger" CACHE STRING "Final logger target name" FORCE)

# ============================================================================
# C++23 Standard Requirement
# ============================================================================

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Compiler Detection and Configuration
# ============================================================================

# Check for C++23 modules support
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "17.0")
        message(FATAL_ERROR "Clang 17.0 or higher required for C++23 modules support")
    endif()
    # Clang C++23 modules flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23 -stdlib=libc++")

    # Optional: Add native CPU optimizations if requested by parent project
    # NOTE: -march=native automatically detects and uses only CPU-supported instructions
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        message(STATUS "Enabling native CPU optimizations for module compatibility")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR "GCC 14.0 or higher required for C++23 modules support")
    endif()
    # GCC C++23 modules flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23 -fmodules-ts")

    # Optional: Add native CPU optimizations if requested by parent project
    # NOTE: -march=native automatically detects and uses only CPU-supported instructions
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        message(STATUS "Enabling native CPU optimizations for module compatibility")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.34")
        message(FATAL_ERROR "MSVC 19.34 (VS 2022 17.4) or higher required for C++23 modules")
    endif()
    # MSVC C++23 modules flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest /experimental:module")

else()
    message(FATAL_ERROR "Unsupported compiler. Requires Clang 17+, GCC 14+, or MSVC 19.34+")
endif()

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: C++23")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ============================================================================
# Clang-Tidy Configuration (Optional)
# ============================================================================
# Configurable clang-tidy search paths
set(CLANG_TIDY_SEARCH_PATHS "${CLANG_DEFAULT_PATH}" "/usr/lib/llvm-21/bin" "/usr/lib/llvm-19/bin" CACHE STRING "Search paths for clang-tidy")

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy-21 clang-tidy
        PATHS ${CLANG_TIDY_SEARCH_PATHS}
        DOC "Path to clang-tidy executable"
    )
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy requested but not found")
    endif()
endif()

# ============================================================================
# Auto-Build Standard Library Module PCM (std.pcm)
# ============================================================================
# Build std.pcm with the SAME flags as the logger library to ensure binary
# compatibility. This PCM can be used by any code that links against the logger.

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Configurable path to libc++ std module source
    # Try multiple common locations, can be overridden via cmake -DLIBCXX_MODULES_PATH=...
    if(NOT LIBCXX_MODULES_PATH)
        if(EXISTS "${LIBCXX_DEFAULT_INCLUDE}")
            set(LIBCXX_MODULES_PATH "${LIBCXX_DEFAULT_INCLUDE}" CACHE STRING "Path to libc++ module sources")
        elseif(EXISTS "/usr/local/share/libc++/v1")
            set(LIBCXX_MODULES_PATH "/usr/local/share/libc++/v1" CACHE STRING "Path to libc++ module sources")
        elseif(EXISTS "/usr/local/include/c++/v1")
            set(LIBCXX_MODULES_PATH "/usr/local/include/c++/v1" CACHE STRING "Path to libc++ module sources")
        else()
            message(FATAL_ERROR "Could not find libc++ module sources. Please set LIBCXX_MODULES_PATH")
        endif()
    endif()

    set(STD_CPPM "${LIBCXX_MODULES_PATH}/std.cppm")
    set(STD_COMPAT_CPPM "${LIBCXX_MODULES_PATH}/std.compat.cppm")

    if(NOT EXISTS "${STD_CPPM}")
        message(FATAL_ERROR "std.cppm not found at ${STD_CPPM}. Please set LIBCXX_MODULES_PATH correctly.")
    endif()

    # Output PCM files in modules/ subdirectory
    set(STD_PCM "${CMAKE_CURRENT_BINARY_DIR}/modules/std.pcm")
    set(STD_COMPAT_PCM "${CMAKE_CURRENT_BINARY_DIR}/modules/std.compat.pcm")

    # Create modules directory
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/modules")

    # Collect all compiler flags that will be used for the logger
    set(STD_PCM_FLAGS
        -std=c++23
        -stdlib=libc++
        -fPIC
    )

    # Add optimization flags if building in Release mode
    if(CMAKE_BUILD_TYPE MATCHES "Release")
        list(APPEND STD_PCM_FLAGS -O3)
    endif()

    # Add native optimizations if enabled (must match logger compilation)
    # NOTE: -ffast-math is excluded to maintain binary compatibility with logger.pcm
    # NOTE: -march=native automatically detects and uses only CPU-supported instructions
    if(ENABLE_NATIVE_OPTIMIZATIONS)
        list(APPEND STD_PCM_FLAGS -march=native)
    endif()

    # Add OpenMP flags if enabled (must match logger compilation)
    if(ENABLE_OPENMP)
        find_package(OpenMP QUIET)
        if(OpenMP_CXX_FOUND)
            list(APPEND STD_PCM_FLAGS -fopenmp=libomp)
        endif()
    endif()

    # Build std.pcm
    add_custom_command(
        OUTPUT ${STD_PCM}
        COMMAND ${CMAKE_CXX_COMPILER} ${STD_PCM_FLAGS}
                --precompile ${STD_CPPM} -o ${STD_PCM}
        DEPENDS ${STD_CPPM}
        COMMENT "Precompiling libc++ std module for logger (std.pcm)"
        VERBATIM
    )

    # Build std.compat.pcm (depends on std.pcm)
    add_custom_command(
        OUTPUT ${STD_COMPAT_PCM}
        COMMAND ${CMAKE_CXX_COMPILER} ${STD_PCM_FLAGS}
                -fprebuilt-module-path=${CMAKE_CURRENT_BINARY_DIR}/modules
                --precompile ${STD_COMPAT_CPPM} -o ${STD_COMPAT_PCM}
        DEPENDS ${STD_COMPAT_CPPM} ${STD_PCM}
        COMMENT "Precompiling libc++ std.compat module for logger (std.compat.pcm)"
        VERBATIM
    )

    # Custom target to build both PCM files
    add_custom_target(build_logger_std_modules
        DEPENDS ${STD_PCM} ${STD_COMPAT_PCM}
        COMMENT "Building standard library modules for logger"
    )

    message(STATUS "std module PCM auto-build enabled:")
    message(STATUS "  Module source: ${LIBCXX_MODULES_PATH}")
    message(STATUS "  Output: ${CMAKE_CURRENT_BINARY_DIR}/modules/")
    message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER}")
    message(STATUS "  Flags: ${STD_PCM_FLAGS}")
    message(STATUS "  OpenMP: ${ENABLE_OPENMP}")
    message(STATUS "  Native opts: ${ENABLE_NATIVE_OPTIMIZATIONS}")

    # Export variables for external projects (parent CMakeLists.txt)
    # These allow external code to use the same PCM with binary compatibility
    set(LOGGER_STD_PCM "${STD_PCM}" CACHE PATH "Path to logger's precompiled std.pcm" FORCE)
    set(LOGGER_STD_COMPAT_PCM "${STD_COMPAT_PCM}" CACHE PATH "Path to logger's precompiled std.compat.pcm" FORCE)
    set(LOGGER_STD_PCM_FLAGS "${STD_PCM_FLAGS}" CACHE STRING "Compiler flags used for logger's std.pcm" FORCE)
    set(LOGGER_STD_PCM_DIR "${CMAKE_CURRENT_BINARY_DIR}/modules" CACHE PATH "Directory containing logger's PCM files" FORCE)

    # Mark these as advanced so they don't clutter the CMake GUI
    mark_as_advanced(LOGGER_STD_PCM LOGGER_STD_COMPAT_PCM LOGGER_STD_PCM_FLAGS LOGGER_STD_PCM_DIR)
endif()

# ============================================================================
# Logger Module Library
# ============================================================================

# Determine library type based on configuration
if(CPP23_LOGGER_BUILD_SHARED)
    add_library(${CPP23_LOGGER_TARGET_NAME} SHARED)
    message(STATUS "Building ${CPP23_LOGGER_TARGET_NAME} as SHARED library")
else()
    add_library(${CPP23_LOGGER_TARGET_NAME} STATIC)
    message(STATUS "Building ${CPP23_LOGGER_TARGET_NAME} as STATIC library")
endif()

# Create an alias for the original 'logger' name if using a prefix
# This allows code to still use 'logger' target when linked
if(CPP23_LOGGER_TARGET_PREFIX)
    add_library(logger ALIAS ${CPP23_LOGGER_TARGET_NAME})
    message(STATUS "Created alias: logger -> ${CPP23_LOGGER_TARGET_NAME}")
endif()

target_sources(${CPP23_LOGGER_TARGET_NAME}
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            include/logger/logger.cppm
            include/logger/code_generator.cppm
)

target_include_directories(${CPP23_LOGGER_TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Thread support required for std::mutex
find_package(Threads REQUIRED)
target_link_libraries(${CPP23_LOGGER_TARGET_NAME} PUBLIC Threads::Threads)

# Match parent project BMI flags for std.pcm compatibility
if(DEFINED UNIVERSAL_MODULE_FLAGS)
    target_compile_options(${CPP23_LOGGER_TARGET_NAME} PRIVATE ${UNIVERSAL_MODULE_FLAGS})
    target_include_directories(${CPP23_LOGGER_TARGET_NAME} SYSTEM PRIVATE ${UNIVERSAL_MODULE_INCLUDES})
else()
    target_compile_options(${CPP23_LOGGER_TARGET_NAME} PRIVATE
        -nostdinc++
        -pthread
        -fopenmp=libomp
        -fstack-protector-strong
        -fno-omit-frame-pointer
        -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2
    )
    target_include_directories(${CPP23_LOGGER_TARGET_NAME} SYSTEM PRIVATE
        /usr/local/lib/clang/21/include
        /usr/local/include/c++/v1
    )
endif()

# Optional OpenMP support for parallel processing
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "Enabling OpenMP support for parallel processing")
        target_compile_options(${CPP23_LOGGER_TARGET_NAME} PRIVATE ${OpenMP_CXX_FLAGS})
        target_link_libraries(${CPP23_LOGGER_TARGET_NAME} PUBLIC OpenMP::OpenMP_CXX)
    else()
        message(WARNING "OpenMP requested but not found")
    endif()
endif()

# Determine output directory
if(CPP23_LOGGER_OUTPUT_DIR)
    set(_LOGGER_OUTPUT_DIR "${CPP23_LOGGER_OUTPUT_DIR}")
else()
    set(_LOGGER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib")
endif()

# Set module output directory and enable position-independent code for shared library linking
set_target_properties(${CPP23_LOGGER_TARGET_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY "${_LOGGER_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${_LOGGER_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${_LOGGER_OUTPUT_DIR}"
)

# Export the target name for parent projects
set(CPP23_LOGGER_LIBRARY_TARGET "${CPP23_LOGGER_TARGET_NAME}" CACHE STRING "Logger library target name" FORCE)

# ============================================================================
# Tests
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Examples
# ============================================================================

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library and module files
# Disabled for submodule usage
# install(TARGETS ${CPP23_LOGGER_TARGET_NAME}
#     EXPORT loggerTargets
#     FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/logger
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Generate and install package config files
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/loggerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/loggerConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logger
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/loggerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# install(FILES
#     "${CMAKE_CURRENT_BINARY_DIR}/loggerConfig.cmake"
#     "${CMAKE_CURRENT_BINARY_DIR}/loggerConfigVersion.cmake"
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logger
# )

# install(EXPORT loggerTargets
#     FILE loggerTargets.cmake
#     NAMESPACE logger::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logger
# )

# ============================================================================
# Package Information
# ============================================================================

message(STATUS "")
message(STATUS "cpp23-logger Configuration:")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  Target name:       ${CPP23_LOGGER_TARGET_NAME}")
message(STATUS "  Library type:      ${CPP23_LOGGER_BUILD_SHARED}")
message(STATUS "  Output directory:  ${_LOGGER_OUTPUT_DIR}")
message(STATUS "  Build tests:       ${BUILD_TESTS}")
message(STATUS "  Build examples:    ${BUILD_EXAMPLES}")
message(STATUS "  Enable clang-tidy: ${ENABLE_CLANG_TIDY}")
message(STATUS "  Clang path:        ${CLANG_DEFAULT_PATH}")
message(STATUS "  libc++ include:    ${LIBCXX_DEFAULT_INCLUDE}")
message(STATUS "  libc++ lib:        ${LIBCXX_DEFAULT_LIB}")
message(STATUS "")
