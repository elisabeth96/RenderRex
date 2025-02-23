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

set(CMAKE_PREFIX_PATH "/Users/elisabeth/projects/dawn/install/Release")
find_package(Dawn REQUIRED)

add_library(webgpu INTERFACE)
target_link_libraries(webgpu INTERFACE dawn::webgpu_dawn)

add_subdirectory(glfw3webgpu)
add_subdirectory(imgui)

# Create executable and ensure proper dependency ordering
add_executable(RenderRex
		main.cpp
		RenderRex.h
		RenderRex.cpp
		Drawable.h
		Renderer.h
		Renderer.cpp
		Camera.h
		Camera.cpp
		Primitives.h
		Primitives.cpp
		Mesh.h
		Mesh.cpp
		VisualMesh.h
		VisualMesh.cpp
		InstancedMesh.h
		InstancedMesh.cpp
		MeshIO.h
		MeshIO.cpp
		Property.h
		Property.cpp
)

target_link_libraries(RenderRex PRIVATE 
	glfw 
	glfw3webgpu 
	imgui 
	webgpu)

#enable asan
#target_compile_options(RenderRex PRIVATE -fsanitize=address)
#target_link_options(RenderRex PRIVATE -fsanitize=address)

#enable udefined behavior sanitizer
#target_compile_options(RenderRex PRIVATE -fsanitize=undefined)
#target_link_options(RenderRex PRIVATE -fsanitize=undefined)

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

