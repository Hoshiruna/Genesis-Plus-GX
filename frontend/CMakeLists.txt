cmake_minimum_required(VERSION 3.24)

project(genplus_frontend LANGUAGES C)

include(FetchContent)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(GENPLUS_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(GENPLUS_CORE_DIR "${GENPLUS_REPO_ROOT}/core")
set(GENPLUS_SDL_DIR "${GENPLUS_REPO_ROOT}/sdl")
set(GENPLUS_CHD_DIR "${GENPLUS_CORE_DIR}/cd_hw/libchdr")
set(GENPLUS_ZLIB_DIR "${GENPLUS_CHD_DIR}/deps/zlib-1.3.1")
set(GENPLUS_LZMA_DIR "${GENPLUS_CHD_DIR}/deps/lzma-24.05")
set(GENPLUS_ZSTD_DIR "${GENPLUS_CHD_DIR}/deps/zstd-1.5.6")

option(GENPLUS_FRONTEND_FETCH_SDL3 "Fetch the official SDL3 VC development package when SDL3 is not found" ON)
set(GENPLUS_FRONTEND_SDL3_VERSION "3.4.12" CACHE STRING "SDL3 VC package version used by FetchContent")

function(genplus_select_sdl3_target out_target)
  find_package(SDL3 CONFIG QUIET)

  if(TARGET SDL3::SDL3)
    set(${out_target} SDL3::SDL3 PARENT_SCOPE)
    return()
  endif()

  if(TARGET SDL3::SDL3-shared)
    set(${out_target} SDL3::SDL3-shared PARENT_SCOPE)
    return()
  endif()

  if(NOT GENPLUS_FRONTEND_FETCH_SDL3)
    message(FATAL_ERROR "SDL3 was not found. Set GENPLUS_FRONTEND_FETCH_SDL3=ON or provide SDL3 through CMAKE_PREFIX_PATH/vcpkg.")
  endif()

  if(NOT MSVC OR NOT WIN32)
    message(FATAL_ERROR "Automatic SDL3 fetching currently uses the official Windows VC package. Install SDL3 and provide it through find_package(SDL3) on this platform.")
  endif()

  FetchContent_Declare(
    sdl3_vc
    URL "https://www.libsdl.org/release/SDL3-devel-${GENPLUS_FRONTEND_SDL3_VERSION}-VC.zip"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  FetchContent_GetProperties(sdl3_vc)
  if(NOT sdl3_vc_POPULATED)
    FetchContent_Populate(sdl3_vc)
  endif()

  set(sdl3_root "")
  set(sdl3_candidates
    "${sdl3_vc_SOURCE_DIR}"
    "${sdl3_vc_SOURCE_DIR}/SDL3-${GENPLUS_FRONTEND_SDL3_VERSION}"
    "${sdl3_vc_SOURCE_DIR}/SDL3-devel-${GENPLUS_FRONTEND_SDL3_VERSION}-VC"
  )

  file(GLOB sdl3_child_dirs LIST_DIRECTORIES true "${sdl3_vc_SOURCE_DIR}/*")
  list(APPEND sdl3_candidates ${sdl3_child_dirs})

  foreach(candidate IN LISTS sdl3_candidates)
    if(EXISTS "${candidate}/include/SDL3/SDL.h")
      set(sdl3_root "${candidate}")
      break()
    endif()
  endforeach()

  if(NOT sdl3_root)
    message(FATAL_ERROR "Fetched SDL3, but could not locate include/SDL3/SDL.h in ${sdl3_vc_SOURCE_DIR}")
  endif()

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(sdl3_arch x64)
  else()
    set(sdl3_arch x86)
  endif()

  set(sdl3_implib "${sdl3_root}/lib/${sdl3_arch}/SDL3.lib")
  set(sdl3_runtime "${sdl3_root}/lib/${sdl3_arch}/SDL3.dll")

  if(NOT EXISTS "${sdl3_implib}" OR NOT EXISTS "${sdl3_runtime}")
    message(FATAL_ERROR "Fetched SDL3, but could not locate ${sdl3_implib} and ${sdl3_runtime}")
  endif()

  add_library(SDL3::SDL3 SHARED IMPORTED GLOBAL)
  set_target_properties(SDL3::SDL3 PROPERTIES
    IMPORTED_IMPLIB "${sdl3_implib}"
    IMPORTED_LOCATION "${sdl3_runtime}"
    INTERFACE_INCLUDE_DIRECTORIES "${sdl3_root}/include"
  )

  set(${out_target} SDL3::SDL3 PARENT_SCOPE)
endfunction()

genplus_select_sdl3_target(GENPLUS_SDL3_TARGET)

file(GLOB GENPLUS_CORE_SOURCES CONFIGURE_DEPENDS
  "${GENPLUS_CORE_DIR}/*.c"
  "${GENPLUS_CORE_DIR}/m68k/*.c"
  "${GENPLUS_CORE_DIR}/z80/*.c"
  "${GENPLUS_CORE_DIR}/sound/*.c"
  "${GENPLUS_CORE_DIR}/sound/tremor/*.c"
  "${GENPLUS_CORE_DIR}/input_hw/*.c"
  "${GENPLUS_CORE_DIR}/cart_hw/*.c"
  "${GENPLUS_CORE_DIR}/cart_hw/svp/*.c"
  "${GENPLUS_CORE_DIR}/cd_hw/*.c"
  "${GENPLUS_CORE_DIR}/debug/*.c"
  "${GENPLUS_CORE_DIR}/ntsc/*.c"
  "${GENPLUS_CHD_DIR}/src/*.c"
)

set(GENPLUS_LZMA_SOURCES
  "${GENPLUS_LZMA_DIR}/src/CpuArch.c"
  "${GENPLUS_LZMA_DIR}/src/LzFind.c"
  "${GENPLUS_LZMA_DIR}/src/LzmaDec.c"
  "${GENPLUS_LZMA_DIR}/src/LzmaEnc.c"
)

set(GENPLUS_ZSTD_SOURCES
  "${GENPLUS_ZSTD_DIR}/lib/common/entropy_common.c"
  "${GENPLUS_ZSTD_DIR}/lib/common/error_private.c"
  "${GENPLUS_ZSTD_DIR}/lib/common/fse_decompress.c"
  "${GENPLUS_ZSTD_DIR}/lib/common/xxhash.c"
  "${GENPLUS_ZSTD_DIR}/lib/common/zstd_common.c"
  "${GENPLUS_ZSTD_DIR}/lib/decompress/huf_decompress.c"
  "${GENPLUS_ZSTD_DIR}/lib/decompress/zstd_ddict.c"
  "${GENPLUS_ZSTD_DIR}/lib/decompress/zstd_decompress.c"
  "${GENPLUS_ZSTD_DIR}/lib/decompress/zstd_decompress_block.c"
)

set(GENPLUS_ZLIB_SOURCES
  "${GENPLUS_ZLIB_DIR}/adler32.c"
  "${GENPLUS_ZLIB_DIR}/crc32.c"
  "${GENPLUS_ZLIB_DIR}/deflate.c"
  "${GENPLUS_ZLIB_DIR}/gzclose.c"
  "${GENPLUS_ZLIB_DIR}/gzlib.c"
  "${GENPLUS_ZLIB_DIR}/gzread.c"
  "${GENPLUS_ZLIB_DIR}/gzwrite.c"
  "${GENPLUS_ZLIB_DIR}/inffast.c"
  "${GENPLUS_ZLIB_DIR}/inflate.c"
  "${GENPLUS_ZLIB_DIR}/inftrees.c"
  "${GENPLUS_ZLIB_DIR}/trees.c"
  "${GENPLUS_ZLIB_DIR}/zutil.c"
)

set(GENPLUS_FRONTEND_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/frontend_host.h"
  "${CMAKE_CURRENT_LIST_DIR}/emu_runner.h"
  "${CMAKE_CURRENT_LIST_DIR}/emu_runner.c"
  "${CMAKE_CURRENT_LIST_DIR}/main.h"
  "${CMAKE_CURRENT_LIST_DIR}/osd.h"
  "${CMAKE_CURRENT_LIST_DIR}/sdl3_frontend.c"
)

add_library(genplus_zlib STATIC ${GENPLUS_ZLIB_SOURCES})
target_include_directories(genplus_zlib PUBLIC "${GENPLUS_ZLIB_DIR}")
target_compile_definitions(genplus_zlib PRIVATE _CRT_SECURE_NO_WARNINGS)

add_library(genplus_core_static STATIC
  "${CMAKE_CURRENT_LIST_DIR}/emu_runner.c"
  "${CMAKE_CURRENT_LIST_DIR}/frontend_host.h"
  "${CMAKE_CURRENT_LIST_DIR}/emu_runner.h"
  "${CMAKE_CURRENT_LIST_DIR}/main.h"
  "${CMAKE_CURRENT_LIST_DIR}/osd.h"
  "${GENPLUS_SDL_DIR}/config.c"
  "${GENPLUS_SDL_DIR}/error.c"
  "${GENPLUS_SDL_DIR}/fileio.c"
  "${GENPLUS_SDL_DIR}/unzip.c"
  ${GENPLUS_CORE_SOURCES}
  ${GENPLUS_LZMA_SOURCES}
  ${GENPLUS_ZSTD_SOURCES}
)

target_include_directories(genplus_core_static PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${GENPLUS_CORE_DIR}"
  "${GENPLUS_CORE_DIR}/m68k"
  "${GENPLUS_CORE_DIR}/z80"
  "${GENPLUS_CORE_DIR}/input_hw"
  "${GENPLUS_CORE_DIR}/sound"
  "${GENPLUS_CORE_DIR}/sound/minimp3"
  "${GENPLUS_CORE_DIR}/sound/tremor"
  "${GENPLUS_CORE_DIR}/cart_hw"
  "${GENPLUS_CORE_DIR}/cart_hw/svp"
  "${GENPLUS_CORE_DIR}/cd_hw"
  "${GENPLUS_CORE_DIR}/debug"
  "${GENPLUS_CORE_DIR}/ntsc"
  "${GENPLUS_SDL_DIR}"
  "${GENPLUS_CHD_DIR}/include"
  "${GENPLUS_LZMA_DIR}/include"
  "${GENPLUS_ZSTD_DIR}/lib"
  "${GENPLUS_ZSTD_DIR}/lib/common"
  "${GENPLUS_ZSTD_DIR}/lib/decompress"
  "${GENPLUS_ZLIB_DIR}"
)

target_compile_definitions(genplus_core_static PRIVATE
  LSB_FIRST
  USE_16BPP_RENDERING
  USE_LIBTREMOR
  USE_LIBCHDR
  MAXROMSIZE=33554432
  HAVE_YM3438_CORE
  HAVE_OPLL_CORE
  ENABLE_SUB_68K_ADDRESS_ERROR_EXCEPTIONS
  Z7_ST
  ZSTD_DISABLE_ASM
  "INLINE=static inline"
  _CRT_SECURE_NO_WARNINGS
)

if(NOT WIN32)
  target_compile_definitions(genplus_core_static PRIVATE HAVE_ALLOCA_H)
endif()

target_link_libraries(genplus_core_static PUBLIC genplus_zlib)

add_executable(genplus_frontend
  "${CMAKE_CURRENT_LIST_DIR}/sdl3_frontend.c"
  "${CMAKE_CURRENT_LIST_DIR}/frontend_host.h"
  "${CMAKE_CURRENT_LIST_DIR}/emu_runner.h"
)
target_include_directories(genplus_frontend PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(genplus_frontend PRIVATE genplus_core_static ${GENPLUS_SDL3_TARGET})

if(WIN32)
  target_link_libraries(genplus_frontend PRIVATE user32 comdlg32)

  add_custom_command(TARGET genplus_frontend POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${GENPLUS_SDL3_TARGET}>"
            "$<TARGET_FILE_DIR:genplus_frontend>"
  )
endif()

if(MSVC)
  target_compile_definitions(genplus_frontend PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

source_group(TREE "${GENPLUS_REPO_ROOT}" FILES
  ${GENPLUS_CORE_SOURCES}
  ${GENPLUS_LZMA_SOURCES}
  ${GENPLUS_ZSTD_SOURCES}
  ${GENPLUS_ZLIB_SOURCES}
  "${GENPLUS_SDL_DIR}/config.c"
  "${GENPLUS_SDL_DIR}/error.c"
  "${GENPLUS_SDL_DIR}/fileio.c"
  "${GENPLUS_SDL_DIR}/unzip.c"
)

source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES
  ${GENPLUS_FRONTEND_SOURCES}
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT genplus_frontend)
