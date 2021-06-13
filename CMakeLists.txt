cmake_minimum_required(VERSION 3.11)
project(WorldOfPadman VERSION 1.6.1 LANGUAGES C ASM)

include(CheckSymbolExists)
include(CheckCCompilerFlag)

set(CODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/code)
set(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)

option(BUILD_CLIENT "" ON)
option(BUILD_SERVER "" ON)
option(BUILD_RENDERER_OPENGL2 "" ON)
option(BUILD_RENDERER_VULKAN "" ON)
option(USE_VOIP "" ON)
option(USE_RENDERER_DLOPEN "" ON)
option(USE_CURL_DLOPEN "" ON)
option(USE_OPENAL_DLOPEN "" ON)
option(USE_CURL "" ON)
option(USE_MUMBLE "" ON)

if (MSVC)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MANIFEST:NO")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFEST:NO")
	# 4244 conversion from 'float' to 'int', possible loss of data
	# 4305 truncation from 'double' to 'float'
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4267 /wd4244 /wd4305")
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	set(USE_CURL OFF)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

if (APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
	add_compile_definitions(MAC_OS_X_VERSION_MIN_REQUIRED=1070)
endif()

add_compile_definitions(PRODUCT_VERSION="${CMAKE_PROJECT_VERSION}")

macro(check_compiler_flag flag)
  string(REGEX REPLACE "[-=+]" "_" _flag ${flag})
  string(TOUPPER ${_flag} _flagfinal)
  check_c_compiler_flag("${flag}" COMPILER_SUPPORTS_${_flagfinal})
  if (COMPILER_SUPPORTS_${_flagfinal})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
  endif()
endmacro()

check_compiler_flag(-Wformat=2)
check_compiler_flag(-Wno-format-zero-length)
check_compiler_flag(-Wformat-security)
check_compiler_flag(-Wno-format-nonliteral)
check_compiler_flag(-Wstrict-aliasing=2)
check_compiler_flag(-Wmissing-format-attribute)
check_compiler_flag(-Wdisabled-optimization)
check_compiler_flag(-Werror-implicit-function-declaration)

add_subdirectory(libs)
add_subdirectory(code)