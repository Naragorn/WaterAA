cmake_minimum_required(VERSION 3.20)
project(WaterAA CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "Oblivion is 32-bit: configure with a 32-bit toolchain (vcvars32.bat, -A Win32, or cmake/toolchain-linux-clang-cl.cmake).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static runtime: the plugin carries no dependency on an installed VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/W4 /permissive- /utf-8)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)

add_library(water_shaders STATIC src/WaterShaders.cpp)
target_include_directories(water_shaders PUBLIC src)

add_library(device_hook STATIC src/DeviceHook.cpp src/Setup.cpp)
target_include_directories(device_hook PUBLIC src)
target_link_libraries(device_hook PUBLIC water_shaders)
# No d3d9.lib: the plugin reaches the device only through its method table, so
# it never pulls a second d3d9.dll into the game.

add_library(plugin_entry STATIC src/PluginEntry.cpp)
target_include_directories(plugin_entry PUBLIC src)

add_library(WaterAA SHARED src/Plugin.cpp)
target_link_libraries(WaterAA PRIVATE plugin_entry device_hook)

enable_testing()

# The tests of the pure parts; tools/build-linux.sh also builds them for the
# host and runs them there.
foreach(test WaterShaders ShaderClasses PluginEntry)
	add_executable(${test}Test tests/${test}Test.cpp)
	target_include_directories(${test}Test PRIVATE src tests)
	add_test(NAME ${test}Test COMMAND ${test}Test)
endforeach()
target_link_libraries(WaterShadersTest PRIVATE water_shaders)
target_link_libraries(PluginEntryTest PRIVATE plugin_entry)

# The two device tests create a real Direct3D 9 device on this machine.
add_executable(DeviceHookTest tests/DeviceHookTest.cpp tests/TestDevice.cpp)
target_include_directories(DeviceHookTest PRIVATE tests)
target_link_libraries(DeviceHookTest PRIVATE device_hook d3d9)
add_test(NAME DeviceHookTest COMMAND DeviceHookTest)

add_executable(SetupTest tests/SetupTest.cpp tests/TestDevice.cpp)
target_include_directories(SetupTest PRIVATE tests)
target_link_libraries(SetupTest PRIVATE device_hook d3d9)
add_test(NAME SetupTest COMMAND SetupTest)

add_executable(ExportsTest tests/ExportsTest.cpp)
add_dependencies(ExportsTest WaterAA)
add_test(NAME ExportsTest COMMAND ExportsTest $<TARGET_FILE:WaterAA>)
