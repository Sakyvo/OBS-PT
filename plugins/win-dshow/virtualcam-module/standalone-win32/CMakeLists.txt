cmake_minimum_required(VERSION 3.5)

project(obs-virtualcam-module-standalone)

# Standalone Win32 build lane of obs-virtualcam-module. The main OBS-PT
# tree only produces the 64-bit filter DLL (the main solution is x64);
# 32-bit DirectShow consumers need obs-virtualcam-module32.dll, which is
# built here in a separate Win32-configured tree, e.g.:
#
#   cmake -S plugins/win-dshow/virtualcam-module/standalone-win32 \
#         -B .target/-vcam32 -A Win32 \
#         -G "Visual Studio 17 2022" \
#         -D "VIRTUALCAM_GUID:STRING=9B78EFCA-8277-4A19-B27A-0E8EE418D6D5"
#   cmake --build .target/-vcam32 --config RelWithDebInfo --parallel
#   (copy the resulting obs-virtualcam-module32.dll into
#    .target/_pkg/OBS-PT/data/obs-plugins/win-dshow before packaging)

get_filename_component(OBSPT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)

set(VIRTUALCAM_MODULE_DIR "${OBSPT_ROOT}/plugins/win-dshow/virtualcam-module")
set(WIN_DSHOW_DIR "${OBSPT_ROOT}/plugins/win-dshow")
set(LIBDSHOWCAPTURE_SRC "${WIN_DSHOW_DIR}/libdshowcapture/source")

set(VIRTUALCAM_GUID "" CACHE STRING "VirtualCam GUID")
if(VIRTUALCAM_GUID STREQUAL "")
	message(FATAL_ERROR "VIRTUALCAM_GUID not set! OBS-PT uses 9B78EFCA-8277-4A19-B27A-0E8EE418D6D5.")
endif()

# Same GUID parsing logic as plugins/win-dshow/CMakeLists.txt
string(REPLACE "-" ";" GUID_VALS ${VIRTUALCAM_GUID})
list(LENGTH GUID_VALS GUID_VAL_COUNT)
if(NOT GUID_VAL_COUNT EQUAL 5)
	message(FATAL_ERROR "Invalid VIRTUALCAM_GUID ${VIRTUALCAM_GUID}")
endif()
list(GET GUID_VALS 0 GUID_VALS_DATA1)
list(GET GUID_VALS 1 GUID_VALS_DATA2)
list(GET GUID_VALS 2 GUID_VALS_DATA3)
list(GET GUID_VALS 3 GUID_VALS_DATA4)
list(GET GUID_VALS 4 GUID_VALS_DATA5)
set(GUID_VAL01 ${GUID_VALS_DATA1})
set(GUID_VAL02 ${GUID_VALS_DATA2})
set(GUID_VAL03 ${GUID_VALS_DATA3})
string(SUBSTRING ${GUID_VALS_DATA4} 0 2 GUID_VAL04)
string(SUBSTRING ${GUID_VALS_DATA4} 2 2 GUID_VAL05)
string(SUBSTRING ${GUID_VALS_DATA5} 0 2 GUID_VAL06)
string(SUBSTRING ${GUID_VALS_DATA5} 2 2 GUID_VAL07)
string(SUBSTRING ${GUID_VALS_DATA5} 4 2 GUID_VAL08)
string(SUBSTRING ${GUID_VALS_DATA5} 6 2 GUID_VAL09)
string(SUBSTRING ${GUID_VALS_DATA5} 8 2 GUID_VAL10)
string(SUBSTRING ${GUID_VALS_DATA5} 10 2 GUID_VAL11)

configure_file("${WIN_DSHOW_DIR}/virtualcam-guid.h.in" "${CMAKE_CURRENT_BINARY_DIR}/virtualcam-guid.h")

configure_file(
	"${VIRTUALCAM_MODULE_DIR}/virtualcam-module.def.in"
	"${CMAKE_CURRENT_BINARY_DIR}/virtualcam-module.def")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_output_suffix "64")
else()
	set(_output_suffix "32")
endif()

if(MSVC)
	add_compile_options("$<IF:$<CONFIG:Debug>,/MTd,/MT>")
endif()

add_definitions(-DVIRTUALCAM_ENABLED -D_USE_MATH_DEFINES -DUNICODE -D_UNICODE)

add_library(obs-virtualcam-module MODULE
	"${CMAKE_CURRENT_BINARY_DIR}/virtualcam-module.def"
	"${VIRTUALCAM_MODULE_DIR}/sleepto.c"
	"${VIRTUALCAM_MODULE_DIR}/sleepto.h"
	"${VIRTUALCAM_MODULE_DIR}/placeholder.cpp"
	"${VIRTUALCAM_MODULE_DIR}/virtualcam-module.cpp"
	"${VIRTUALCAM_MODULE_DIR}/virtualcam-filter.cpp"
	"${VIRTUALCAM_MODULE_DIR}/virtualcam-filter.hpp"
	"${WIN_DSHOW_DIR}/shared-memory-queue.c"
	"${WIN_DSHOW_DIR}/shared-memory-queue.h"
	"${WIN_DSHOW_DIR}/tiny-nv12-scale.c"
	"${WIN_DSHOW_DIR}/tiny-nv12-scale.h"
	"${LIBDSHOWCAPTURE_SRC}/log.cpp"
	"${LIBDSHOWCAPTURE_SRC}/dshow-base.cpp"
	"${LIBDSHOWCAPTURE_SRC}/dshow-enum.cpp"
	"${LIBDSHOWCAPTURE_SRC}/dshow-formats.cpp"
	"${LIBDSHOWCAPTURE_SRC}/dshow-media-type.cpp"
	"${LIBDSHOWCAPTURE_SRC}/output-filter.cpp")

target_include_directories(obs-virtualcam-module PRIVATE
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${WIN_DSHOW_DIR}"
	"${OBSPT_ROOT}/libobs/util")

target_link_libraries(obs-virtualcam-module
	setupapi
	winmm
	strmiids
	gdiplus)

set_target_properties(obs-virtualcam-module PROPERTIES
	OUTPUT_NAME "obs-virtualcam-module${_output_suffix}"
	PREFIX "")
