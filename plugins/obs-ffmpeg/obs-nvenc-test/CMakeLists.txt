project(obs-nvenc-test)

add_executable(obs-nvenc-test
	jim-nvenc-test.c)

target_include_directories(obs-nvenc-test PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(obs-nvenc-test
	d3d11
	dxgi
	dxguid)

set_target_properties(obs-nvenc-test PROPERTIES FOLDER "plugins/obs-ffmpeg")

install_obs_core(obs-nvenc-test)
