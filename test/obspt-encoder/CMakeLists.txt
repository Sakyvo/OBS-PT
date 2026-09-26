project(obspt-encoder-test)

find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil)

add_executable(obspt-encoder-test
	obspt-encoder-test.cpp
	test-module-stub.c
	"${CMAKE_SOURCE_DIR}/plugins/obs-ffmpeg/obspt-nvenc-model.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-ffmpeg/obspt-amf-model.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/mp4-mux.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/flv-mux.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/amf.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/log.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/hashswf.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/parseurl.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/rtmp.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/cencode.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/md5.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/rtmp-hevc.c"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs/rtmp-av1.c"
	"${CMAKE_SOURCE_DIR}/libobs/obs-hevc.c"
	"${CMAKE_SOURCE_DIR}/libobs/obs-av1.c"
	"${CMAKE_SOURCE_DIR}/libobs/obs-nal.c")

target_include_directories(obspt-encoder-test SYSTEM PRIVATE
	"${CMAKE_SOURCE_DIR}/libobs"
	"${CMAKE_SOURCE_DIR}/plugins/obs-outputs"
	"${CMAKE_SOURCE_DIR}/plugins/obs-ffmpeg"
	${FFMPEG_INCLUDE_DIRS})

# flv-mux.c pulls librtmp headers; match the plugin's NO_CRYPTO build shape
target_compile_definitions(obspt-encoder-test PRIVATE NO_CRYPTO)

target_link_libraries(obspt-encoder-test
	libobs
	${FFMPEG_LIBRARIES}
	ws2_32
	winmm)

set_target_properties(obspt-encoder-test PROPERTIES FOLDER "tests and examples" CXX_STANDARD 20)

if(ENABLE_UNIT_TESTS)
	add_test(NAME obspt-encoder-test
		COMMAND $<TARGET_FILE:obspt-encoder-test>)
endif()

add_executable(obspt-module-smoke
	obspt-module-smoke.cpp)

target_include_directories(obspt-module-smoke SYSTEM PRIVATE
	"${CMAKE_SOURCE_DIR}/libobs")

target_link_libraries(obspt-module-smoke libobs)

set_target_properties(obspt-module-smoke PROPERTIES FOLDER "tests and examples" CXX_STANDARD 20)

# the smoke must run from rundir/<config>/bin next to the real DLLs
add_custom_command(TARGET obspt-module-smoke POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:obspt-module-smoke>"
		"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/$<TARGET_FILE_NAME:obspt-module-smoke>"
	VERBATIM)

add_executable(obspt-profile-switch-repro
	obspt-profile-switch-repro.cpp)

target_include_directories(obspt-profile-switch-repro SYSTEM PRIVATE
	"${CMAKE_SOURCE_DIR}/libobs")

target_link_libraries(obspt-profile-switch-repro libobs)

set_target_properties(obspt-profile-switch-repro PROPERTIES FOLDER "tests and examples" CXX_STANDARD 20)

add_custom_command(TARGET obspt-profile-switch-repro POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:obspt-profile-switch-repro>"
		"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/$<TARGET_FILE_NAME:obspt-profile-switch-repro>"
	VERBATIM)
