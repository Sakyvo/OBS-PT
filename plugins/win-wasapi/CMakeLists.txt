project(win-wasapi)

set(win-wasapi_HEADERS
	enum-wasapi.hpp)

set(MODULE_DESCRIPTION "OBS WASAPI module")
configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in win-wasapi.rc)
set(win-wasapi_SOURCES
	win-wasapi.cpp
	executable-audio-session-model.cpp
	enum-wasapi.cpp
	plugin-main.cpp
	win-wasapi.rc)

add_library(win-wasapi MODULE
	${win-wasapi_SOURCES}
	${win-wasapi_HEADERS})
target_link_libraries(win-wasapi
	Avrt
	libobs)
set_target_properties(win-wasapi PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(win-wasapi data)

if(BUILD_TESTS)
	add_executable(obspt-executable-audio-session-model-test
		executable-audio-session-model-test.cpp
		executable-audio-session-model.cpp)
	set_target_properties(obspt-executable-audio-session-model-test PROPERTIES
		FOLDER "tests and examples")
endif()
