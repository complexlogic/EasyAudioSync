message("Setting up vcpkg...")
include(FetchContent)
FetchContent_Declare(
  vcpkg
  GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
  GIT_SHALLOW TRUE
  SOURCE_DIR ${PROJECT_BINARY_DIR}
)
FetchContent_Declare(
  vcpkg_overlay
  GIT_REPOSITORY https://github.com/complexlogic/vcpkg.git
  GIT_TAG origin/easyaudiosync
  GIT_SHALLOW TRUE
  SOURCE_DIR ${PROJECT_BINARY_DIR}
)
FetchContent_MakeAvailable(vcpkg vcpkg_overlay)
set(VCPKG_OVERLAY_PORTS "${CMAKE_BINARY_DIR}/_deps/vcpkg_overlay-src/ports")
if (NOT VCPKG_TARGET_TRIPLET)
  if (WIN32)
    set (VCPKG_TARGET_TRIPLET "x64-windows")
  elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE CPU_ARCH)
    if (CPU_ARCH STREQUAL "x86_64")
      set(VCPKG_TARGET_TRIPLET "x64-linux")
    elseif (CPU_ARCH STREQUAL "aarch64")
      set(VCPKG_TARGET_TRIPLET "arm64-linux")
    else ()
      message(FATAL_ERROR "Unsupported CPU architecture: ${CPU_ARCH}")
    endif ()
  endif()
endif ()

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/_deps/vcpkg-src/scripts/buildsystems/vcpkg.cmake")
