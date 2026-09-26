project(obs-ffmpeg)

if(MSVC)
	set(obs-ffmpeg_PLATFORM_DEPS
		w32-pthreads)
endif()

option(ENABLE_FFMPEG_LOGGING "Enables obs-ffmpeg logging" OFF)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
include_directories(${FFMPEG_INCLUDE_DIRS})

if(WIN32)
	# AMF SDK headers live in the vendored deps tree (AMF < 1.4.30 lacked
	# VideoEncoderAV1.h; OBS-PT carries the 1.4.30 header set)
	include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/deps2019/win64/include")
endif()

if(UNIX AND NOT APPLE)
        find_package(Libpci REQUIRED)
        include_directories(${LIBPCI_INCLUDE_DIRS})
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/obs-ffmpeg-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/obs-ffmpeg-config.h")
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(obs-ffmpeg_config_HEADERS
	"${CMAKE_CURRENT_BINARY_DIR}/obs-ffmpeg-config.h")

set(obs-ffmpeg_HEADERS
	obs-ffmpeg-compat.h
	obs-ffmpeg-formats.h
	obs-ffmpeg-mux.h)

set(obs-ffmpeg_SOURCES
	obs-ffmpeg.c
	obs-ffmpeg-audio-encoders.c
	obs-ffmpeg-nvenc.c
	obs-ffmpeg-av1.c
	obs-ffmpeg-output.c
	obs-ffmpeg-mux.c
	obs-ffmpeg-hls-mux.c
	obs-ffmpeg-source.c)

if(UNIX AND NOT APPLE)
	list(APPEND obs-ffmpeg_SOURCES
		obs-ffmpeg-vaapi.c)
	LIST(APPEND obs-ffmpeg_PLATFORM_DEPS
		${LIBVA_LBRARIES}
		${LIBPCI_LIBRARIES})
endif()

if(ENABLE_FFMPEG_LOGGING)
	list(APPEND obs-ffmpeg_SOURCES
		obs-ffmpeg-logging.c)
endif()

if(WIN32)
	set(MODULE_DESCRIPTION "OBS FFmpeg module")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in obs-ffmpeg.rc)
	list(APPEND obs-ffmpeg_SOURCES
		jim-nvenc.c
		jim-nvenc-helpers.c
		obspt-nvenc-model.c
		obspt-amf-model.c
		texture-amf.cpp
		obs-ffmpeg.rc)
	list(APPEND obs-ffmpeg_HEADERS
		jim-nvenc.h
		jim-nvenc-ver.h
		obspt-nvenc-model.h
		obspt-amf-model.h
		texture-amf-opts.hpp)
endif()

add_library(obs-ffmpeg MODULE
	${obs-ffmpeg_config_HEADERS}
	${obs-ffmpeg_HEADERS}
	${obs-ffmpeg_SOURCES})
if(WIN32)
	target_compile_definitions(obs-ffmpeg PRIVATE ENABLE_HEVC)
endif()
target_link_libraries(obs-ffmpeg
	libobs
	opts-parser
	media-playback
	${obs-ffmpeg_PLATFORM_DEPS}
	${FFMPEG_LIBRARIES})
set_target_properties(obs-ffmpeg PROPERTIES FOLDER "plugins/obs-ffmpeg")

install_obs_plugin_with_data(obs-ffmpeg data)

add_subdirectory(ffmpeg-mux)

if(WIN32)
	add_subdirectory(obs-nvenc-test)
	add_subdirectory(obs-amf-test)
endif()
