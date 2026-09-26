project(obspt-service-reload)

add_executable(obspt-service-reload
	obspt-service-reload.cpp)

target_include_directories(obspt-service-reload SYSTEM PRIVATE
	"${CMAKE_SOURCE_DIR}/libobs")

target_link_libraries(obspt-service-reload libobs)

set_target_properties(obspt-service-reload PROPERTIES FOLDER "tests and examples" CXX_STANDARD 20)

# must run from rundir/<config>/bin to find the real plugin DLLs and data
add_custom_command(TARGET obspt-service-reload POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:obspt-service-reload>"
		"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/$<TARGET_FILE_NAME:obspt-service-reload>"
	VERBATIM)
