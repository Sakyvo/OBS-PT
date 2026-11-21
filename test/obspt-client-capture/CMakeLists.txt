add_executable(obspt-client-capture-test client-capture-test.cpp
  "${CMAKE_SOURCE_DIR}/plugins/win-capture/client-capture.cpp")
target_include_directories(obspt-client-capture-test PRIVATE
  "${CMAKE_SOURCE_DIR}/plugins/win-capture")
target_link_libraries(obspt-client-capture-test libobs d3d11 dxgi advapi32)
set_target_properties(obspt-client-capture-test PROPERTIES CXX_STANDARD 17
  FOLDER "tests and examples" OUTPUT_NAME OBS-PT)
add_executable(obspt-game-source-test game-source-test.cpp)
target_link_libraries(obspt-game-source-test libobs)
set_target_properties(obspt-game-source-test PROPERTIES CXX_STANDARD 17
  FOLDER "tests and examples" OUTPUT_NAME OBS-PT
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/source-test")
