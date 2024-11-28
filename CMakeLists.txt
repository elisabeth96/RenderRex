cmake_minimum_required(VERSION 3.10)
project(RenderRex)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT EMSCRIPTEN)
	add_subdirectory(glfw)
else()
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)

# Create executable and ensure proper dependency ordering
add_executable(RenderRex main.cpp RenderRex.cpp RenderRex.h Drawable.h Drawable.cpp Renderer.h Renderer.cpp)
target_link_libraries(RenderRex PRIVATE glfw webgpu glfw3webgpu)

target_copy_webgpu_binaries(RenderRex)

if (MSVC)
	target_compile_options(RenderRex PRIVATE /W4)
else()
	target_compile_options(RenderRex PRIVATE -Wall -Wextra -pedantic)
endif()

if (MSVC)
	# Disable warning C4201: nonstandard extension used: nameless struct/union
	target_compile_options(RenderRex PUBLIC /wd4201)
endif (MSVC)

target_compile_definitions(RenderRex PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")