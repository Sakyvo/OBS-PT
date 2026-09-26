project(obs-amf-test)

add_executable(obs-amf-test
	obs-amf-test.cpp)

target_include_directories(obs-amf-test SYSTEM PRIVATE
	"${CMAKE_SOURCE_DIR}/libobs"
	"${CMAKE_SOURCE_DIR}/deps2019/win64/include")

target_link_libraries(obs-amf-test
	d3d11
	dxgi
	dxguid)

set_target_properties(obs-amf-test PROPERTIES FOLDER "plugins/obs-ffmpeg")

install_obs_core(obs-amf-test)

# must sit next to OBS-PT.exe in the runnable tree (obs-ffmpeg locates it
# via os_get_executable_path_ptr)
add_custom_command(TARGET obs-amf-test POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:obs-amf-test>"
		"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/$<TARGET_FILE_NAME:obs-amf-test>"
	VERBATIM)
